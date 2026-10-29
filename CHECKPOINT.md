# SpideyPS1 Recomp checkpoint

Updated: 2026-10-28 (Eastern Time). Resume this file when asked to continue
SpideyPS1 Recomp, then inspect the current branch and working tree.

## Objective and authorization

Optimize and standardize both games, fix demonstrated bugs, and work toward full
gameplay verification. The user authorized GitHub pushes, compilation, and test
support for `GTTeancum/SpideyPS1recomp`. Work on the existing `master` branch;
preserve unrelated changes. Use cloud tools and ordinary collaborating agents.
Do not launch Codex tasks/services or use the user's Dot or PC. Keep chat updates
within one screen. Private discs, extracted retail data, saves, credentials, and
backups must not enter public commits.

## Starting state

- Clean `master` at `b100572675d4caba3cf3162529b300e88be033f4`.
- No GitHub Actions runs existed at inspection. The shared tools target .NET 10.
- Retail data and generated game C# are absent from the clean repository. Bundled
  replacement actors and existing synthetic tests are available.
- Open reports include [sewer crash #5](https://github.com/GTTeancum/SpideyPS1recomp/issues/5)
  and [costume texture corruption #8](https://github.com/GTTeancum/SpideyPS1recomp/issues/8).
  Old `TO_DO.md` statements are historical evidence, not a current all-clear.

## Implemented in this batch

1. **Lazy debug memory.** `RamLogger` allocates its two access-history arrays only
   when the RAM map or memory editor is open and releases them when both close.
   Ordinary guest writes skip heatmap recording while retaining dispatcher
   notifications, memory guards, idle detection, and projection-depth invalidation.
   `PSMemory` creates its freeze mask on the first real freeze and releases it after
   the last unfreeze or Clear. `HostWindow` creates RAM image buffers and the GL
   texture on demand, waits for any conversion/swap before releasing buffers, and
   consumes a ready image before another worker can swap it.
2. **Custom actor validation.** `SuitModel` rejects noncontiguous/reordered texture
   records, unsupported palette sizes, missing native row/record padding, and
   unexplained trailing texture data before native loading. New fixtures retain
   valid 4-bit/8-bit odd-width textures and independent material order.
3. **Replacement texture allocation.** Removed generated mipmaps that the existing
   Linear minification setting never sampled. Level-zero filtering is unchanged;
   existing cache accounting now matches the allocated levels.
4. **Repeatable verification.** `python tools/run_regressions.py` builds the shared
   solution and runs six asset-free suites, retaining per-step logs and a JSON
   outcome. The new GitHub workflow runs that command on Windows and Linux.

The debug changes avoid **40 MiB of array payload** for the games' 8 MiB guest RAM
configuration: 16 MiB of timestamps, 16 MiB of RAM images, and an 8 MiB freeze mask.
This is checked allocation behavior, not a measured change in Windows private
bytes or frame rate. No gameplay/GPU visual acceptance is claimed.

## Actual checks

Cloud Linux, .NET SDK 10.0.403, Release configuration:

| Check | Result |
|---|---|
| Shared runtime/recompiler solution | Pass; 6 existing warnings, 0 errors |
| BundledAssetsRegression | Pass; 12 assertions |
| CustomSuitModelRegression | Pass; 24 printed cases |
| DiscInstallRegression | Pass; 17 checks |
| InteriorHookRegression | Pass; 2 synthetic execution cases |
| MemoryDiagnosticsRegression | Pass; 32 checks, 0 failures |
| VideoSetupRegression | Pass; 2 grouped check sets |
| Python native texture repeat tests | Pass; 8 tests |
| Parser before/after reproduction | Old parser accepts truncated 16-color row padding; repaired parser rejects it |
| Existing actor layout audit | 36 bundled 18-part actors / 463 texture records conform |
| Diff whitespace and workflow/runner syntax | Pass |

Run `python tools/run_regressions.py --dotnet /path/to/dotnet` to reproduce the
asset-free gate. Reports go to ignored `artifacts/regressions/`. These tests do
not boot the games, render gameplay, or demonstrate campaign completion.

An initial restricted-shell MSBuild attempt failed without a useful diagnostic;
the authorized cloud execution with network/subprocess access passed. No source
workaround was committed. GitHub Actions execution is pending the first push of
this workflow; inspect its run before recording a remote pass.

## Current blockers and next actions

- **Sewer crash investigation is active.** The log in issue #5 identifies SM1
  `L5A3`, frame 4284, after two Lizard-man spawns. Collision-grid traversal reaches
  unmapped `0x0D6F0201`; that word and a neighboring register value occur in bundled
  `lizman2.psx` animation bytes. A public release's generated IL is being inspected
  to identify the writer and any overlapping storage. Matching bytes alone do not
  establish the cause, and no sewer fix or successful level run is claimed yet.
- **Costume report remains open.** The parser flaw is reproduced, but no supplied
  failing costume pack or runtime reproduction ties it to issue #8. Existing PNG
  retirement, safe GPU texture release, and SM2 custom actor cleanup were found;
  do not describe the report as a proven leak or closed issue.
- **Full gameplay requires private retail input.** Obtain the user's supported USA
  disc/extracted data for SM1 (SLUS-00875) and SM2 (SLUS-01378), regenerate game code,
  then run actual level/progression tests. Keep those inputs out of Git. Boot/level
  screenshots must remain distinct from objectives, transitions, bosses,
  death/retry, save/load, and campaign completion.
- **Next:** finish the collision-grid writer trace and implement a reproducible
  bounded repair if supported; monitor the new Windows/Linux CI run; retain the
  exact remaining asset/verification gap at the next stopping point.

## Changed-file map

- Runtime: `Memory/RamLogger.cs`, `Memory/PSMemory.cs`, `Host/Window/HostWindow.cs`,
  `Assets/Suits/SuitModel.cs`, `Gpu/Backends/Common/GlCore.cs` under
  `tools/RecompOne/RecompOne.Runtime/`.
- Regressions: updated `CustomSuitModelRegression/Program.cs`, new
  `MemoryDiagnosticsRegression/`, and `tests/README.md` under `tools/RecompOne/`.
- Runner/CI: `tools/run_regressions.py`, `.github/workflows/regressions.yml`,
  `tools/README.md`, and an ignored local report directory in `.gitignore`.
- Continuity: this file and the pointer in root `TODO.md`.
