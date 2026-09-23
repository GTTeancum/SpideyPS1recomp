# OpenSpidey SMU Batch 01 — cumulative 07a

**This is a SMALL-BATCH checkpoint, not the completed 233-suit pack.**
The original uploaded project or any accepted Repair02–06 baseline needs only this
one cumulative patch for the code/assets completed so far. Nothing is sent to
Work/Codex, a repository, or your PC by this package.

## Completed this batch
Eight new native retargeted suits: **1602, 2099 New, unmasked Ben Reilly, Big Time,
Big Time Red, Spider-Gwen, Gwenom, and Noir**. Spider-Ham and 2099 were regenerated
as regression controls; their native actors and full-resolution RGB textures are
byte-identical to Cumulative06. Their existing selector names/reports are retained.

**10 / 233 ready-catalogue entries verified; 223 pending.** The supplied archive's
preliminary model FBXs are not duplicate costume targets. Last Stand was explicitly
excluded by that source catalogue; no extra costume has been invented for it.

Every tested FBX hierarchy, bind matrix, source property, cluster and exact source
weight list is retained in RTG2 provenance. All runtime influences and triangles
survive. The existing fist/thumb policy, independent shoulder calibration and
native swing-wrist correction are applied unchanged. The 10 tests include 31,306
original triangles; no decimation or substitute template weights were used.

The batch tool checks the FBX material connections. These diffuse images have
non-opaque alpha data, but the supplied materials do NOT connect it to opacity.
Their existing opaque RGB interpretation is retained; the original RGBA sources
are untouched. Full SMU normal/specular/emission/shadow-ramp shader recreation is
not included. Material-aware atlas/geometry support for multi-diffuse scenes is a
pending task, not silently flattened or labelled complete.

## Actual integration update
`SpiderMan.csproj` now copies **all mods/suits/** to build/publish output, not just
Spider-Ham and 2099. The eight suits use the existing runtime's native `actor.psx`
and `suit.json` paths. This is not just a viewer export.

**All 126 prior Cumulative06 paths are retained.** The SFD decoder/player, sewer
safety guards, shared Windows/Linux build changes, fists/shoulders/web runtime and
native DLL/ELF files are unchanged. Only the suit-output copy rule and the
regression harness changed among prior files. New tooling adds per-suit journals,
source-bound material checks, native web tests and repeatable offline renders.

## Verification completed here
- **246 checks passed**, covering all **300 original clips / 4,196 frames per suit**:
  **41,960 character-frames**. Tests include source rig/cluster/weight preservation,
  triangle and texture identity, fist independence, target proportions, unchanged
  link lengths, no shoulder translation transfer, signed-16 transport limits and
  malformed rig rejection. C# hook checks within this count are explicitly static.
- Original native socket/line routines were compiled and executed for both swing
  clips: **1,150 character-frames / 18,400 line segments**. Every final coordinate
  equals the current retargeted wrist, with unchanged anchor and original actor/rope
  buffers, and no segment seams. This uses controlled offline fixture transforms,
  not a saved gameplay scene. No final native GPU projection/occlusion was executed.
- The freshly rebuilt Linux native library is byte-identical to Cumulative06. No
  runtime ABI change is introduced by this batch.
- Final package/installer results are recorded in `BATCH-VERIFICATION.json`.
  Historical Repair06 evidence is retained under `evidence/`; do not count those
  old logs as fresh tests of this batch.

**No game EXE, full .NET build, live managed hook test or in-game screenshot is
included.** These remain native-asset/core-verified changes. SM2 suit-retargeting
integration has not been implemented. This checkpoint updates SM1 suits; the
previous shared/build repairs for both games are carried forward unchanged.

## Install or keep for later
Extract, run `INSTALL-REPAIR.cmd`, and select the project root containing
`spiderman`, `spiderman2`, `tools`, and `dreamcast`. The hash-checking installer
backs up replacements, refuses unknown local edits, and rolls back ordinary write
failures. Keep the game/editors closed while installing. Do not manually overwrite
a differently modified project if the installer rejects it.

The included `BUILD-RETARGET-REPAIR.cmd` is the existing .NET 10 build entry point.
A matching rebuild is still required for any not-yet-applied source changes. There
is no need to test an old EXE now; this archive can simply be kept as the saved
small-batch checkpoint until the remaining catalogue work is complete.

Original SFD media is not duplicated. Direct SFD playback from Repair06 is retained;
optional Dreamcast-disc detection/extraction remains deferred as previously agreed.

## What is next
The current selector admits only 40 mod entries. Its native list/routing/save
behaviour must be expanded before shipping all 233. Larger and multi-material
rigs require lossless support rather than dropped triangles. Progress and all
source identities are in `overlay/tools/retarget/SMU-BATCH-PROGRESS.json`.

New converters/tests under `tools/retarget/` accept the supplied source archive
and original donor. Batch01 conversion reports contain actual temporary paths
from this run; those are audit records, not installation locations. The original
source FBXs/texture sources remain in the user's upload, not the game mod folders.
