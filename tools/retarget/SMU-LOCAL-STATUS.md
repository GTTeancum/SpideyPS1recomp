# Local SMU checkpoint

## Current checkpoint

167 of 233 suits are installed and committed through batch17 plus capacity
recovery batch26. Each has full
offline rig/animation/web regression evidence; native gameplay evidence is
representative per batch, not per suit. These are conversion checkpoints, not
final visual acceptance. See `SMU-OPEN-ISSUES.json` for unresolved visual work.

Arana Gymnast and Poison Hunter now convert losslessly with a fallback that
packs across all 16 distinct transport slots and duplicates both alternate-hand
packets. Both pass 4,196 animation frames, 115 native web frames, and managed
actor/material loading. Separate native game runs and two inspected images for
each show their full bodies and costume geometry; Poison Hunter also has a visible
wrist-connected swing line. Arana's first image catches the jump before a visible
line, so it is not visual web proof. Their previous
packet-capacity failures are no longer conversion blockers. Damon Ryder (6,500
runtime vertices) and Other (4,511) still exceed the actual 4,096-vertex runtime
limit and need a rendering transport extension, not asset simplification.

Multi-material, authored alpha/repeat textures, multiple source meshes,
undefined-normal repair, and alternate finger layouts are supported. Source
geometry, full skin influences, hierarchy, bind transforms and provenance remain.
Batch18 has offline results but is not installed or accepted yet. Remaining
planned batches are preserved in the local task workspace.

The reported Poison tongue is attached to the mouth in a front-facing native
game capture. Its rigid upward-curled source pose still needs visual correction.
An explicit head-relative regression passes all 4,196 frames for its four tongue
bones and 82 weighted vertices. This is attachment evidence, not pose acceptance.
Six-Armed Spider-Man's extra arms retain their source rest pose, and visual fist
closure is not established for every rig. Both remain open work.

The sections below are the retained initial 15-suit checkpoint and its original
test totals/blockers. They are historical, not the current catalogue status.

## Initial 15-suit checkpoint

15 of 233 catalogue suits are converted and pass the complete offline rig and
animation checks. 218 remain unconverted. This is not the completed suit pack.

Batch01 / cumulative 07a is reconciled with the existing local source. The
253-row selector remains intact; both generation scripts now retain that limit.
Repair06 SFD, sewer, Linux, fists, shoulders and shared swing-wrist code remain.
The SFD audio upload was corrected for the installed Silk.NET OpenAL API.

Batch02 adds Venom, Poison Rhino, Venom 2099, Scarlet Spider-Ham and 2211.
Venom 2099 needs 1,074,028 source bytes because of preserved rig metadata.
The host file allowance is now 4 MiB; after stripping host-only RTG2 metadata,
its guest actor is 552,708 bytes and the 1 MiB guest limit remains enforced.
No source mesh, bind pose, weight, hierarchy or texture was simplified.

## Verification

- Batch01 installed assets: 246 checks, 41,960 animation frames, PASS.
- Batch02: 125 checks, 20,980 animation frames, PASS.
- Native web oracle: 115 swing frames per suit, 1,725 total, exact wrist
  endpoints with unchanged anchors and no segment seams. Offline diagnostics.
- Both Windows game projects build. Selector tests: SM1 382 and SM2 416 checks.
- SFD managed/native fixtures: 43 checks; movie instruction emitter: 7 checks.
- Portability and SM1 bundled actor validation: 257 checks.
- Real large-actor loader and source/guest size rejection tests pass.
- Actual SM1 game framebuffer captures in level l5a3 show Spider-Ham, 2099,
  Poison Rhino and Venom 2099 during a swing and after release. Each run exits
  normally at frame 2400. Captured frames were inspected individually.
- Each live run logs both the preserved-rig pose hook and the shared web-wrist
  hook. The web endpoint is computed from the animated wrist, not a fixed height.

The first frame in each successful capture run is the level's Venom cutscene,
not proof of the selected player suit. Only the later HUD/gameplay frames count
as player-suit evidence. Native capture uses only the game's framebuffer and
process-local input; no desktop capture or host input was used.

## Concrete Blockers

All 233 sources were structurally audited. 198 pass preflight, including the 15
converted suits. Preflight does not mean converted, animation-tested or shipped.
35 fail before conversion:

- 10 exceed the native 18-packet, 256-vertices-per-packet transport capacity.
  Examples: Adriana Soria, Dorma, Izumi and Venom Verse Carnage. Increasing the
  metadata byte limit does not fix this geometry capacity limit.
- 13 need lossless multi-mesh/material support, including Aaron Davis and both
  newer Iron Spider variants.
- 4 have authored transparency bindings requiring explicit runtime handling.
- 3 have different finger layouts from the current fist calibration assumptions.
- 5 generate nonfinite vertex/normal data rejected by the native validator.

The exact per-suit results are in verification/local-batch02/catalogue-preflight.json.
Do not work around these failures by dropping meshes, materials, weights or bones.
The remaining 183 preflight-compatible suits still need conversion and all
acceptance gates; they are not counted as complete.

A separate real-game failure remains: the l1a1 Bank Heist intro exited with
0xC0000409 in ucrtbase.dll after the comic cover. Its capture is not gameplay
proof. The l5a3 acceptance runs completed normally. Root cause of the intro
failure is unverified. Full live SFD/audio acceptance, Linux execution and SM2
suit retargeting remain unverified; the latter was absent from the imported pack.

## Local History

The pre-import source is preserved on safety/pre-07a-source-snapshot. Master
was not advanced. Work continues on repair/smu-07a-local; no changes were pushed.
Original source archives remain untouched and untracked. Verification summaries
and the updated progress journal distinguish offline checks from live evidence.
