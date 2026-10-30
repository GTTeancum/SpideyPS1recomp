# SpideyPS1 Recomp checkpoint

Updated: 2026-10-30 (Eastern Time). Resume this file when asked to continue
SpideyPS1 Recomp, then inspect the current branch and working tree.

## Rollback 2026-10-30: shared infrastructure batches reverted

At the user's direction, the three "shared infrastructure" batches were reverted
in one commit: `f579d39`, `32887fb`, `dcd7ce0`, `30ccdab` and `6067315` (VRAM
upload/readback/fill wrapping, input stick reset, settings null repair,
installer retry error reset, VRAM tracker wrapping, their tests, CI steps and
records). The source tree is identical to `8ce5bf7`; the default local gate
(shared build plus 11 suites) passed on Windows. The commits stay in history.

**Priority now:** fix reported bugs from the
[GitHub issue list](https://github.com/GTTeancum/SpideyPS1recomp/issues). Do not
harden or rewrite shared code that no issue points at. Reproduce first, keep each
fix minimal and tied to its issue. The SM1 sewer villain is **the Lizard** (older
records below say "Lizard-man"). Work may run locally on the user's Windows PC
at their request (checkout on a network share: use `git -c safe.directory=*`;
never stage the retail `.7z` archives in the root).

### Issue batch 2026-10-30 (no level traversal)

Local setup: discs at the repo root (ignored), private work under `private/`
(excluded via `.git/info/exclude`), launcher `private/run-hidden.sh` (hidden
window, OpenAL null device). The file server locks `.exe` files after they run:
build games with `-p:OutDir=bin/Release/net10.0-b/` and run the gate with
`UseAppHost=false`.

- **#15 fixed** (`4f9f3c7`): SM2 Costume Viewer narration. ReadS XA without a
  movie stream was ignored, `CdLastCom` (0x80094180) was unmapped, and seeks
  did not end XA reads. Now one clip per selection, stopped by the game.
- **#8 not reproduced in the viewer:** 30 consecutive v1.1 suits rendered
  correctly. In-level suit check not yet done.
- **Suit limit fixed** (`be32fb2`): the v1.1 packs (93 suits) overflowed the
  60-row selectors, so ~50 suits per game were rejected at startup. Both games
  now allow 128 rows; SM2 registers 91 and SM1 90 of the release suits (two
  models exceed the separate 1 MiB limit). Last suit verified in both viewers.
- Runs: `.exe` files and `dotnet Game.dll` both fail here (server lock; memory
  cards beside the launcher). Build with `UseAppHost=false` into
  `bin/Release/net10.0-c/`; `private/run-hidden.sh` copies a fresh apphost per run.
- **#17 SM2:** stock Symbiote sets ability ID 3 and switch 0x800C2218 in play
  (snapshots at 9900/10500/11500). Web-firing input in E1M0 was inconclusive.
  SM1 (the likelier report target) not yet tested.
- **#12, #14:** not started.

**Build and test locally only** (`python tools/run_regressions.py` on the
user's PC). Do not use or wait on GitHub Actions as verification; this
supersedes the cloud-tool and CI instructions in the sections below. The
"Asset-free regressions" workflow is disabled in GitHub's settings (the file is
unchanged); re-enable only if the user asks:
`gh workflow enable "Asset-free regressions" -R GTTeancum/SpideyPS1recomp`.

## Objective and authorization

Optimize and standardize both games, fix demonstrated bugs, and work toward full
gameplay verification. The user authorized GitHub pushes, compilation, and test
support for `GTTeancum/SpideyPS1recomp`. Work on the existing `master` branch;
preserve unrelated changes. Use cloud tools and ordinary collaborating agents.
Do not launch Codex tasks/services or use the user's Dot or PC. Keep chat updates
within one screen. Private discs, extracted retail data, saves, credentials, and
backups must not enter public commits.

## Current batch: SM2 targeted-entry repair; #10 and #11 remain open

Published game-source fix:
[`b0d2f90`](https://github.com/GTTeancum/SpideyPS1recomp/commit/b0d2f90d75d3864886aba9db13dbfc4d51793252).
The SM2 level override previously loaded E1M3/E1M4 resources while retaining
E1M0's tutorial descriptor and flags. Both original 4200 snapshots demonstrated
that mismatch. The fix selects the supported native campaign descriptor at the
initial stage-loader lookup, bounds the name write, and leaves subsequent native
transitions and non-campaign fallback intact. It does not establish the cause
or resolution of the community's reported Shocker crash.

Actual verification:

- Full retail-generated SM2 Linux build: **three warnings, zero errors**.
- Focused actual-hook regression: **28 checks passed**, using synthetic bounded
  storage, not gameplay. Generator/config output matched and independent native
  source review passed.
- [Windows and Ubuntu asset-free CI](https://github.com/GTTeancum/SpideyPS1recomp/actions/runs/40946682539)
  passed for the published source. Windows gameplay was not performed.
- Corrected E1M3: all 12 snapshots showed `e1m3_t` / flags `0x000E0001`.
  Three ordinary Square taps damaged thug record 61: **50 -> 42 -> 34 -> 26 HP**.
  No defeat was established. Game Over appeared by 4800; configured `SPIDEY_EXIT=5000` returned
  **0 after 155.13 s**, with 41 retained captures.
- Corrected E1M4: all 32 snapshots showed `e1m4_t` / flags `0x000E0009`.
  The boss encounter rendered and Right input was delivered at 3600–3720.
  A native failure newspaper was visible at 9840; configured `SPIDEY_EXIT=12000` returned
  **0 after 274.60 s**, with 100 retained captures. Retry was not selected.
- Neither corrected run showed an observed crash or fired watchdog. The
  ordinary E1M3 completion/cutscene -> E1M4 transition remains untested. See the
  [corrected #11 record](docs/issues/issue-11-shocker-crash.md).

For #10, the armor actor was present and enabled in all 120 retained L8A3
snapshots. The closest sample's native contact metric was **287**, while
collection requires **<128**; it was still outside pickup range. Two further
ordinary-input attempts also failed to reach armor: a compressed route fell and
retried (126.48 s), and a replay preserving recorded button timing diverged
during climbing/damage (241.57 s, final sampled HP 10). Both were deliberately
stopped, not crashed. There are now seven unsuccessful pickup attempts, no
executed armor-crash reproduction, and no armor fix. See the
[#10 follow-up](docs/issues/issue-10-armor-crash.md).

**Next action:** advance the observed native E1M4 failure screen to Retry using
ordinary inputs, then target the actual E1M3-to-E1M4 completion boundary with a
supported nearby save or verified combat route. Source tracing identifies
14 initial thug defeats followed by one final thug selected from four possible
positions; the latest run established damage, not any defeat or completion.
For #10, obtain a verified pickup route or supported nearby save before another
attempt; the retained L8A3 recordings are not deterministic pickup replays.
All game processes from this batch have stopped. Keep #5 closed for the next
release; keep #10 and #11 open. No new release was published.

## Previous batch: initial issue #10 and issue #11 screens

The user explicitly selected **#10 (armor pickup)** and **#11 (before Shocker)**.
Both remain open. This batch restored the private inputs and Linux tools,
investigated the actual native paths, and ran bounded gameplay screens. It did
not reproduce either reported crash or change game behavior.

- **#10:** The retail armor texture swap has a concrete compatibility hazard:
  converted player texture hashes are absent from its fixed retail map, and
  a failed lookup can leave destination pointers unwritten. This is a static
  source/data finding, not an executed reproduction of the reported crash.
  Five ordinary-input attempts in L8A3/L8A1 did not establish an armor pickup.
  The closest L8A3 sample was about 256 units from the pickup with armor inactive.
  The final player-only L8A1 test exited normally at 12000 vblanks with armor
  inactive. No armor fix was implemented or verified. See the
  [issue #10 record](docs/issues/issue-10-armor-crash.md).
- **#11:** Supported trigger data maps the route to **E1M3 → E1M4 (Shocker)**.
  Direct E1M4 entry loaded the boss, rendered the encounter, accepted movement,
  and reached Game Over before the configured exit at 6000 vblanks (155.15 s,
  exit 0). Direct E1M3 entry loaded the preceding stage and accepted movement,
  jump, and pause; the operator stopped it at 4440 vblanks (108.07 s, SIGTERM
  `-15`). Neither screen reproduced a crash. The actual inter-stage transition
  remains untested. Later analysis established that these initial screens used
  E1M0 tutorial metadata with the target assets; their acceptance scope is
  corrected in the [issue #11 record](docs/issues/issue-11-shocker-crash.md).
- **Environment:** Both retained archive SHA-256 values matched the originals.
  Extraction restored 28 SM1 and 45 SM2 disc files and their WAD entries. Both
  retail-generated Linux builds of `892df80` succeeded with zero errors (SM1:
  five warnings; SM2: three). .NET 10.0.403 and cloud Xvfb/Mesa are available.
  All game processes from this batch have stopped. No new release was published.
- **Observation limit:** Live input-journal observations appeared stale while
  captures continued. Completed run metadata retained later successful input
  releases and timely stop handling, so the persistent-stall hypothesis was
  withdrawn. No blocked XSync or failed safety loop is established.
  Prefer original captures, completed snapshots, and final run metadata over a
  stale journal alone. No controller replacement was made.

## Current focus: short reproductions with visible evidence

The user changed the immediate priority after reviewing the sewer test: focus on
things that can be reproduced easily and make the evidence visible in chat.
This supersedes the earlier instruction to finish L5A3 before other work.

- Choose a short command or a small number of ordinary menu/gameplay steps with
  a clear expected result. Confirm the failing behavior before changing it.
- Prefer a bounded reproduction in startup, asset loading, conversion, texture
  parsing, or accessible menus over lengthy level navigation. An unverified
  community report is a screening candidate, not an established easy bug.
- For each repaired behavior, retain the exact build/setup/steps and a failing
  before result plus the corresponding after result. Label isolated tests,
  source findings, and full-game observations separately.
- Show relevant original screenshots or a short capture directly in chat for
  visual/gameplay claims. Include concise command/output evidence for faults
  that are not visual. Repeated green regressions are not new bug progress.
- Keep unsuccessful screening bounded. Record what was attempted and move to
  another concrete case if no failure is reproduced; do not spend the next
  batch extending a navigation harness just to reach a difficult report.

**Issue #5 is closed as resolved for the next release, per the user's decision.**
The [resolution comment](https://github.com/GTTeancum/SpideyPS1recomp/issues/5#issuecomment-6503366323)
records the committed repair and the unchanged verification limits. Its exact
buffer overwrite was reproduced in an isolated released-code probe. The later
patched game reached combat, damage, death, and Retry. There has been no paired
unpatched gameplay crash followed by the same successful patched route, and no
level completion. The saved captures demonstrate patched gameplay only.

The user has now selected #10/#11 for the current batch above; their severity
does not establish reproduction ease. The observed-once log cessation in #19
also remains tracked. #17
is only a possible short ability check once the game and native stock slot are
confirmed. A clean SM1 profile does not unlock stock symbiote slot 2, and
`SPIDEY_CHEATS=all` would invalidate an infinite-webbing check by enabling that
cheat. Do not claim it is immediately reproducible from a clean boot.

The original evidence archive and four selected frames were recovered after
workspace maintenance and matched their previously saved SHA-256 values. This
priority/evidence update did not run either game, rebuild code, or claim a new
reproduction. The current batch above subsequently restored the local test
prerequisites; the private checkpoint records their paths and retained inputs.

### Previous tracking update: issue #5 closed for the next release

The issue body now records **resolved for the next release**, and GitHub was
read back as `closed` with state reason `completed`. The resolution comment
and body matched the intended content. This update did not run gameplay,
rebuild code, or publish a release. The repair is already on `master`; no
upcoming version number was assigned.

The live GitHub inventory after closure contains **14 open issues**:
**#4, #6, and #8–#19**, with **no open pull requests**. Community reports remain
awaiting reproduction; #19 remains an observed-once diagnostic failure. Broader
L5A3 acceptance is unfinished but is no longer a condition keeping #5 open.
Continue with the short-reproduction focus above; restore only the prerequisites
needed for the selected case. `git diff --check` passed for this documentation
update. No new runtime checks were performed.

## Most recent gameplay batch: targeted L5A3 gameplay and test control

The two bounded Linux runs now extend verification into **actual sewer gameplay**.
The tested game/runtime source remains
[`774b3c4`](https://github.com/GTTeancum/SpideyPS1recomp/commit/774b3c4eceee1b30d5579387bec14c3facd41ec5);
this batch changes documentation and private test helpers, not the game binary.
The intervening root README rename in `e92dfb3` was preserved.

- Ordinary movement, jumping, wall climbing, pause, and Restart Level were
  exercised in the first run. The second reached the first Lizard-man pair and
  visibly exercised enemy movement, attack/grab animation, and combat.
- The real loader corrected slot 8 from **37 to 19** animation parts at vblank
  1234. The nearby pair spawned at 4608; two further spawns at 4616 are distinct
  distant pipe guards, not a duplicate-spawn finding.
- Saved snapshots identify four live linked Lizard-men. During combat, one
  nearby actor's health fell **120 to 100** and the player's **100 to 12**.
  The cached animation count remained 19 and all eight sampled collision-grid
  words remained unchanged; the historical corrupt pair was absent.
- Captures show a Lizard-man at 5184, a grab at 6240, Game Over at 6720, and live
  play again at 8640 after ordinary Retry. No enemy kill or level exit was
  verified. Health is signed 16-bit actor offset `+0xDE`; `+0x21A` is a geometric
  offset and must not be used as health evidence.
- Run 1 ended by requested test stop after 1,091.32 seconds; run 2 reached its
  1,800-second outer timeout. Both child exit codes were `-15` from the wrapper's
  SIGTERM, not a demonstrated game crash or normal game exit. Both original
  save/settings hash comparisons passed. All game processes are stopped.
- A private controller defect blocked further input on the retry: the log-based
  progress reader stopped advancing while captures and snapshots continued.
  Completed saved-artifact fallback, log truncation/rotation recovery, and
  monotonic observations passed **15 focused checks**, including the real
  stopped-log case. The running wrapper did not receive that repair; a fresh
  run is required to verify its live control behavior. The cause of log silence
  remains unestablished.

Both game READMEs now distinguish the default **30 Hz presentation / 60 Hz
vblank clock**, script start offsets, VSync-callback hold counts, and actual
visual gameplay acceptance. `menu.spidey` labels alone are not gameplay proof.
No rebuild or shared CI rerun was needed for this documentation batch; the
earlier source-specific ten-suite Windows/Ubuntu gate remains historical evidence.

See [the gameplay record](docs/testing/linux-gameplay-2026-10-29.md) and
[updated issue #5 evidence](docs/issues/issue-5-sewer-crash.md). Logs, 414 captures,
input journals, snapshot summaries, exact helper versions, and audit results
are retained privately. Retail assets and raw snapshots are not public evidence.

**Deferred L5A3 acceptance route:** start a fresh bounded run with the repaired controller, handle the
first pair, cross the pipe section using normal jump/swing input, and verify the
far encounter and `L5A4_T` transition. The mapped route is guidance, not a
completed replay. Targeted `SPIDEY_LEVEL=l5a3` also permits the existing model
residency rescue; normal campaign entry remains a separate acceptance gate.
Issue #5 is now closed for the next release; this route remains broader gameplay
acceptance. SM2 live control, both campaigns, audible audio, save/load,
and the community reproduction queue remain incomplete.

### GitHub tracking completed in this batch

The user requested proper GitHub tracking for the outstanding findings. All
ten community intake targets now have issue coverage: **R01–R04 are #10–#13,
R05 remains #8, and R06–R10 are #14–#18**. The observed native Linux log cessation
has its own [issue #19](https://github.com/GTTeancum/SpideyPS1recomp/issues/19).
The [community tracker](docs/issues/community-reports-2026-10-29.md) maps each
report to its canonical issue and preserves the reproduction/closure criteria.

Existing issues received evidence updates rather than duplicates:
[sewer #5](https://github.com/GTTeancum/SpideyPS1recomp/issues/5#issuecomment-6501565567),
[suit textures #8](https://github.com/GTTeancum/SpideyPS1recomp/issues/8#issuecomment-6501565969),
and [Linux support #6](https://github.com/GTTeancum/SpideyPS1recomp/issues/6#issuecomment-6501566564).
All ten new issue bodies, open states, and bug labels, plus the three comments,
were read back and matched the intended content. No issue was closed in that
tracking batch; #5 was subsequently closed as recorded above.
Community symptoms remain reported/unreproduced; #19 is an observed-once
diagnostic failure with an unknown cause. The private controller workaround is
not a game-logging fix. The former queue prioritized ordinary armor
pickup reproduction [#10](https://github.com/GTTeancum/SpideyPS1recomp/issues/10)
and the SM2 pre-Shocker report
[#11](https://github.com/GTTeancum/SpideyPS1recomp/issues/11); use the current
reproduction-first focus above when choosing the next bounded task.

## Previous batch: native Linux preparation and launch

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

Reviewed the indexed v1.1 release discussion and compared it with the nine GitHub
issues present at intake time and the existing checkpoint/TODOs. The
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

The later tracking batch above created the missing GitHub issues. The current
focus now favors short reproductions rather than automatically putting L5A3,
R01/#10, or R02/#11 first. The queue remains part
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
in the previous Linux preparation batch. The current targeted run adds actual
spawn/combat/death/retry evidence; level completion and normal campaign entry
still require gameplay acceptance.

## Remaining acceptance backlog

The numbering below identifies unfinished areas, not their execution order.
Apply the current short-reproduction focus above before choosing work.

1. **Complete sewer gameplay acceptance.** Use the prepared supported SM1 input
   (USA SLUS-00875), unchanged rebuilt game, and repaired private controller in a
   fresh bounded run. Load correction, first-pair spawning, live combat/damage,
   death, and Retry are now observed. Enemy defeat, the pipe traversal, the far
   encounter, level exit, and normal campaign entry remain open. Check animation
   and near/distant geometry as well as crash absence. This is broader gameplay
   acceptance; issue #5 is closed as resolved for the next release.
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
   Private inputs and targeted sewer evidence are retained; earlier full Linux
   builds and the two-platform CI gate are recorded above. Restore only the
   local prerequisites required for the selected short reproduction. No tracked
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
- Current gameplay scope and run evidence: `docs/testing/linux-gameplay-2026-10-29.md`.
