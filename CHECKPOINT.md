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

## Current batch: native Linux preparation and launch

The asset prerequisite is complete. Both user-supplied archives were downloaded,
passed `7z t`, and were retained privately. Independently retrieved copies matched
the originals' exact sizes and SHA-256 hashes. The private asset checkpoint holds
their identities and recovery locations; do not publish those links or the data.

- SM1 is supported USA `SLUS-00875`; extraction produced 28 disc files and 616 WAD
  records. Regeneration produced 3,314 functions, 79 patches, and 24
  reimplementations, with 17 existing residual targets.
- SM2 is supported USA Rev 1 `SLUS-01378`; extraction produced 45 disc files and
  634 WAD records / 633 unique files. The duplicate `daazeve.psx` records have
  identical payloads. Regeneration produced 3,262 functions, 34 patches, and 32
  reimplementations, with nine existing residual targets.
- The actual retail-generated games both built on Ubuntu 24.04 x64 with .NET SDK
  10.0.403. SM1 had five warnings / zero errors; SM2 had three warnings / zero
  errors. Serial MSBuild avoided the earlier parallel child-process failure.
- The source batch is published on `master` as
  [`774b3c4`](https://github.com/GTTeancum/SpideyPS1recomp/commit/774b3c4eceee1b30d5579387bec14c3facd41ec5).
  Preserve the unrelated
  untracked temporary ZIP in `spiderman/port/bundled/`. Extracted assets, generated
  C#, build output, and captures remain outside public commits.

### Changes in this batch

1. **Portable publishing.** Both game projects import shared `GamePort.props`.
   Publishing defaults to the SDK host RID, honors an explicit `-r`, and uses
   separate `port/dist/<RID>/` directories. An explicit output directory still
   wins. Ordinary build paths remain unchanged. VC++ DLLs are included only for
   `win-x64`; the three Windows validation scripts now use that RID directory.
2. **Ubuntu native-library discovery.** The old Silk resolver used
   `ubuntu.24.04-x64`, then missed the packaged `runtimes/linux-x64/native`
   directory. Native GLFW and Mesa worked, but the managed window platform was
   unavailable. `BundledNativeRuntime.Initialize` now adds a portable RID
   fallback, preserving the existing resolvers, their order, environment
   variables, and the Windows CRT loading behavior.
3. **SM2 generator parity.** Regenerating the old configuration silently dropped
   three established Video Setup/input hooks. The generator now retains all
   three and emits a configuration byte-identical to the checked-in 34-hook
   configuration. The actual regenerated game contains the three calls.
4. **Regression coverage.** Added actual MSBuild evaluation of 22 game project
   configurations and a no-display native GLFW load test. The runner now has ten
   suites; the Linux-specific native-load test reports a skip on other systems.

### Current verification

- The complete local shared-tools build and ten-suite gate passed. Logs and
  `results.json` are under ignored `artifacts/regressions/linux-portability/`.
- [Run 40886198566](https://github.com/GTTeancum/SpideyPS1recomp/actions/runs/40886198566)
  passed on Windows and Ubuntu for `774b3c4`. Both shared builds had six existing
  warnings / zero errors. Both evaluated all 22 publish configurations. Linux
  passed the six native-loader checks; Windows explicitly skipped that
  Linux-specific behavior. All remaining suites passed on both systems and
  their logs were retained as workflow artifacts.
- Publish configuration before/after: 18 failing scenarios before the fix; all
  22 pass afterward. Simulated host RIDs verify settings, not another OS's
  execution.
- Native loader before/after: old code passes four checks and fails two, including
  an actual `Glfw.GetApi()` load. Repaired code passes all six and queries GLFW
  3.4.0 with no display. The caller's explicit legacy RID remains unchanged.
- Actual default-RID SM1 publish: succeeds and emits an ELF executable under
  `port/dist/linux-x64/`. It boots under Xvfb with Mesa llvmpipe OpenGL 4.5,
  captures two visually verified title frames, and exits normally at the planned
  frame. No runtime-ID environment workaround was used for the published app.
- Normal SM1 START input visibly transitions from the title to the main menu.
  The first longer route was stopped by the optional watchdog at 11.74 vblanks/s,
  below its fixed threshold of 12. Frames were still advancing; this is not
  evidence of a game crash. Subsequent software-render runs must leave
  `SPIDEY_STALL_EXIT` unset, retain diagnostic logging, and use an outer timeout.
- The first development launch before the native resolver repair reached title
  loading without a usable window and produced no screenshots. That run is
  explicitly excluded from rendered-game acceptance.
- Both full development games were rebuilt with the resolver fix (zero errors)
  and opened OpenGL 4.5 without a runtime-ID override.

| Actual game run | Observed result | Limit |
|---|---|---|
| SM1 published title | Two correct title captures; planned exit 0 at frame 992 | Boot/title only |
| SM1 development normal new game | Title, main menu, cinematic, and L1A1 rooftop with player, world and HUD; five captures | External 360-second timeout (124) before the planned final capture/exit; no level completion |
| SM2 development normal new game | Title, main menu, E1M0 city introduction and Spider-Man/Beast scene; four captures; planned exit 0 at frame 2944 | Captured scenes are introductory cinematics; player control/HUD acceptance remains open |
| SM1 targeted L5A3 entry | Actual loader corrected `lizman2.psx` slot 8 from 37 to 19; Venom introduction and a sewer frame with player/HUD rendered; three captures; planned exit 0 at frame 1846 | Descriptor override, not campaign entry; Lizard-man spawns, player control, and level completion remain unverified |

These runs use the actual supplied disc revisions and bundled default actors.
The normal new-game runs use controller input only, with no level redirect,
cheat, replay, or artificial trigger. File-load anchors identify the scheduling
boundary; visual inspection determines what each capture actually shows.
The separate L5A3 run uses `SPIDEY_LEVEL=l5a3`, which also permits the existing
model-residency rescue path. It proves the repair hook runs against the real
loaded actor. It does not prove that either Lizard-man spawned or that animation,
collision, and level exit are correct during normal play.
The short directional-input pulse occurred during the introduction, so the later
player/HUD capture is not accepted as a movement or collision test.

Audio is routed to an OpenAL null device, so audible audio is unverified. Software
rendering is a functional test, not a hardware performance benchmark. Current
native Linux checks do not establish SteamOS/Wine/Proton compatibility or full
campaign completion. The next SM2 run must schedule captures after the remaining
introductory dialogue is dismissed; its first geometry load occurs before player
control and is not a sufficient gameplay anchor.

## Historical published baseline

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
- Retail input and regenerated game C# remain absent from the clean repository.
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

All intake entries are reported and unverified on the current build. The intake
reviewed source, issue history, and documentation; it did not build or play a game.
The prior CI and isolated-routine evidence below remain historical results for
their exact commits. The source checkout, SDK, and private retail inputs have
since been restored; the current Linux batch above supersedes the former asset
blocker.

Next: keep the L5A3 acceptance run first, then prioritize R01 and R02. The queue remains part
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

## Historical verification of the earlier source batches

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

Those historical checks did not boot either game, render gameplay, or establish
campaign completion. Full game regeneration/builds are now available as recorded
in the current batch; the isolated repair still requires gameplay acceptance.

## Remaining blockers and next action

1. **Verify the sewer fix in gameplay.** Use the prepared supported SM1 input
   (USA SLUS-00875) and rebuilt game. The targeted entry now logs the real model
   correction; next play `L5A3` through the two spawns, progression, and level exit.
   Check animation and near/distant geometry as well as crash absence. Do not
   mark issue #5 closed solely from the isolated decoder proof.
2. **Reproduce costume report #8.** The parser defect is demonstrated, but no
   supplied failing pack or selection sequence ties it to the
   [reported texture corruption](https://github.com/GTTeancum/SpideyPS1recomp/issues/8).
   Existing texture retirement and SM2 custom-actor cleanup are present; a leak
   has not been established. Obtain the exact pack and sequence, then compare
   texture ownership/retirement around costume and level changes.
3. **Complete both campaigns.** Supported private SM2 input is USA SLUS-01378.
   First extend the E1M0 menu route past the introductory dialogue and verify
   live HUD and ordinary movement. The current geometry-anchored captures occur
   too early for that acceptance.
   Track objectives, transitions, bosses, death/retry, save/load, and completion
   separately from boot screenshots or level entry. Historical 20/24 SM2
   level-entry checks are not current full gameplay coverage.
4. **Resume priority:** inspect the latest branch, checkpoint, issue evidence, and
   [community report queue](docs/issues/community-reports-2026-10-29.md).
   Assets, full Linux builds, native rendered launch checks, and the two-platform
   CI gate are available. Prioritize L5A3 acceptance and crash cases R01/R02. No tracked
   replay currently proves the L5A3 completion route. Direct level entry may help
   isolate actor loads, but must be labeled separately from normal progression;
   do not substitute another synthetic pass for a level run.

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
