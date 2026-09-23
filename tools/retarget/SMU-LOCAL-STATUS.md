# Local SMU checkpoint

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
