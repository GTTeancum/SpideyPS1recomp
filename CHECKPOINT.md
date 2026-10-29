# SpideyPS1 Recomp checkpoint

Updated: 2026-10-29 (Eastern Time). Resume this file when asked to continue
SpideyPS1 Recomp, then inspect the current branch and working tree.

## Objective and authorization

Optimize and standardize both games, fix demonstrated bugs, and work toward full
gameplay verification. The user authorized GitHub pushes, compilation, and test
support for `GTTeancum/SpideyPS1recomp`. Work on the existing `master` branch;
preserve unrelated changes. Use cloud tools and ordinary collaborating agents.
Do not launch Codex tasks/services or use the user's Dot or PC. Keep chat updates
within one screen. Private discs, extracted retail data, saves, credentials, and
backups must not enter public commits.

## Repository and execution state

- This session started from clean `master` at
  `b100572675d4caba3cf3162529b300e88be033f4`.
- The first batch is published as
  [`b7e02a1`](https://github.com/GTTeancum/SpideyPS1recomp/commit/b7e02a1af4ed2670c3aa54a08e850e7accad944b).
  Its new [Windows and Ubuntu workflow](https://github.com/GTTeancum/SpideyPS1recomp/actions/runs/40806344318)
  completed successfully on both platforms.
- The sewer repair, converter safeguard, diagnostic frame gate, and expanded
  regression coverage are published as
  [`307562e`](https://github.com/GTTeancum/SpideyPS1recomp/commit/307562ee3f20c6ff6f8b1d96e6825763ef5d308c).
  [Run 40807894298](https://github.com/GTTeancum/SpideyPS1recomp/actions/runs/40807894298)
  passed on Windows and Ubuntu. This documentation follow-up records those
  completed results; it does not change the verified source.
- The shared tools target .NET 10. Local verification uses Linux and SDK 10.0.403.
  The restricted shell's first MSBuild attempt failed without a useful diagnostic;
  authorized cloud execution with network/subprocess access passed. No source
  workaround was committed.
- Retail input and regenerated game C# are absent from the clean repository.
  Bundled replacement actors, synthetic fixtures, and the public SM1 v1.1 release
  supported the checks below. The release was used for isolated routine execution,
  not a booted game or a level run.

## Community issue intake — 2026-10-29

Reviewed the indexed v1.1 release discussion and compared it with all nine current
GitHub issues and the existing checkpoint/TODOs. The new
[community report tracker](docs/issues/community-reports-2026-10-29.md) records ten
separate reproduction targets with source links, proposed priorities, and closure
criteria. R05 maps to open #8; R10 adds a distinct failure alongside #6. Other
symptom similarities remain comparison leads, not confirmed duplicate causes.

All intake entries are reported and unverified on the current build. This pass
reviewed source, issue history, and documentation; it did not build or play a game.
The prior CI and isolated-routine evidence below remain historical results for
their exact commits. Workspace maintenance removed the prior local checkout and
tool cache; recover source from GitHub and restore the SDK before the next local
build. No private retail inputs had been supplied.

Next: keep the L5A3 acceptance run first, then prioritize R01 and R02 once the
supported private gameplay assets are available. The expanded queue remains part
of campaign and costume verification; no report is closed by this intake.

## Completed changes

### Diagnostic memory and custom textures — published in b7e02a1

- `RamLogger` allocates access-history arrays only while the RAM map or memory
  editor is open. `PSMemory` creates its freeze mask on the first real freeze and
  releases it after the last unfreeze or Clear. Guest writes retain dispatcher
  notifications, guards, idle detection, and projection-depth invalidation.
- `HostWindow` creates RAM-map image buffers and the GL texture on demand, waits
  for conversion/swap before releasing buffers, and consumes a ready image before
  another worker can swap it.
- These changes avoid **40 MiB of array payload** in the ordinary 8 MiB guest
  configuration: 16 MiB of timestamps, 16 MiB of images, and an 8 MiB freeze mask.
  This is checked allocation behavior, not a measured Windows private-bytes or
  frame-rate improvement. The GL/UI lifecycle has not received visual acceptance.
- `SuitModel` rejects noncontiguous or reordered texture records, unsupported
  palettes, missing native row/record padding, and unexplained trailing data.
  Valid odd-width 4-bit/8-bit textures and independent material ordering remain
  supported. An audit found 36 bundled 18-part actors / 463 texture records
  compatible with the tightened parser.
- Replacement textures no longer generate mipmaps that their existing Linear
  minification setting never sampled. Level-zero filtering remains the same.

### Sewer collision corruption — published in 307562e

The [issue #5 log](https://github.com/GTTeancum/SpideyPS1recomp/issues/5#issuecomment-5682904515)
reports SM1 `L5A3`, frame 4284, shortly after two Lizard-man spawns. Collision
traversal encounters `0x0D6F0201`, with neighboring corrupted data `0xF2820894`.

The bundled `lizman.psx` and `lizman2.psx` have 19 objects but 56 meshes and 37
terminal LOD markers. Native preprocessing (`func_80074C98`) counts the terminal
markers as animation parts and caches 37 at model-table entry +52. All 40 raw
animation clips in either actor actually encode 19 parts. The sampled decoder
(`func_800742A8` / `func_80074310`) writes 24 bytes per part into scratch beginning
at `0x800A58E4`; collision globals begin only 720 bytes later at `0x800A5BB4`.

An isolated harness executed the released parser and decoder with the real marker
layout and animation bytes. A count of 37 writes 888 bytes and reproduces both
reported values at collision-grid +8/+12. The actual repair hook changes the
count to 19; the same released decoder then writes 456 bytes and leaves the grid
sentinels intact. This passed for both bundled Lizard-man files. The probe supplies
the documented post-loader return contract; it does not execute the complete
loader or play the sewer level. Reproduction source, input hashes, and commands
are in [the issue #5 evidence note](docs/issues/issue-5-sewer-crash.md).

- `spiderman/patches/Sm1AnimationBounds.cs` runs after `func_80068BB0`, when the
  full loader has installed its model record and animation-bank pointer. Both the
  checked-in config and its generator register the post-hook.
- The hook requires an exact live expanded allocation, a version-4/magic-2 model,
  an overcount, an object count within the 30-part scratch capacity, and an actual
  tag-0x2A payload found through the bounded metadata chain. Every clip, including
  the final one, must exactly prove the object count before the cached count is
  corrected. Guest registers, model geometry, and LOD data are preserved.
- Matching counts, retail-heap models, freed allocations, compressed banks,
  padding, malformed metadata, and ambiguous layouts remain unchanged. Review
  caught and fixed an initial header-width mismatch before publishing: the real
  packed header is `0x00020004`, not the 32-bit value 4.
- `LooseWadOverrides.TryGetAllocationSize` supplies the exact tracked bounds.
  The converter now rejects newly emitted layouts whose terminal count differs
  from the object count. Existing bundled ZIPs do not need to be rewritten for
  the runtime correction to apply in a rebuilt game.
- The broader audit covered 43 raw banks / 1,049 clips. Only the two Lizard-man
  assets had the mismatched terminal count; padded symbiote clips belong to
  models whose counts already agree and are left untouched.

### Diagnostics and repeatable verification

- Fixed `MemGuard.HitValue` ignoring `SPIDEY_GUARD_AFTER_FRAME`. Matching writes
  before the configured start no longer consume the report budget. Address and
  value watches now share the same gate. This threshold counts **host presents**;
  the game's logged vblank count can be twice as large.
- `python tools/run_regressions.py` builds the shared runtime/recompiler and runs
  seven .NET suites plus the Python converter suite. Logs and `results.json` go
  to ignored `artifacts/regressions/`. GitHub runs the same gate on Windows and
  Ubuntu with .NET 10, Python, Pillow, and a positive guard threshold of 2000.
- README and game TODOs distinguish historical runs, isolated reproductions,
  and remaining gameplay verification. The historical SM2 claim that SM1 still
  lacks the unresolved-anchor guard was corrected after checking both sources.

## Actual verification

The first published six-suite gate passed locally and on both GitHub platforms
([run 40806344318](https://github.com/GTTeancum/SpideyPS1recomp/actions/runs/40806344318)).
The expanded eight-suite gate passed locally and on **both Windows and Ubuntu**
at source commit `307562e`, with `SPIDEY_GUARD_AFTER_FRAME=2000`. Both final job
logs confirm the suite counts below and 6 build warnings / 0 errors:
[Windows job 123231090415](https://github.com/GTTeancum/SpideyPS1recomp/actions/runs/40807894298/job/123231090415)
and [Ubuntu job 123231089956](https://github.com/GTTeancum/SpideyPS1recomp/actions/runs/40807894298/job/123231089956).
Per-suite logs and JSON results were uploaded as workflow artifacts. There are
no pending source edits or test runs from this batch.

| Check | Actual result |
|---|---|
| Shared Release build | Pass; 6 existing warnings, 0 errors |
| BundledAssetsRegression | Pass; 12 assertions |
| CustomSuitModelRegression | Pass; 24 cases |
| DiscInstallRegression | Pass; 17 checks |
| InteriorHookRegression | Pass; 2 synthetic execution cases |
| MemoryDiagnosticsRegression | Pass; 38 checks with guard threshold 2000 |
| Sm1AnimationRegression | Pass; 33 checks, bounded reads/writes and register preservation |
| VideoSetupRegression | Pass; 2 grouped check sets |
| Sm1ActorAnimationLayoutConverterRegression | Pass; 6 Python tests |
| Python native texture-repeat tests | Pass; 8 tests |
| Custom texture parser before/after | Old parser accepted a truncated odd-width 4-bit row; fixed parser rejects it |
| Value-guard before/after | Old code failed 2 new checks; repaired code passed all 38 checks |
| Converter invariant before/after | Old code failed 2 negative cases; repaired converter passed all 6 cases |
| Released parser/actual hook/decoder reproduction | Both Lizard-man files reproduce exact issue #5 corruption at 37; actual hook corrects to 19 and grid stays intact |
| Independent runtime source review | Pass; bounded metadata/clip checks, correct header and hook, preserved registers |
| Config/runner and diff checks | Pass; hook registered once in both configs, valid JSON, clean whitespace |

These checks do not boot either game, render gameplay, or establish campaign
completion. The new patch is compiled through the regression project, but a full
game regeneration/build still requires the supported private retail input.

## Remaining blockers and next action

1. **Verify the sewer fix in gameplay.** Supply the supported private SM1 input
   (USA SLUS-00875), regenerate/rebuild, confirm the model-load correction is
   logged, then play `L5A3` through the two spawns, progression, and level exit.
   Check animation and near/distant geometry as well as crash absence. Do not
   mark issue #5 closed solely from the isolated decoder proof.
2. **Reproduce costume report #8.** The parser defect is demonstrated, but no
   supplied failing pack or selection sequence ties it to the
   [reported texture corruption](https://github.com/GTTeancum/SpideyPS1recomp/issues/8).
   Existing texture retirement and SM2 custom-actor cleanup are present; a leak
   has not been established. Obtain the exact pack and sequence, then compare
   texture ownership/retirement around costume and level changes.
3. **Complete both campaigns.** Supported private SM2 input is USA SLUS-01378.
   Track objectives, transitions, bosses, death/retry, save/load, and completion
   separately from boot screenshots or level entry. Historical 20/24 SM2
   level-entry checks are not current full gameplay coverage.
4. **Resume priority:** inspect the latest branch, checkpoint, issue evidence, and
   [community report queue](docs/issues/community-reports-2026-10-29.md).
   With supported retail input available, follow the README's extraction and
   `python tools/build.py` steps, then prioritize the normal L5A3 route above and crash cases R01/R02.
   Until then, the complete gameplay acceptance remains blocked; do not substitute
   another synthetic pass or a public-release routine probe for a level run.

## File map

- Shared runtime: `tools/RecompOne/RecompOne.Runtime/Memory/`,
  `Host/Window/HostWindow.cs`, `Assets/Suits/SuitModel.cs`,
  `Assets/LooseWadOverrides.cs`, `Diagnostics/MemGuard.cs`,
  `Gpu/Backends/Common/GlCore.cs`.
- Sewer hook/config: `spiderman/patches/Sm1AnimationBounds.cs`,
  `spiderman/tools/mkconfig.py`, `spiderman/config/spiderman.json`.
- Converter: `spiderman/tools/port_dc_character.py`,
  `dreamcast/tools/test_sm1_actor_animation_layout.py`,
  `docs/ports/dreamcast-characters-in-ps1-sm1.md`.
- Regressions: `tools/RecompOne/tests/`, `tools/run_regressions.py`,
  `.github/workflows/regressions.yml`.
- Isolated release probe and evidence: `spiderman/tools/animation_release_probe/`,
  `docs/issues/issue-5-sewer-crash.md` (no release binaries or game code committed).
- Continuity: this file, root `TODO.md`, and each game's `TO_DO.md`.
