# Bank rendering reports — November 16, 2026

## November 22 SM1 bank entrance

The bank entrance repair is in the staged SM1 executable, SHA-256
`11654d48de7f0413f7244a4ba352d17a7b3c2ed1dcb2ddc6f41f3d6b629889c1`.

On November 22 the tester confirmed the bank geometry fix during normal gameplay in the staged SM1 build.

The playtest was on November 16 with an earlier build. The specific build is
unknown. Commit `15deb3e` introduced horizontal scaling in GTE projection on
August 29. Commit `e62d7ba` repaired native object bounds and GTE subdivision
on September 8. Its SM1 hooks do not cover the CPU clipper. The same hooks are
present in the September 29 `sm1-v1.1` tag, November 5 `6541518` and November
10 `6d7a0c3`. The November 10 rollback `b292b97` returned to the November 5
tree. The September 29 Windows release reproduced both gaps using its packaged
executable, period costume files and the same bank camera positions.
[Release, rollback and corrected views](../.local/bank-entrance/SM1-bank-projection-proof.png).

The CPU clipper used fixed 4:3 bounds and rounded camera-space intersections.
Its triangle and quad grids then used a CPU perspective divide that bypassed
GTE subpixel projection and lost depth before texture interpolation. The repair
carries the source vertices through the five actual clip planes, computes each
edge intersection once for both position and the native texture/color weights,
and propagates screen position and depth through the 15- and 25-vertex grids.
Ordinary world geometry still draws in the game's ordering-table order.

The headless, muted comparison covered 32 stationary bank views and 24 captures
of a recorded approach and camera turn. At frame 2169 the sign-side region
(x ≥ 855) went from 101,437 black pixels in the rollback to zero. At frame 1899
the 129,736-pixel opening facing the elevator closed. In four consecutive
rendered frames of the approach, all 418 triangles using the hallway wall
palette carried depth at every vertex. Three additional 4:3 gameplay captures verified the sign, elevator and doorway.
The regression suite passed 72 checks,
including textured pixel readback and both 4:3 and widescreen clipping.
[Recorded camera turn](../.local/bank-entrance/SM1-bank-walk.webp).

### November 16 renderer comparison

The earlier rollback build restored suit-only depth, the four native
actor-transform hooks, the original per-list custom draw order and the player
shadow used in the November 16 10:12 p.m. deployment. Its SHA-256 is
`ae33b83a3518299aaeb6cda7a9fc8b2a2760a8b4e3e184491199be7b4134c1a9`.
The two executables have matching compiled method bodies for all 36 methods
in PlayerShadow, WorldFrustum, WorldSubpixel and Wide, and all 15 checked
methods and wrappers for the bank world draw, clipping, subdivision and actor
transforms.

Each executable ran the same 32 hidden, muted bank captures with the slot 1
save and the Amazing TASM suit. At frame 2169 the black wall gap beside THE
BANK sign covers the same pixels in both images. All 101,437 pure black pixels
in the right side (x ≥ 855) match in the two native captures. Character poses
and lighting differ in some other views.
[Archive and fresh build side by side](../.local/bank-entrance/SM1-bank-Nov16-vs-rebuilt.png).

The November 21 staged retest confirmed the SM2 opening sweep. SM1 still had a
vertical black strip between the wall panels by THE BANK sign. The strip stayed
visible with the player and camera at rest. It also appeared on the opposite
side of the picture looking back at the elevator.

A hidden, muted SM1 run reproduced both views at player position
`(2730, -97, -2850)` in `l1a4_t`. It used the staged Amazing TASM suit.
The sign view reproduced with frame interpolation disabled as well.
[Sign before](../.local/bank-occlusion/hall-seam/frame_01305.png) and
[elevator before](../.local/bank-occlusion/hall-poses/frame_01899.png).

SM1 has two CPU projection loops for clipped, subdivided faces. The triangle
loop at `80079AE0` and quad loop at `8007A2D0` both divide camera X and Y by
camera Z and add the original 512 × 240 screen centre. These vertices bypass
the GTE widescreen projection and arrive at the GPU without depth. In the
sign view, 48 wall triangles using CLUT 358 took that path. At the junction,
the GTE placed one corner at X 299.51 with depth 446.51. The CPU placed its
copy at X 313 using the 4:3 projection. The difference opens the visible gap
and the missing depth also makes texture coordinates interpolate affinely.

### Source history from the slot named a

Slot 1 contains `L1A4_T`. Its 180 bytes match the November 16 card backup
from 9:22 p.m. Eastern. The card format does not date individual slots. The
November 18 history entry belongs to slot 3.

The local executable archived before the Continue fix (`a73bd43d4994`) and
the November 16 10:09 p.m. build (`b5d3f42bc6b0`) both tested depth only
for replacement suit surfaces. By the 10:47 p.m. local build, `GlCore.DrawTri`
also sent opaque world triangles through `LEQUAL`. Overlapping scenery drawn
in PS1 ordering-table order could then lose pixels to a previous draw. This
change subsequently landed in `911aebd` on November 21. The staged rollback
(`1eaf85793040`) writes receiver depth with `ALWAYS` for ordinary scenery.
The maximize change in `806e642` guards a host window size write.

The archived method bodies for the SM1 world draw loop (`80076480`), world
transform (`8007C2AC`), clipper (`80077EC8`), and both CPU grid builders
(`80079574`, `80079DB8`) match before and after the depth change. All eight
methods in `WorldSubpixel`, the three in `WorldFrustum`, and the widescreen
classifier also match. The bank geometry, lighting/texture library, objects,
trigger file and all 689 staged builtin asset files match the November 16
installation manifest.

An experimental SM1 build aligned the CPU-projected vertices with the original
GTE edges, supplied camera Z to the texture interpolator and widened the two
horizontal clip planes in both classification and intersection. At the same
stationary position with interpolation enabled, the wall beside THE BANK and
the floor edge facing the elevator meet in both directions. The experiment was removed when SM1 returned to the November 16 rendering code. [Hallway views before and after](../.local/bank-entrance/SM1-bank-hallway.png).

### The black wedge through the doorway

At the circled type of opening below the brown panel, a source-level ray
through the recorded view crosses bank floor mesh 165 (`Fourth_11`, name CRC
`47AC876B`) before the black face on `Third_01`. The floor is about 1,085
camera units away. The face behind it is about 1,733 units away. The captured
GPU packet carries RGB (0, 0, 0) at every corner of that face. Its authored
color word at `l1a4_g.psx + 0x2C98 + 8` is `0x2C000000`.

The bank entry script begins at `l1a4_t.trg + 0x6AA`. Its opcode 191 at
`+0x76E` hides `Fourth_00` through `Fourth_28`. The native group setter
at `8005F984` sets object flag bit 0. The visibility pass at `8007B304`
checks that bit before the frustum planes, then sets bit 15 at `8007B5B4`.
The draw loop at `8007660C` skips the object. Five archived bank snapshots
taken on November 16 before the depth change already show `Fourth_11` with
flags `0x8001`. The reported staged build has the same flags. The experimental CPU
projection correction closed the hallway seams while the room script still
hides the doorway floor. The available source and snapshots do not establish
what first exposed that sightline during the user's playthrough.
[Doorway capture](../.local/bank-occlusion/hall-trace/frame_01447.png).

The original September v1.1 executable also draws the dark face below the door
panel in a native 4:3 view. The new widescreen build shows it at the same
doorway. The disc mesh contains the black face and the bank entry script
controls the floor object in front of it.
[4:3 release and corrected widescreen doorway](../.local/bank-entrance/SM1-doorway-comparison.png).

## November 21 rollback in both games

The scene-wide depth comparison introduced in `911aebd` could hide later
scenery behind the backdrop. The SM2 opening camera sweep in `e1m0` exposed
this at the Vicarious Visions billboard. Buildings on both sides disappeared
partway up their façades. The same ordering change also affected SM1.

Both games again draw ordinary geometry in the native ordering-table order.
Opaque surfaces record the depth of the visible receiver for the N64 silhouette.
Replacement suit faces still depth-test against each other. Textures still use
the original per-vertex projection depth for perspective-correct interpolation.

Hidden, muted SM2 runs captured 45 corresponding views from the level load
through the opening sweep. At frame 1747, the rollback restores the tan tower
and the striped tower on opposite sides of the sign. Frame 1987 restores the
neighboring buildings seen through the unfinished floor. A further 20 captures
follow the camera down to Spider-Man and Beast. [Billboard comparison](../.local/bank-geometry/SM2-billboard.png),
[window comparison](../.local/bank-geometry/SM2-window.png),
[animated sweep comparison](../.local/bank-geometry/SM2-opening-sweep.webp).

SM1 runs covered the entrance in both `l1a3` and `l1a4` and a right-stick
turn near the bank sign with the staged Amazing TASM suit. The 63 rollback
captures showed the corridor, thresholds and bank walls. These routes did not
reproduce the reported whole-section loss or establish where textures visibly
warped. [Entrance](../.local/bank-occlusion/geometry-after-presented/frame_01200.png),
[doorway during the turn](../.local/bank-occlusion/geometry-turn-after/frame_01410.png).

The hidden GL pixel tests verify native draw order, suit self-occlusion and a
receding wall whose perspective-correct sample differs from its affine sample.
All 40 scene-depth checks, 234 shadow checks and 27 SM2 shadow checks passed.
[Staged executable hashes](../.local/bank-geometry/deployment.json).

The November 16 wall/hostage comparison below describes the superseded behavior.
Native ordering can again show the bank hostage through a partition.

## NPC occlusion

The user supplied three screenshots from the staged SM1 install. The second
shows a kneeling NPC through the opaque bank wall at the left edge; the third
shows part of the NPC through the wall beside the right column. This is visible
occlusion failure; the screenshots alone do not distinguish incorrect draw order
from misplaced actor geometry.

The preserved log loads `l1a4_t` at 22:21:39 and again at 22:22:50, matching the
screenshots taken around 22:22–22:23. The active custom model is
`smu-spiderman-surveillancesuit`. The earlier bank report identified `l2a1` and
predated orbit deployment, so this is not established as an orbit regression.

Private evidence is retained inside the repository:

- `.local/bank-occlusion/reported-1.png`
- `.local/bank-occlusion/reported-2.png`
- `.local/bank-occlusion/reported-3.png`
- `.local/bank-occlusion/reported.log`

### Repair and evidence

The wall-overdraw failure is reproduced and repaired. In the packaged EXE's
same-frame comparison at frame 2870, part of a hostage is painted through the
partition at the left edge with scene depth disabled. Enabling scene depth hides
that part behind the partition while keeping the actors in front visible.
The geometry, camera, textures and animation pose are identical in the pair.

Private native captures (1707 x 960, no synthetic image reconstruction):

- [Depth off: hostage visible through partition](../.local/bank-occlusion/release-proof/frame_02870_depth_off.png)
- [Depth on: partition occludes hostage](../.local/bank-occlusion/release-proof/frame_02870_depth_on.png)
- [Packaged run record](../.local/bank-occlusion/release-proof/result.json)

`GlCore.DrawTri` previously depth-tested only replacement-model surfaces. Native
actors and scenery relied on the PS1 ordering table, allowing actor triangles
submitted later to paint over closer wall triangles. SM1 now depth-tests opaque
world triangles with complete GTE projection provenance. HUD, screen-space
backgrounds, translucent effects and shadows keep their existing ordering;
transparent texture pixels still discard without occluding geometry behind them.

Actor depth must first be normalized. Native `80077198` selects a higher-precision
path for nearby actors: translations and model coordinates use 16 times the
scenery units. `SceneDepth` scopes host projection depth to 1/16 for that mode
around `8007B9CC` / `8007BD04`; far transforms `8007B798` / `8007BBD4` use scale 1.
Native GTE SZ, screen coordinates, clipping flags, ordering-table indices and
gameplay memory are unchanged. Enter Electro now scopes the same depth normalization
around its own four actor transforms. See [SM2 stencil shadows](n64-shadows.md#enter-electro).

The captured geometry independently identifies the ordering failure: frame 2869
has 22 sampled hostage-triangle centroids behind closer wall triangles that were
submitted earlier. For example, actor triangle 1636 has camera depth 1172.71 at
its centroid, while wall triangle 489 (CLUT 360) is at 1130.16. Both carry valid
GTE depth. This supports the inspected capture pair; it is not a substitute for
the texture-aware rendered result. [Trace](../.local/bank-occlusion/release-proof/occlusion-trace.txt).

Validation:

- 35 focused checks pass: all four original actor transforms, near/far depth
  agreement, unchanged native projection, opaque visibility in both draw orders,
  front actors, HUD, blending, texture cut-outs, frame/replay clearing, and A/B
  readback. [Test source](../tools/RecompOne/tests/SceneDepthRegression/Program.cs)
  and [local log](../.local/bank-occlusion/tests.log).
- All 12 native images from the packaged six-angle bank comparison were inspected
  individually. The run exited normally after 81.42 seconds. It was hidden and
  muted, with process-local scripted input, a 180-second watchdog and an isolated
  memory-card copy that remained byte-identical. No desktop input was generated.
- A separate normal-presentation run (comparison disabled) exited normally after
  66.38 seconds. Its three native captures were inspected: startup legal screen,
  bank entrance with the Surveillance suit and HUD, and the bank partition with
  the hostage correctly hidden. [Normal gameplay capture](../.local/bank-occlusion/release-presented/frame_02870.png)
  and [run record](../.local/bank-occlusion/release-presented/result.json).
- Continue's 34 regression checks still pass. The wider rendering suite retains
  its six existing GP0 wrap failures (four effect-bound checks and two precise-XY
  checks); this is not an all-green rendering-suite claim.

The opt-in comparison uses `SPIDEY_CAPTURE_DEPTH_PAIR=1` with interpolation
recording enabled. It reads the recorded world draw area, not the GPU's display
origin: the game can already have flipped to the other buffer at capture time.
Earlier `paired/` and `alcove/` runs read the wrong buffer and are **not** A/B
evidence. Replay captures can precede HUD submission; normal presented-frame
captures are checked separately. The comparison flag is unset for normal play.

Validated release SHA-256:
`f52b190304e4448d6bb2493b765a9eba881e5974ec0bcca7d5a880ec524fa465`.
Deployed to `Spider-Man/SpiderMan.exe` on November 16, 2026. A before/after
SHA-256 manifest confirms all 1,807 other staged files are unchanged, including
settings, memory cards, suit selection and assets. The previous EXE is retained
at `.local/bank-occlusion/SpiderMan.before-clipping.exe`.
[Deployment record](../.local/bank-occlusion/deployment.json).
The separate N64 shadow work is recorded below and in [the ROM trace](n64-shadows.md).

## N64 shadow parity: next task

Update, November 17: the actual N64 projection and cached ground receiver have
been traced, replayed and implemented. Native captures confirm the bank floor,
raised ledge and ceiling behavior. See [trace, implementation and validation](n64-shadows.md).
The original report and acceptance criteria below remain as history; exceptional
visibility rules outside these tested bank cases remain in TODO.

The user supplied a fourth screenshot showing Spider-Man attached to the
ceiling. The silhouette on the floor appears to follow Spider-Man rather than
the expected light direction. The requested reference is the actual N64 game,
not an approximation based on the current "N64-style" code comment.

Local private input: `Spider-Man (USA).z64`, 33,554,432 bytes, big-endian N64
header `80371240`, internal title `SPIDERMAN`, game code `NSLE`, revision 0.
SHA-256: `feff90ed1201c91ff167d66958048e61c192c9d6a756ddb98f799017ac9cd25c`.
The ROM is ignored by Git. Ceiling evidence is
`.local/bank-occlusion/reported-ceiling-shadow.png`.

Trace the N64 shadow code and its callers/data, document ROM/runtime addresses
and relevant transforms, and mirror its behavior in the port. Establish how it
chooses the light vector and receiving geometry, projects the silhouette,
handles ground/wall/ceiling attachment, and clips or suppresses shadows. Record
any behavior that differs between the original and replacement character meshes.
Validate representative poses and camera angles against the traced behavior
using silent, non-interactive tests and native captures where needed. Inspect
the actual resulting visuals; frame counts alone are not acceptance evidence.

Before this repair, SM1 `PlayerShadow` flattened captured triangles along a configured fixed
world-space light (default X=0.45, Y=1, Z=0.55), transformed into camera space.
It infers a horizontal receiving plane from the original blob's screen position;
it does not obtain scene light or receiving geometry from the N64 game. Blob
identity is inferred from material and proximity, and cached triangles have no
capture-generation check in `OnPrim`. These are investigation leads, not proof
of the cause of the ceiling report. Capture and drawing can span a vblank, so
a simple frame-number equality check is not a justified fix.

The N64 trace shows that the earlier fixed-light estimate was incorrect. The
current implementation projects vertically onto the native cached ground plane
and applies the ROM's exact basis construction on sloped receivers. Broader
level/script suppression parity is explicitly tracked separately.
