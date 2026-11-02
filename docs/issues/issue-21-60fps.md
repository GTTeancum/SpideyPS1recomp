# Issue #21: 60 FPS interpolated

Both games run their logic at 30 FPS. The host now presents 60 FPS by drawing an
in-between frame, without changing the game clock or what the game draws.

## How

- `FrameInterpolator` wraps the GL backend. Every call passes straight through and is
  recorded. At each game-frame presentation the recording is replayed at the halfway
  point and presented; the real frame is then replayed and presented half a frame
  later. VRAM transfers are not replayed.
- `GteIds` names every GTE projection so the same point can be found in consecutive
  frames: source address (LWC2) or model coordinates, plus occurrence count since the
  last display swap. The name reaches the GPU vertex through the exact projected screen
  position the provenance sidecar already carries.
- Both games build world vertices on the CPU as world position minus camera position,
  then project with the camera rotation and no translation. The camera position is at
  0x800A575C (SM1) / 0x800B3254 (SM2), rotation at 0x800A57CC / 0x800B32C4. Adding the
  position back names scenery by its world position; untranslated points also keep their
  raw name for things drawn around the camera on purpose (the sky).
- Scenery triangles are not matched point by point; they are redrawn from a camera
  blended between the two frames (rows rescaled, not orthonormalised: the view matrix
  carries the pixel-aspect scale). That keeps faces the game splits differently from
  frame to frame joined without seams.
- Ground subdivision points (`WorldSubpixel`) are named from their corners and weights.
- Characters and other moving objects use name matching; a point shared by several
  triangles takes one movement. The silhouette shadow names its own vertices.
- The real frame is redrawn before the half-frame wait, so only the swap is timed and
  the game keeps nearly the whole other half frame for its own work.

## Verification (2026-11-01)

- SM1 rooftop, running: in-between frames sit halfway in pose, scenery and shadow; no
  seams after the blended-camera change (earlier versions showed background slivers).
- SM2 e1m0 rooftop, running, custom suit: same.
- SM1 boot, menus, level-start cutscene and gameplay sweep: no artefacts.
- Presentation spacing (hidden window): 16.72 ms average, 23 ms worst, against 16.67.
- Regression suite passed.

The setting is under Display, "60 FPS (interpolated)", on by default.
