# N64 player-shadow trace

Private reference: root `Spider-Man (USA).z64`, big-endian USA revision 0,
SHA-256 `feff90ed1201c91ff167d66958048e61c192c9d6a756ddb98f799017ac9cd25c`.
The ROM and extracted code stay in ignored local storage.

## What the ROM does

The silhouette is **not cast from a point light or a fixed diagonal light**.
The game flattens the already-posed player mesh, rotates that flattened shape to
the receiving plane, and draws it black at alpha 128. On a horizontal floor,
each world vertex `(x,y,z)` becomes `(x,groundHeight,z)`. Changing Spider-Man's
height or attachment orientation does not change the ground plane. His pose
still changes the silhouette naturally.

The trace used actual executable instructions, not the generic `SHADOW` texture
or the older blob-shadow function also present in the ROM:

| N64 RAM address | Role |
| --- | --- |
| `80000870` | Loader: reads the 16-block ERZ table at ROM `13B34` |
| `80000C50` | ERZ v2 decompression wrapper, executed to unpack the main program |
| `80016AE0` | Start of unpacked main program |
| `80084EAC–80084F70` | Ground physics probe and cached receiving-surface record |
| `8003632C–80036444` | Selects the cached collision plane for rendering |
| `80036448–80036728` | Player draw setup, visibility checks and shadow-mode selection |
| `800378B0–80037E1C` | Player silhouette renderer |
| `800BD020` | Translation matrix initialization |
| `800BDF18` | Basis construction from the negated collision normal |
| `800BDDB8` | Scale the matrix basis by `(1,0,1)` |
| `800BDD5C` | Translate by the negative receiver anchor |
| `800BCF04` | Compose with the camera matrix |
| `80047F64` | Redraw the player's posed model under that projection |
| `80081C4C` | Generic body blob shadow; this is **not** the player silhouette renderer |

The normal ground probe starts 40 world units above the player and ends 140
units below that start, extended by the downward movement step. Its successful
collision record is copied to player `+121C`; validity is `+123C`. A probe miss
clears validity. Updates which do not perform this probe retain the cached
record, including attached movement. Rendering reads its height at record
`+14` and normal at `+4`, copying them to player `+260` and `+264`.

At `8003632C`, level `0x42` has an extra world-down probe of 8000 units except
in substate `0xE`. A missing receiver, or its material with flag bit 0 set at `+60`,
suppresses drawing. The draw caller also has level/state suppression checks,
including player state `0x400`. Mode selection accounts for camera distance,
camera-to-ground occlusion and player state. Those platform-specific visibility
branches must be distinguished from the common projection math.

The receiver branch was also executed directly: an invalid record returns 0;
a valid record without a material returns 2; a material with flags 0
returns 1; flags 1 returns 0. These are actual MIPS results, including the branch
delay slot, in `.local/n64-shadow/receiver-oracle.json`.

The draw caller's story-level indices refer to the descriptor table at `800E4E50`,
not the separate filename lookup table at `800E4B98`. Thus `0x13` is `l6a1_t`,
`0x15` is `l6a3_t`, and `0x18` is `l7a2_t`. The first two suppress the silhouette
when player `+CF0` is nonzero, unless substate is `0xE`. That substate selects
the demo descriptor table at `800E4D60` (`dem1_t`, `dem3_t`, `dem4_t`). `0x42`
is training level **`lha1_t` (Zip-line Training)**. The loader at `800329AC–800329E8` selects the
17-entry training table at `800E4D84` and subtracts **50** from the level index;
66 therefore selects its final entry. Both tables have 12-byte records.
Global `800E41A0` bit 2 also suppresses the shadow: the trigger
interpreter sets it for level 2/script `0xEA` at `800A6998` and clears it at
`800A69E0`. The PS1 trigger files contain the same script IDs and opcodes.

At `8003792C`, the anchor is `(player.x,receiver.height,player.z)/16` in N64
renderer units. Let `up=-normal`, `forward=normalize(X cross up)` and
`right=normalize(up cross forward)`, with zero preserved for degenerate crosses.
The matrix transforms a point `v` to:

```
anchor + (v.x-anchor.x)*right + (v.z-anchor.z)*forward
```

This is not an orthogonal projection along the normal on sloped ground; it
rotates the flattened X/Z footprint. Replacing it with a conventional light
projection would change the original behavior.

The renderer emits primitive black/alpha `00000080`, primitive-color combiner
`FCFFFFFF FFFDF638`, and two RDP depth/blend paths. Mode 1 uses `0C184A50` with
pixel depth. Mode 0 uses primitive depth zero (`EE000000 00000000`), mode
`0C184978`, then a box pass with mode `0C184260` before restoring camera state.
The host renderer expresses silhouette coverage with stencil and uses the
projected receiver's scene depth; it does not emulate those RDP command sequences.

## Port implementation

`N64ShadowProjection.Create` mirrors the matrix construction. The PS1 ground
probe returning at `80036390` is the matching physics call; the hook captures
its fixed-point height and normal before the original code consumes them.
Level loading clears the cache. A missed probe invalidates it. The `lha1_t`
training exception performs the ROM's 8000-unit world-down query through the
native PS1 collision engine. Other levels retain the cached native ground probe.

`PlayerShadow` captures the current player's posed meshes, including custom
costume joint vertices, per actor draw rather than per vblank. It normalizes
the near transform's 16x coordinates, recovers world coordinates, applies the
N64 matrix, and projects through the current camera. Near-plane crossings are
clipped. Player blob removal uses the native owner pointer, including both the
generic body blob and the player's separate `+BB0`/`+BB4` quad lifecycle, so
NPC/pickup blobs cannot be mistaken for the player's silhouette.

Projected triangles use the custom ordering-table queue before OT0 packets.
An eight-byte native NOP packet binds that queue to the actual traversal chain:
the original game's OT optimizer otherwise removes the empty OT0 insertion
point. A fully clipped pose allocates no packet. Unrelated immediate/font
ordering tables cannot discard the pending world shadow.
Their real scene depth is tested without writing depth, with a coplanar raster
bias. Stencil limits darkening to once per pixel. Native menu/HUD packets retain
their painter ordering. `SPIDEY_SHADOW=off` retains the original PS1 blob;
`debug` draws the captured pose in magenta. The old `SPIDEY_SHADOW_LIGHT` estimate
has been removed.

The PS1 scene renderer replaces the N64 RDP visibility mechanics. The remaining
visibility implementation is in `N64ShadowVisibility` and `PlayerShadow`:

- Player state N64 `+1118` maps to **PS1 `+F3C`**, established by the matching
  physics loads at `800842A4` / `80035864`. The former port's `F44` binding was
  wrong. Attachment N64 `+CF0` maps to **PS1 `+BC4`** (`80084238` / `800357F4`).
- Tunnel Crawl and Lizard's Maze suppress the silhouette while attached. Bank
  wall/ceiling attachment does not. Demo filenames use the demo policy.
- Script opcode C3 sets suppression in `l1a3_t` script EA and clears it when
  the cinematic value becomes zero. Bank script 16E retains the previous N64
  cinematic value. C7 in `l7a3_t` script 15D clears the flag. Level reset also
  clears it. Hooks at PS1 `8005C984` and `8005D06C` retain original game logic.
- The collision record's associated object is a **material**, not a moving
  actor. `800550B0–80055140` obtains it from the hit mesh's material header;
  `800D4D90` uses its bit 0 to choose translucent RDP modes. The PS1 counterpart
  is the hit face at query `+80`: face bit 0 selects textured rendering
  (`8007C7A0`), and bit 6 becomes GP0 semi-transparency (`8007F760`). Textured
  translucent receivers suppress the silhouette; no-material receivers retain
  mode 1, exactly as the N64 selector's return value 2 does.
- Mode 1 applies to non-demo levels 24/66, state mask `00800480`, distance
  strictly greater than 1500 units, or a blocked camera-to-ground ray. Mode 0
  hides the shadow when camera forward Y is strictly above 0.5 and the camera
  is below the receiver. The camera ray ends ten world units above the cached
  receiver. The port queries it only when this early-out can affect visibility;
  both remaining draw paths use scene-depth stencil. Camera forward Y comes
  from the transposed native view matrix, element 7. N64 `80025370` and
  `80059DDC` establish the same matrix transpose and 1/16 translation conversion.

The N64 `80037A20` guard checks all four translation-row components of the
**projection × camera × perspective** matrix against [-32700,32700] before
`guMtxF2L`. Sixteen executed boundary cases cover every component and both signs.
This is a signed 16.16 RSP matrix-format limit. The host uses float geometry and
does not encode that matrix, so copying the limit onto an unprojected host view
matrix would reject different scenes. It is deliberately not a host visibility
rule. Native near clipping and scene-depth testing still apply. Likewise, the
RDP primitive-depth/box-restoration passes are replaced by the host depth/stencil
pipeline rather than issuing N64 commands to OpenGL.

The complete applicable visibility implementation was verified and deployed on
November 21; the evidence and explicit validation limits are recorded below.

## Reproducible evidence

`tools/trace_n64_shadows.py` executes the ROM's decompressor and its actual
shadow matrix routine through `80037A20` under Unicorn, with an identity camera.
It writes private unpacked RAM and six reference matrices beneath
`.local/n64-shadow`. The committed numeric fixture contains no ROM code/assets.

`tools/RecompOne/tests/ShadowRegression` compares transformed vertices against
those outputs and exercises native receiver capture, invalidation, ownership,
near clipping and the actual hidden OpenGL depth/stencil path.

Validation on November 17, 2026:

- ROM replay regenerated all six checked-in reference matrices exactly. Four
  additional executions establish the receiver-selector return values above.
- **48 ShadowRegression checks pass**, including ROM-derived matrix outputs,
  ceiling height, near clipping, receiver cache invalidation/ownership, both PS1
  blob paths, native compacted OT traversal, and actual hidden OpenGL pixels for
  depth, stencil, coplanarity and HUD layering.
- Native hidden/silent bank runs use the normal title/Continue flow with an
  isolated copy of slot 1. The process-local script walks, climbs and traverses
  the ceiling; camera-angle pokes affect only that process. No host input is
  generated. Memory-card contents remain unchanged.
- Inspected all captures from `floor-bound`, `climb-bound`, `deposit-ceiling`
  and `deposit-upper` individually. The entrance ceiling captures are obscured
  by the level's bright light effect and are not ceiling-shadow proof. The later
  open-bank capture at frame 4190 visibly shows the full posed silhouette on the
  floor beneath the ceiling-attached player; frame 4450 shows partial occlusion
  by the intervening bank geometry. Frame 3700 from `deposit-ceiling` shows the
  receiver moving onto the raised ledge.

These pre-release captures used executable SHA-256
`7df1af4f16d8c0e6fe6a9e46adebb9f34e7479fede102a8d9bfcd48e9689c8e1`.
Final release SHA-256:
`0f3450d2f6b98aada16eb5e7cd5773b67e87d4e262987440d2cc5a1d2e5ad3e6`.
The release build includes the same projection and adds the tested empty-pose
packet guard. Its custom-suit bank run completed normally in 96.28 seconds;
all six captures were inspected in order: floor, lower wall climb, raised ledge,
upper wall climb, ceiling, and ceiling traversal with foreground occlusion.
The floor receiver is outside the view during the upper-wall shot, and correctly
does not paint onto the wall. The ceiling shot shows it on the floor below.

The existing **35 SceneDepthRegression** and **34 ContinueRegression** checks
also pass against the updated code. Audio was deliberately muted throughout;
these runs do not claim an audible-audio check.

Private native captures:
[floor](../.local/n64-shadow/floor-bound/frame_01200.png),
[raised ledge](../.local/n64-shadow/deposit-ceiling/frame_03700.png),
[ceiling](../.local/n64-shadow/deposit-upper/frame_04190.png),
[ceiling with foreground occlusion](../.local/n64-shadow/deposit-upper/frame_04450.png).

Final release captures:
[floor](../.local/n64-shadow/final-custom/frame_01200.png),
[lower wall](../.local/n64-shadow/final-custom/frame_03400.png),
[ledge](../.local/n64-shadow/final-custom/frame_03700.png),
[upper wall](../.local/n64-shadow/final-custom/frame_04060.png),
[ceiling](../.local/n64-shadow/final-custom/frame_04190.png),
[ceiling traversal](../.local/n64-shadow/final-custom/frame_04450.png).
[Run log and executable/card verification](../.local/n64-shadow/final-custom/result.json).

The built-in suit was checked in `stock-natural` with no memory overrides;
that run exited normally after 57.48 seconds, with all four captures inspected
in sequence. Its floor silhouette is visible and correct. The ceiling receiver
is obscured by the entrance light, and later wall views exclude the floor;
those captures do not establish visible ceiling-shadow correctness for that
suit. The earlier `final-stock` run is excluded from climb/ceiling acceptance:
its copied fixed camera-poke address belonged to the custom-suit allocation,
whereas the stock camera is at `80118010` (yaw `+236`). This harness issue does
not affect production: `PlayerShadow` reads the live camera pointer.
[Built-in floor proof](../.local/n64-shadow/stock-natural/frame_01200.png),
[isolated run record](../.local/n64-shadow/stock-natural/result.json).

Deployed the validated release to `Spider-Man/SpiderMan.exe` on November 17.
Before/after SHA-256 manifests confirm that **all 1,807 other staged files are
unchanged**, including settings, cards, suit selection and assets. The existing
verified clipping build is retained as rollback; no additional backup EXE was
duplicated. Two superseded test executables were removed (219,884,854 bytes).
[Deployment and rollback record](../.local/n64-shadow/deployment.json).

## Visibility validation — November 21, 2026

`tools/trace_n64_shadow_visibility.py` executes the supplied ROM's actual draw
caller, receiver selector, renderer early-out and C3/C7 script handlers. It stubs
external collision answers and stops before RDP submission; it does not replace
the visibility instructions with a hand-written oracle. The outputs cover 29
draw paths, eight script transitions, 16 fixed-matrix range boundaries and all
51 story/training names. Re-execution exactly matches the committed numeric
fixture `tools/RecompOne/tests/ShadowRegression/n64-visibility.json`.

The same tool checks the actual N64/PS1 load instructions and opcode tables used
for the port bindings, and the EA/16E/15D script headers in the private PS1 TRG
files. Its private binding report is `.local/n64-shadow/ps1-bindings.json`.
The material, camera, demo, training and hardware-format branches described above
are resolved; none is being deferred as an unexplained visibility exception.

Validation passes **234 ShadowRegression**, **35 SceneDepthRegression** and
**34 ContinueRegression** checks. These include actual native collision calls,
caller register/GTE-control preservation, transparent-receiver rejection,
script hooks, the training query ignoring a stale cached floor, and hidden GL
pixel tests. Logs are `.local/n64-shadow/visibility-tests-final.log`,
`scene-depth-final.log` and `continue-final.log`.

All six `visibility-bank` captures were inspected individually: floor, two
raised-ledge views, upper-wall climb, ceiling, and foreground occlusion.
Frame 4190 shows the floor silhouette below the ceiling-attached player; frame
4450 shows it clipped by intervening bank geometry. This run uses build
`b79efa1c563bb27a9d2ceb5e0b1d8e6269ee316a1ba8e9e8a3fc0f0a727478ce`.
The subsequent release changes only the shared Exit label's Y coordinate from
22 to 12; its shadow implementation is identical.

The release build is
`eb5a3ebc728b4d53f0220a7c2137e1d55fa3c0a90946dce969a0cfd09d6d507b`.
`training-zip` reaches **lha1_t through the original Training → Item Collection
→ Zip-line Training menus**, with no filename substitution or forced attachment.
All four captures were inspected in order. Frame 1946 shows the duration menu;
2451 shows the ceiling start with the native timer/coins/HUD; 2751 shows the
silhouette on a sloped receiver far below the ceiling; 3051 faces the ceiling
and cannot establish the off-screen shadow's appearance. The trace confirms
the fresh long query and its changing receiver heights/normals throughout.

Proof: [bank ceiling](../.local/n64-shadow/visibility-bank/frame_04190.png),
[bank occlusion](../.local/n64-shadow/visibility-bank/frame_04450.png),
[training ceiling and distant receiver](../.local/n64-shadow/training-zip/frame_02751.png),
[training run record](../.local/n64-shadow/training-zip/result.json).

All five `final-l6a1` captures were inspected in sequence. Frame 1598 shows the
normal floor silhouette; frames 2318 and 2378 show wall/ceiling attachment;
2678 shows wall attachment with the floor in view and no player silhouette,
matching Tunnel Crawl's suppression rule. The trace retains a valid opaque
receiver during suppression, so this is not just a missing collision plane.
Frame 2438 passes through the costume during the camera transition and is not
used as shadow proof. That camera-overlap behavior is outside this shadow change.
[Tunnel Crawl attachment proof](../.local/n64-shadow/final-l6a1/frame_02678.png),
[run record](../.local/n64-shadow/final-l6a1/result.json).

All five `final-l6a3` captures were also inspected. Its initial floor silhouette
is visible; the native trace confirms Lizard's Maze switches to suppression
with a valid opaque receiver when attached. The attached views face the nearby
wall/ceiling or intersect the costume, so they do not independently demonstrate
an unobstructed floor without a shadow. The shared rule's visible floor check
is the Tunnel Crawl capture above; both level selectors also match ROM execution.
[Lizard's Maze run record](../.local/n64-shadow/final-l6a3/result.json).

These are representative native visual checks plus ROM-execution regression
coverage, not a complete playthrough of every script in every level. Native
audio was deliberately muted; there is no audible-audio claim. All runs are
hidden and use process-local input, isolated settings/cards and a 180-second
watchdog. No host input or desktop capture is used.

Deployed this release to `Spider-Man/SpiderMan.exe` on November 21. Before/after
hash inventories confirm **only the EXE changed; all 1,807 other staged files
are unchanged**, including saves, settings, suit selection and assets. The
previous staged EXE already exists at `.local/menu-suits/sm1/SpiderMan.exe` and
was retained as rollback without making another copy. The completed shadow item
was removed from TODO.
[Deployment and preservation record](../.local/n64-shadow/visibility-deployment.json).

## Enter Electro

The two games compile the same projection, pose reconstruction, near clipping,
receiver visibility and depth-tested stencil code. Enter Electro supplies the
following bindings from its USA PlayStation executable (`SLUS_013.78`).

| Enter Electro code or field | Use |
| --- | --- |
| `800C21F4`, `800837C4`, `80084234`, `800842F0` | Identify the current player, begin/end its draw, and collect the meshes posed by both actor paths. |
| `8003C528` | Capture the native world-down query after collision. The query begins at `SP+40`. Its hit, fixed-point height, normal and face are at `+68/+70/+78/+80`. |
| `8007F2D4`, `800818F4` | Initialize and cast a separate collision query for zip-line training or the below-floor camera test. |
| `80088DE0`, `8008BDD8` | Face bits 0 and 6 identify textured and semi-transparent surfaces. |
| `8003B938`, `8003B8C8` | Player state is at `+FB4` and attachment at `+C10`. A skipped ground probe during wall or ceiling attachment retains the last ground hit. A reported miss clears it. |
| `GP+1310`, camera `+74` | Read the live rendering camera and its world-to-view rotation. |
| `80087DD8`, `8008800C`, `80088214`, `80088344` | Scope the near/far actor depth scale from `GP+1358` so foreground actors and scenery occlude projected shadows. |
| `800633EC`, `80057BC4` | Retire only the player's native blob and two native shadow quads through their original cleanup paths. NPCs and pickups keep native shadows. |
| `GP+D18`, `GP+D20`, `GP+8D0` | Locate the current frame's ordering table, packet cursor and packet limit. |

Campaign stages and demos use the normal collision receiver. The long probe
is enabled for the Training Session's `tr03_t` and the Challenge Session's
`wr03_t`. Both are selected as **Zip Line** through the retail Training menu.
The TR03 trigger explicitly teaches R1 zip-line traversal. This stage mapping is an
adaptation for the sequel. The specific Tunnel Crawl, Lizard's Maze and story
script exceptions belong to the first game's levels and scripts.

### SM2 release verification

The hidden, muted release reached Zip Line through the retail Training menu.
R1 attached the player to the ceiling at world Y `-941.77`. The 8000-unit probe
found face `800FEA38` on the platform at Y `657.84`, about 1,600 units below.
Two runs used the same process-local inputs and captured 1,903 frames after
`tr03_t.trg` loaded. Switching off the port shadow removes the silhouette
beneath Spidey in that view. In the 91×71 floor crop, 574 pixels darken by at
least 12 green-channel levels with the stencil enabled.
[Ceiling, stencil on](../.local/menu-suits/sm2-shadow-compare-on/frame_03786.png)
and [same view, stencil off](../.local/menu-suits/sm2-shadow-compare-off/frame_03787.png).

The retail Challenge Session's Zip Line option loaded `wr03_t`. Its timed course
showed a floor silhouette during ceiling attachment, with the long probe hitting
face `800FEAA0`. [Challenge capture](../.local/menu-suits/sm2-shadow-challenge-release/frame_03787.png).

In the opening campaign mission the red Big Time suit casts a posed shadow beside
the question-mark pickup's round shadow. The subsequent scripted movement went
over the rooftop edge and reached Game Over. The first frame supplies the
grounded comparison. [Rooftop capture](../.local/menu-suits/sm2-shadow-roof-release/frame_03047.png).

The 234 first-game ROM/renderer checks and 27 SM2 binding checks passed.
The latter exercise the native query, ownership, attachment, camera ray,
translucent receivers, training selectors, packet insertion, transform depth
and level reset. [SM2 regression](../tools/RecompOne/tests/Sm2ShadowRegression/Program.cs).

The staged self-contained SpiderMan2.exe has SHA-256
`5fdfcb4e9726226bf159247e559d91209c68102563e6092681d5183394aad68e`.
Before/after inventory matched all 1,186 other staged files.
[Deployment record](../.local/sm2-shadow/deployment.json).
