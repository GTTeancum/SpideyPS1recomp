# Timing, audio, and crash repairs

Status: 2026-09-16. Local fixes for GitHub #5, #3, #1, and the investigated #2 audio defects are implemented and verified. Both ports enforce a 30 FPS presentation maximum with separate console IRQ delivery. The sewer crash, Chase exit order, wrapped white web strips, missing dialogue/title music, coarse Windows waits, and music delays caused by default verbose logging have concrete reproductions and passing checks below. The root [TODO](../TODO.md) retains all five issues; GitHub issues remain open pending delivery. Hardware-specific validation and unrelated mesh artifacts are listed as limits, not claimed fixed.

## Sewer collision corruption

Issue #5's exact invalid pointer (`0x0D6F0201`) was reproduced by starting in the L5A3 room and running its original trigger 92, which spawns both lizards. A write trace identifies native animation interpolation function `80074310` overwriting collision state. The old converted lizard is incorrectly counted as 37 bones because its 56 meshes include duplicated terminal LOD meshes. Its source object table and animation bank actually contain 19 bones. Retail interpolation scratch at `800A58E4` holds only 30 × 24 bytes and ends at the collision grid at `800A5BB4`.

The final repair rebuilds `lizman.psx` and `lizman2.psx` using their original 38 meshes and 19 terminal LOD links, replacing only those two entries and their manifest records in the bundled runtime assets. Their original object table, hierarchy, animation bank, and vertex ownership are preserved. A bundle-build check now rejects animated models with more terminal LODs than authored bones, and regressions check both game bundles plus an intentionally corrupted lizard.

The original bundle crashes the baseline executable at frame 1158 in the room fixture (`sewer-room-baseline-v3`). With only the repaired assets, that same executable reaches the planned frame-2200 exit (`sewer-fixed-assets-baseline`). All three captures were inspected in order and show coherent lizard attack, standing, and crouching poses. The corrected-clock build also passes (`sewer-lizard-native-lods`), with all three captures inspected. The native registry reports 19 bones instead of 37.

The earlier 64 KiB animation-scratch relocation prevented collision corruption but left the wrong animation stride. It has been removed: production uses the original native buffer with the corrected assets. The intermediate single-detail-level experiment also had geometry defects and was rejected. Historical proof folders remain available, but neither experiment is the final repair. The batch converter's report referenced the removed `far_lod_sources` variable; it now reports actual emitted mesh counts without failing after conversion.

The room fixture changes only the initial restart position and links to original trigger 92, which spawns the two lizards. Production level assets are unchanged. Its ignored generator is `proof_render/issues-1-3/make_sewer_room_fixture.py`.

The final rebuilt executable was tested with its own extracted embedded bundle (`sewer-final-bundle`), without an asset-directory override or scratch relocation. It exits normally at frame 2400; all three native captures were inspected in sequence and show coherent lizard poses during combat. Both RAM snapshots retain 19 bones and the intact 400-cell collision grid. Extracted lizard hashes match the updated bundle. The temporary level fixture was removed from the test executable's asset directory afterward. The timing/audio regressions also pass after this rebuild.

## Clock and presentation

Both ports now cap host presentation at 30 FPS and deliver separate console vblank interrupts at 60 Hz. The former implementation delivered two interrupts together at each 30 Hz presentation. Spider-Man's gameplay loop waits for two successive counter changes, so it took approximately 66 ms per update with the old clock. Separate edges restore approximately 30 gameplay updates per second.

Event-only service passes no longer render extra frames. Presentation pacing applies even when monitor vsync is enabled, and a stall cannot cause a burst of catch-up presentations. The old `SPIDEY_HZ` and `SPIDEY_VBLANK` settings no longer override the fixed cadence.

Local measurements increased Spider-Man gameplay from roughly 15 to 29 updates/s. Spider-Man 2 reached live rooftop gameplay at about 29 updates/s and presentations/s with about 58 vblank interrupts/s. These measurements include host overhead; a 30 FPS cap is not a guarantee of 30 FPS on every machine. The reporting users' hardware has not been tested.

The native counter waits (`8005E748` in SM1 and `80069124` in SM2) now yield through paced IRQ delivery instead of spinning on RAM. This exposed a second pacing conflict: the old artificial 33 ms GPU busy period sometimes demanded a third wait, lowering gameplay to approximately 22 updates/s. Both ports now disable that redundant timer. Final steady gameplay intervals measure 29.8–29.9 updates/s in SM1 and 29.8–30.0 in SM2, with matching presentation rates and approximately 60 IRQs/s. The shared optional GPU timer remains available for other ports.

### Optimization coverage for both games

Every shared runtime optimization must be checked in both ports. A game-specific hook needs an explicit review of its equivalent in the other game; one game's native run does not establish the other's behavior.

| Change | SM1 coverage | SM2 coverage |
| --- | --- | --- |
| Fixed 30 FPS presentation and separate 60 Hz console edges | Shared regression plus `pacing-final-sm1`: 29.8–29.9 gameplay updates/s | Shared regression plus `pacing-final-sm2`: 29.8–30.0 gameplay updates/s |
| High-resolution Windows frame wait | Shared timing regressions, controlled A/B benchmark, and `chase-high-resolution` native check | Same shared timer, published build, and `high-resolution-sm2` native check at 29.9 updates/presentations per second |
| Yielding retail interrupt-counter wait | Hook `8005E748`, GP+`C74`; published build and native sewer run | Hook `80069124`, GP+`CD0`; published build and native rooftop run |
| Remove redundant artificial GPU busy delay | Port initialization sets delay to zero; native sewer captures inspected | Port initialization sets delay to zero; native rooftop captures inspected |
| Shared CD sector pacing and XA buffering | Shared regressions, title waveform comparison, tutorial dialogue, Chase speech capture | Shared regressions plus `sm2-gameplay-dialogue`: captured Stan Lee introduction and complete opening Spider-Man/Beast exchange; waveform continuity not independently compared |
| SDK CD-command query used to advance dialogue | `CdLastCom` mapping retained in both function maps; native tutorial dialogue | Equivalent `80094180` mapping retained in both function maps; generated SDK binding and native opening dialogue verified |
| Wrapped off-screen particle rejection | Shared GP0 regression plus `web-wrap-fixed`: 72 malformed web strips reduced to zero and one inspected swing capture | Same regression through SM2 plus `web-wrap-sm2`: one inspected swing capture, no tall transparent strips in the trace |
| Opt-in verbose actor logging | `chase-quiet-clock` A/B and `chase-default-final` verify intended music overlap and 29.8–30 gameplay updates/s | No equivalent verbose actor hook; existing native run verifies 29.9 updates/presentations/s |
| Chase Venom building-script cadence | SM1-specific script hook selects three IRQs per update only while this script runs; lifecycle regressions pass | No equivalent Chase Venom sequence in SM2; its native counter wait remains at two IRQs per gameplay update |

The native performance runs completed normally: SM1 used 42.44 CPU seconds over 76.27 wall seconds, and SM2 used 63.50 over 96.45. These totals include startup and different scenes, so they are not a direct performance comparison between games. Both native capture sets were inspected for expected scene content and animation, beyond frame counters alone. New shared optimizations must retain both-game verification before completion.

## Windows frame-wait precision

The remaining music phrase jitter exposed another cause of low frame rate: the limiter used `Thread.Sleep` for the coarse part of each wait. On this Windows host, a requested 15 ms sleep sometimes takes 30–31 ms. A standalone harness using the actual limiter, with no rendering, reproduces approximately 21–22 FPS. This explains how low aggregate CPU/GPU utilization can coexist with poor frame rate; it is distinct from the earlier native-counter and GPU-busy repairs.

`FrameClock` now uses a private Windows high-resolution waitable timer for that coarse wait, retaining the short final spin and unchanged 30 FPS presentation cap. It does not advance extra IRQs or repay stalls with fast frames. The handle is disposed at runtime shutdown. Other platforms, older Windows versions, and timer-creation/wait failures retain the existing sleep fallback. The API is supported from Windows 10 version 1803; see [Microsoft's timer documentation](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw).

`clock-wait-ab.log` is a clean same-process A/B comparison, alternating two 120-presentation runs of the old sleep backend and the new timer, with warmup and no concurrent build/game activity:

| Backend | Run A | Run B | Presentation p95 |
| --- | --- | --- | --- |
| Thread.Sleep | 21.587/s | 21.309/s | 47.130 / 59.332 ms |
| High-resolution timer | 29.996/s | 29.996/s | 33.339 / 33.343 ms |

The fixed runs consume 0.359 and 0.328 CPU seconds over four wall seconds, including the existing final spin. An earlier fixed-only trial overlapped compilation and is excluded from the comparison. `timing-high-resolution.log` passes the IRQ-edge, counter-wait, 30 FPS cap, post-stall, Chase lifecycle, and audio regressions. Both ports were published and checked through their native process-local flows. `chase-high-resolution` exits at 6300 after 113.81 wall seconds and 65.77 CPU seconds. Its single frame-6052 capture was inspected and shows Spider-Man on the exterior wall with the HUD restored after the building script. The recorded music has 43 observed plays in the correct sequence; the final play is cut short by the planned process exit. The device trace has no post-startup restarts. Most phrase intervals are near 1.84–1.88 seconds, but one reaches approximately 2.00 seconds, so high-resolution sleeping does not by itself establish gapless music.

`high-resolution-sm2` exits at 4250 after 79.20 wall seconds and 57.14 CPU seconds. Its single frame-3870 capture was inspected: normal rooftop swinging, web rope, character, HUD, and tutorial marker remain visible, without the white strip. The steady gameplay interval reports 29.9 updates/presentations and 59.6 IRQs per second. Existing large rooftop polygons remain outside this timing repair. These game runs verify compatibility and behavior; the 21–22 to 30 FPS performance comparison above belongs specifically to the controlled limiter benchmark, not a claim that these native scenes all gained that much speed.

## Missing dialogue

Clock repair alone did not restore gameplay XA audio. Additional defects were found:

- `ReadS` continued to enter movie streaming after the movie ring was closed. It now reads gameplay XA when no movie stream is active.
- CD mute and demute commands now affect the mixed CD signal while decoding continues. A completed seek stops the old stream instead of continuing at the new location.
- Every XA sector, including filtered-out sectors, consumes disc time at 75 or 150 sectors/s. Seeking and background reading share a lock so a read cannot race the command transaction.
- Spider-Man's formerly unnamed `CdLastCom` leaf read stale retail state rather than the HLE CD-command state. Its SDK mapping now exposes the current command, allowing clip completion to advance subsequent dialogue. The mapping is preserved in `config/funcmaps/manual.json` as well as the generated map.
- Spider-Man 2 has the equivalent leaf at `80094180`, reading retail byte `800C09E1`. Its gameplay audio routine compares the result with `GetlocP` (`11`) before advancing clips. Both SM2 maps now bind this leaf to the shared SDK command state; generated code confirms that binding. The shared regression checks command queries through seek, location polling, and pause. Native capture confirms successive gameplay clips after the opening movie is skipped once.
- A one-time 2048-output-frame startup buffer (46.4 ms at 44.1 kHz) absorbs packet arrival jitter. Previously, a late sector inserted an approximately 6.5 ms discontinuity into title music. After buffering, 24 continuous seconds of native mixer output correlate at 0.9996 with the intended disc waveform. A deterministic regression also verifies every resampled output frame against the same source queued in advance, with packet delivery delayed by the observed interval.

The captured tutorial audio now transcribes the full Stan Lee introduction, Black Cat's bank/hostage explanation, Spider-Man's replies, and the subsequent hostage reminder. Three native frame captures from that run were inspected in order: helicopter/building introduction, Black Cat conversation, and rooftop gameplay with HUD. This establishes dialogue progression for that flow, not perceptual audio quality or correctness of every cutscene.

SM2's separate `sm2-gameplay-dialogue` run records Stan Lee's introduction and the complete opening Spider-Man/Beast exchange through the compass explanation and invitation to follow Beast. Only the opening FMV receives a skip input; the in-game sequence plays uninterrupted. All three renderer captures were inspected sequentially: city establishing shot, close building-facade camera position, and both characters talking on the roof. This run verifies dialogue progression, not every camera position, post-conversation control, or waveform continuity. It exits normally at frame 6200 after 109.20 seconds; steady in-game rates remain 29.8–29.9 updates and presentations/s with 59.6–59.8 IRQs/s.

### Chase dialogue timing against the original audio

`chase-audio-continuity` records CD commands and the actual submitted mixer PCM through the building sequence, with no image sequence. The game requests ten dialogue plays, and all ten are located in their corresponding recorded time windows. Their references are decoded offline from the original `COMPILED.XA` sectors selected by the native ReadS/filter commands. Matching each occurrence separately avoids falsely accepting a repeated line by finding its earlier playback.

The central 1.03–3.38 seconds of each clip correlate with the full mixed output at 0.798–0.987; concurrent SPU sound effects and clipping reduce those scores. Quarter-second comparisons retain the same sample alignment. One voiced tail has almost equally strong peaks one pitch period apart (198–200 samples); at the unchanged alignment its correlation remains 0.937 and then 0.733, so that ambiguity is not evidence of a dropout. The title segment correlates at 0.9995. After accounting for the time to reach each channel's first interleaved sector, all ten dialogue starts have wall-clock-to-PCM offsets spanning only 11.90 ms over roughly 70 seconds. The long playback interruptions reported in #3 were not reproduced in this run.

The native commands deliberately mute and seek between short voice clips. Those gaps must not be reported as decoder underruns. This analysis covers the central dialogue waveforms and their timing; it excludes 150 ms at each reference boundary and does not independently identify or validate every SPU background-music voice. Artifacts include `segments.json`, `reference-*.pcm`, `waveform-comparison.json`, the CD-command log, and the captured WAV. The process exits normally at frame 6300 after 111.45 wall seconds and 65.50 CPU seconds.

### Chase background music and device queue

`NativeAudioStateTrace` adds optional `RECOMP_AUDIO_STATE_TRACE=<jsonl path>` diagnostics shared by both ports. It records the actual OpenAL source state and processed-buffer count, plus asynchronous SPU voice snapshots and original-sample prefixes. JSON serialization and file writes run on a bounded background queue, not the mixer thread. Voice snapshots have their own timestamps because they occur after the source-state observation. This diagnostics change does not change buffering, playback gain, or scheduling.

The first synchronous diagnostic, `chase-audio-state`, recorded three post-startup queue restarts but also performed serialization on the mixer thread. It cannot establish a production underrun. The corrected `chase-audio-state-async` finishes normally at frame 6300 after 113.03 wall seconds and 68.78 CPU seconds. Its 110 source observations include only the initial startup restart; there are no later queue restarts. Asynchronous voice inspection lag is at most 4.98 ms. No images were requested or inspected for these runs.

The original executable identifies the Chase music independently of perception or filenames. Native `80062CE0` selects the level `0x501` playlist at `8009B9C8`: sound aliases 300, 301, 302, with duration entries 108, 108, 108 at `8009B9D4`. `l5a1.sfx` maps these aliases to programs 13–15; the corresponding original VAB samples are 14–16 at byte offsets 138720, 151408, and 164080. All three appear in the live SPU trace at pitch 1085 and nonzero volume. The related alert-music path uses player fields 514/518/524 and sound handles; identifying its counterpart in the [PC decompilation's StopAlertMusic](https://github.com/krystalgamer/spidey-decomp/blob/master/spidey.cpp) helped locate the PS1 code, but the playlist and sample evidence above come from this game's original executable/assets.

`check_chase_music.py` independently decodes the original ADPCM samples and applies the renderer's documented Gaussian coefficients at the observed pitch. It matches each SPU-observed play in its own time window, avoiding false acceptance from similar phrases or earlier repeats. All 42 observed plays follow 301,302,300 repeatedly, from approximately 30.52 through 107.71 seconds. One-second central waveform correlations approach 1.0 when dialogue is absent and fall as low as 0.212 in the full speech/effects mix. All 42 plays are present; this does not prove every boundary is gapless. The three references last 1.896–1.901 seconds, while observed start intervals range from 1.833 to 2.014 seconds. That leaves phrase-boundary timing as the next specific check, rather than a claim of completely verified music continuity. Retained artifacts include `states.jsonl`, `audio.wav`, `music-comparison.json`, `music-plays.json`, and `audit-summary.json`.

### Final music repair: verbose logging is opt-in

The music scheduler uses gameplay ticks, and repeated synchronous actor logs delayed those ticks enough to lose the intended phrase overlap. `chase-quiet-clock` changes only the existing quiet setting on the same high-resolution build. All 43 phrases appear in order, with PCM starts 1.817–1.881 seconds apart, shorter than the 1.896–1.901-second source phrases. Gameplay measures 29.8–30 updates/s, with 30 presentations/s through the 20-update cutscene. CPU time is 60.73 seconds over 110.45 wall seconds, versus 65.77 over 113.81 for the preceding verbose run. These totals include startup and are not a general hardware benchmark.

`GameTrace.On` now defaults to false and requires `SPIDEY_TRACE_GAME=1`; `SPIDEY_QUIET` still overrides it. Missing-overlay/fatal diagnostics and the watchdog remain enabled. SM2 has no corresponding verbose actor hook; its existing diagnostics already avoid this per-actor output. The native music scheduler and both games' simulation clocks remain intact.

`chase-default-final` verifies the published production default with neither `SPIDEY_QUIET` nor `SPIDEY_TRACE_GAME` set. It exits normally at 6300 after 111.25 wall seconds and 62.09 CPU seconds. All 43 music phrases retain the intended sequence and overlap; PCM start intervals again span 1.817–1.881 seconds. All ten dialogue clips match their original XA references at their own recorded times, with central correlations 0.795–0.987 in the full effects/music mix. Quarter-second alignment differs by at most one sample; interleave-corrected wall-to-PCM offset spread is 17.93 ms over the sequence. There are no post-startup audio-source restarts. No images were captured for the quiet/default comparisons; the changed behavior is logging and audio timing, and the preceding native scene captures are retained.

## White web strips

`chase-web-release` reproduces the reported large white vertical strip during ordinary scripted jumping/swinging, without the route follower or region-command pulses. Both captures were inspected; the later one reaches the chase failure screen and is not evidence of successful web cleanup. Correct 30 FPS timing alone does not prevent this artifact.

`web-geometry` isolates 72 tall, narrow triangles in frames 1820–1824. Their UV rectangle is `(96,208)..(120,232)`, CLUT/page 40, with constant recovered depth. The RAM snapshot contains the corresponding packet corners at Y=-1028..-1021. Signed 11-bit GP0 decoding wraps the first corner to +1020; with draw-buffer offset 256, the submitted triangle runs from +1276 to -765. A seven-pixel off-screen particle becomes a 2041-pixel strip. The modern renderer's enlarged span allowance accepts it, whereas the retail GPU's vertical span limit would reject it.

The repair retains signed 16-bit packet coordinates alongside decoded coordinates and rejects spans enlarged by wrapping when they exceed the retail axis limit. Complete, validated subpixel projections bypass this check because their host XY does not use the wrapped integers. Legitimate large saturated world polygons retain the widened span allowance, avoiding a return of the earlier widescreen floor holes. The shared GP0 regression covers positive/negative wrap on both axes, both draw buffers, ordinary visible particles, and legitimate 2047-pixel world surfaces.

`web-wrap-fixed` repeats the original SM1 inputs and frame-1824 checkpoint. Its single capture was inspected: the large white slab is absent while Spider-Man, the swing rope, HUD, and city remain visible. The equivalent trace window contains zero of the 72 malformed web-strip triangles. The process exits normally at 1900 (38.63 wall seconds, 29.19 CPU seconds). The short interval includes loading and is not a steady gameplay-rate measurement. Other visible rooftop spikes remain outside this repair.

`web-wrap-sm2` exercises rooftop jumping/swinging through SM2's native input path. Its single frame-3869 capture was inspected: Spider-Man swings with a normal rope, HUD, city background, and tutorial marker, without the white slab. The geometry window contains 14,127 triangles and zero tall, narrow transparent strips. The native span audit rejects 1,636 oversized Y spans over the run. It exits normally at 4250 (78.19 wall seconds, 55.56 CPU seconds); the preceding steady interval reports 29.8 gameplay updates/presentations and 59.5 IRQs per second. Large orange rooftop polygons remain visible; this is targeted web/pacing verification, not certification of all scene geometry.

## Verification

Run the focused console regressions from the repository root:

```powershell
dotnet run --project tools/RecompOne/tests/TimingRegression -c Release
dotnet run --project tools/RecompOne/tests/RenderingRegression -c Release
dotnet run --project tools/RecompOne/tests/Sm2RenderingRegression -c Release
python dreamcast/tools/test_bundled_actor_lods.py
```

Timing regressions pass for individual IRQ/counter edges, callback register preservation, yielding native counter waits, the 30 FPS limit, event-only service, stalls, monitor vsync, movie-to-XA transitions, seek/pause behavior, mute/demute through the actual SPU mixer, filtered-sector clock rates, and continuity across a late XA packet. Existing rendering regressions also pass.

Native game verification uses hidden game windows and process-local controller scripts. PNGs are read from the game's renderer, and optional `RECOMP_AUDIO_CAPTURE` records the PCM buffers submitted to OpenAL as a 44.1 kHz stereo WAV. Neither facility captures the desktop or sends host input. `SPIDEY_CONTROL_FILE` adds an opt-in append-only command file to the existing Spider-Man test harness; see its README for syntax.

Local proof artifacts are ignored under `proof_render/issues-1-3/`:

| Artifact | Evidence and limits |
| --- | --- |
| `timing-regression.log`, `rendering-regression.log` | Passing focused regressions. |
| `sewer-before/`, `audio-clock-only/` | Old approximately 15 Hz gameplay versus corrected approximately 29 Hz gameplay; clock-only gameplay still lacked XA. |
| `audio-command-fixed/` | Full tutorial audio WAV, transcription, and three inspected native captures. Later builds also reapply the normal costume-viewer and texture-registry generation patches. |
| `sm2-gameplay/` | Three inspected rooftop gameplay captures with HUD, character, background, and changing question-mark orientation; audio was deliberately skipped during setup and is not a full audio acceptance test. |
| `title-audio-final/` | Two inspected native captures show the Press Start screen. Several one-second PCM segments correlate above 0.999 with independently extracted title XA file 1/channel 13, starting at LBA 79439. The continuous eight-second comparison fails its 0.98 criterion (0.578): the alignment shifts by approximately 6.5 ms partway through. Music content is restored, but continuity remains under investigation. |
| `title-audio-buffered/` | Both native Press Start captures inspected. The subsequent buffering fix passes the continuous 24-second waveform comparison at 0.9996; `comparison.json` records the alignment and result. |
| `tutorial-buffered/` | All three native captures inspected in order, and the full tutorial introduction and Black Cat dialogue remain present in the audio transcription after buffering. |
| `timing-regression-final.log` | Includes yielding counter waits and exact resampled-frame comparison across delayed XA packet delivery. |
| `pacing-final-sm1/`, `pacing-final-sm2/` | Latest pacing changes verified in both games. All two SM1 sewer captures and all three SM2 rooftop captures inspected in order, showing correct scene backgrounds, HUDs, characters, and changing animation poses. SM2 movie audio was skipped during setup. |
| `sm2-audio-command-fixed/` | Unskipped SM2 opening movie; all three captures inspected in order. The planned frame-6500 exit occurs before the movie ends, so this is not gameplay dialogue acceptance. |
| `sm2-gameplay-dialogue/`, `timing-regression-sm2-command.log` | One FMV skip, then uninterrupted Stan Lee introduction and Spider-Man/Beast dialogue, retained mixer WAV/transcription and CD-command trace, three sequentially inspected native captures, normal exit, and passing command-query/timing/audio regressions. |
| `sewer-opening-before/`, `sewer-grid-before/` | Opening-area movement and a side-passage route did not reproduce the reported crash. |
| `collision-probe.log` | 20,000 queries through native collision functions against a sewer RAM snapshot did not reproduce an invalid pointer. This does not test all dynamic game states. |
| `collision-long-segments.log`, `collision-extreme-segments.log` | Two further 100,000-query runs span much longer segments. They exercise the crashing collision leaf 942,338 and 228,313 times respectively without reproducing an invalid pointer. Coverage remains limited to the saved world state. |
| `sewer-live-fixed/` | Additional process-local movement did not reproduce the crash before the harness timeout. No full-level completion claim. |
| `snapshot-verification.log` | SM1 crash snapshots now retain all 8 MB, including a seeded collision-stack value at `807FFB50`, without pumping the game. Normal snapshots use raw RAM copying to avoid triggering the memory read idle breaker. |
| `lizard-source-audit.json` | Both bundled lizard variants preserve the Dreamcast source hierarchy, uncompressed animation bank, and original full-detail vertex ownership/positions. This narrows the pose investigation but does not validate rendering. |
| `chase-building-aligned/` | All six captures inspected sequentially. Trigger 300 runs a later exterior swing, not the reported through-building chase; this run is excluded from acceptance. |
| `chase-through-building/` | Identified the actual entry as trigger 70, whose linked trigger 71 routes Spider-Man through points 54, 276, and 277, matching Venom's route. All nine captures inspected in order. The exploratory shortened-route fixture enters the scene but subsequently invokes the failure script and reaches Game Over, so it cannot establish correct exit order. The next test must preserve the chase progression/state that avoids that failure. |
| `sewer-final-bundle/`, `lizard-final-asset-audit.json`, `lod-regression-fixed.log` | Final embedded asset repair, preserved source rig/animation/ownership, correct native bone count, intact collision data, and passing bundle regression including deliberate excess-LOD rejection. |
| `chase-follow-route/` | Unmodified level triggers and Venom route, with an opt-in process-local player-position follower. All three native captures inspected in order. Venom reaches route point 14 and waits on task 15 for an actor trigger signal; native `8005BFF0` sets that signal when the corresponding trigger fires. The harness has not reached the building cutscene. Captures also show visible Venom mesh distortion. This run is diagnostic evidence, not cutscene acceptance. |
| `chase-follow-offset/` | Both captures inspected. Offsetting the teleported follower by 500 native units in X/Z fails to preserve normal traversal and returns to the loading screen. Rejected as acceptance evidence. |
| `chase-follow-pulses/` | The follower bypasses collision with invisible region planes. Invoking original region commands 290, 291, 34, 39, and 44 at the five taunt stops preserves the full Venom route and reaches original building trigger 70. Follower releases at frame 4190. All four captures inspected. Audio transcription contains Venom taunts and building dialogue; this establishes speech presence, not complete mix/continuity verification. |
| `chase-cutscene-captured/` | Corrected the capture anchor to `model.chase-cutscene`. All 19 native captures inspected sequentially, from frame 4201 through 6360. The actual building sequence runs, but Spider-Man is still inside after the camera moves outside, followed by “You lost Venom.” Visible Venom distortion remains. `positions.json` retains both characters' native states at 4200, 5000, 5600, and 6000. Source rig audit preserves both Venom variants' original bones, vertex ownership, hierarchy, and animations; runtime has 21 bones for 21 meshes. Stitch diagnostics are retained in `stitch-summary.json`. No acceptance claim: investigate whether the entry movement state from teleporting the follower contributes to the failed exit. |

## Chase cadence investigation history

### Chase cadence comparison

The first `chase-cadence-20` probe did not alter cadence: the loop makes two calls with `A0=1`, rather than a single `A0=2` call. Its six native captures were inspected in sequence and reproduce the failed exit. It is excluded from the 20-update comparison.

The corrected diagnostic extends only the main loop's second wait (`RA=8002C294`) by one genuine console IRQ. In `chase-cadence-20-corrected`, both actors' RAM states report three ticks per update. The final steady interval measures 19.8 gameplay updates/s, 29.7 presentations/s, and 59.5 IRQs/s. All six captures were inspected in sequence: Spider-Man now swings outside at frame 5875, whereas the 30-update fixture leaves him inside. At frame 6000 his position is approximately `(18152,597,7008)`, beyond the building, while Venom is farther along the chase at `(18748,255,9045)`. This changes the investigation: a cutscene-specific cadence repair is promising, but these captures do not establish Venom's exact exit order. The comparison's diagnostic remained active after the cutscene; a subsequent probe must verify restoration of 30-update gameplay.

The bounded `chase-exit-order` follow-up passes that exit-order check. All 77 native captures were inspected sequentially: entry at 4269, every six console ticks from 5548 through 5992, then gameplay at 6468. Venom exits first at 5860–5878; Spider-Man follows at 5896–5938 and swings away at 5944–5950. At 6400 both actors report two ticks per update and the player-script active flag is zero. The gameplay capture shows Spider-Man outside on the building wall with the HUD restored. This establishes order and timing recovery in this fixture, not normal player traversal of the entire chase. Venom's spiky geometry remains plainly visible. The mixer WAV is retained, but its rough automated transcript does not establish dialogue quality or uninterrupted music.

The production hook now recognizes the actual player-script initializer (`80049D54`, player in A0 and script in A1), gated by level `l5a1_t` and its distinctive command prefix. It no longer depends on the follower or a diagnostic rate flag. Completing/skipping the script, replacing the player/script, or loading/retrying a level clears the cadence selection. `timing-regression-chase.log` verifies these boundaries using real IRQ waits. The initial `chase-production` attempt missed the leading animation command and did not activate; all six captures were inspected and reproduce the failed exit. `chase-hook-trace` established the actual native script start at `800E2B3E`, including the leading `0,17,30` words. The corrected regression uses those observed bytes.

`chase-production-final` verifies the corrected production hook with no diagnostic rate flag. All three targeted captures were inspected: Venom exits at 5786, Spider-Man swings out afterward at 5862, and the HUD returns with Spider-Man outside at 6386. The native trace selects the cadence at 4186 and restores it at 5930. RAM reports three ticks during the building script and two afterward, including a subsequent unrelated script. Presentation stays near 29.9/s during the 19.9-update/s cutscene. The process exits normally at 7300 after 129.02 wall seconds and 65.31 CPU seconds. Since the fixture sends no movement after releasing the cutscene, Venom eventually gets away; this is exit-order and cadence verification, not completion of the chase. The final reporting interval crosses that transition and its negative GAME TICK delta is not a usable gameplay-rate measurement.

`chase-native-xy` disables only the test build's subpixel XY selection. Both captures were inspected; Venom's spikes remain with native integer coordinates. That diagnostic switch has been removed from source. The cause is not solely the choice between integer and subpixel raster coordinates.

The `chase-retail-venom` diagnostic replaces only the test asset directory's Venom model with the original PS1 model. All six native captures were inspected in order. The same spiky extensions appear with the retail model, and Spider-Man remains inside after the building camera moves outside. The converted Dreamcast Venom asset is therefore not the sole cause. The precise geometry source has not yet been isolated. This test stops before the later failure screen and does not verify normal chase traversal.

`chase-all-retail` extends that comparison to the entire original extracted WAD directory. The loader confirms original `spidey.psx` (287,928 bytes) and `venom2.psx` (113,344 bytes). Its single targeted capture at 2190 was inspected: Venom still has stretched triangles. The run exits normally at 2500. This narrows the cause beyond one replacement model or the combination of Dreamcast character assets; it does not identify the faulty transform, face submission, or effect code.

## Completion audit

| Requirement | Current evidence |
| --- | --- |
| Prioritize issues and record all of them in a to-do file | Root `TODO.md` contains all five currently open GitHub issues, ordered #5, #3, #1, #2, #4; SM1's old to-do points to it. |
| Repair the sewer crash | Exact original pointer failure reproduced; asset-only A/B removes the failure; final native bundle run verifies coherent lizard poses and intact collision data. Current `lod-regression-completion.log` passes both bundles and deliberate malformed-LOD rejection. |
| Enforce the overall 30 FPS cap and fix clock delivery | Both Programs force two vblanks per presentation; shared runtime clamps the minimum. `timing-high-resolution.log` passes individual IRQ edges, yielding waits, service-only behavior, cap/vsync/stall behavior, and Chase hook lifecycle. |
| Correct Chase Venom exit order | Production script hook and targeted native captures show Venom first, Spider-Man afterward, then restored gameplay timing; detailed earlier sequence/RAM evidence is retained. No diagnostic rate override is required. |
| Remove the reported white web artifact | Original normal-input reproduction, identified coordinate wrap, passing GP0 regressions in both games, and inspected native swing captures with no white slab. |
| Address low performance in both ports | Native counter yielding, redundant GPU-delay removal, separate clocks, controlled Windows wait A/B (21–22 to 29.996 FPS), and opt-in SM1 actor logging. Native SM1 and SM2 runs verify roughly 29.8–30 gameplay updates/s in their tested flows. |
| Investigate clock involvement in missing audio and repair it | Clock repair alone was insufficient; CD routing/state/pacing and XA buffering repairs pass shared regressions. Native title waveform, SM1 tutorial dialogue, SM2 opening dialogue, and final Chase music/dialogue evidence verify the reported flows. |
| Preserve both-game optimization coverage | Shared changes are checked in both ports. The Chase script and verbose actor logger are SM1-specific; SM2's equivalent wait, CD query, renderer, and native gameplay flow are covered separately. |
| Verify without desktop control and limit images | Hidden process-local scripts, native renderer PNGs, RAM/geometry dumps, SPU/mixer captures, and non-interactive commands only. Later checks use a few targeted captures or no images. |

## Verification limits

The Chase route follower establishes the authored building sequence and its exit order; it does not prove normal player traversal/completion of the entire level. Ordinary input separately verifies the white-web reproduction and repair. The reporting users' exact PCs were unavailable, so their final frame rates remain unverified; local measured results and the controlled limiter reproduction are the performance evidence. Existing Venom/building mesh distortion and large SM2 rooftop polygons are separate rendering defects and remain unresolved. SFD movie support (#4) remains deferred. GitHub issue state has not been changed by these local fixes.
