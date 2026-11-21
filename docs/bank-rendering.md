# Bank rendering reports — November 16, 2026

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
