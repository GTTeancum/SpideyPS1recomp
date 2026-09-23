# Performance repairs — September 17, 2026

Implementation is in progress. This is the durable handoff for both games;
the [assessment](performance-assessment-2026-09-17.md) remains historical
before-change evidence. Do not mark cross-hardware performance or ordinary
Chase completion verified from a hidden scripted run.

## User correction: excessive elapsed time

User reports nine hours elapsed. Repeated ordinary-input Chase route tuning has
consumed too much time without proving full completion. Do not continue the same
sequence of speculative replay adjustments as if each were a game fix. Current
verified full ZIP remains OpenSpidey-Gateway-Chase-Diagnostics-2026-09-18.zip.
The last west-building-corner-normal run ended with all owned exits0. Existing
RAM review shows it passed the corner to(11304,1475,10318) at3139, climbed
and reached(10705,898,10663) at3229. LostVenom script800E4CAC then fires
atframe8644; full chase failed.
3719 positive-counter presentations,max48.0189ms,none>100ms,0audio recoveries
after5s; last audio observation179.345s. No additional run/build was launched
while the explicit pause decision is pending. No diagnostic game remains intentionally running.
Unfinished audio log rotation experiment was removed; production binaries/ZIP
unchanged. Audio-state telemetry still has a180-second limit; disclose this gap
for sustained acceptance. Frame/process logs remain bounded and continue.
Full Chase completion and external hardware acceptance remain unproven. Prior
Normal-priority comparison changes interpretation of diagnostic stalls, not a
claim that every user report has been fixed. Reassess return on further automated
work with the user before resuming repetitive route trials.

## Current status - September 18; read before chronological evidence

P0/P1 remain open. Earlier pending statements below are historical; TODO.md is
remaining scope. User prefers local resolution before laptop testing. Run muted,
with real mixing/queues enabled, no desktop control, few native images, one game
at a time. Never discard failing runs or claim 99% cross-hardware confidence.

- **Current Chase handoff:** next untested fixture is
  native-chase-west-building-corner-script.txt. The3040jump now reaches
  (13520,1248,11311) at3090 but Up climbs mesh75 and losesVenom by3190.
  Original wall ends nearby at(13389,11279); mesh topY-2633 makes climbing an
  unsuitable route. Test ordinary Left around the end, then UpLeft jump/swing;
  do not edit original collision. Exact source/RAM reasoning and proposed
  snapshot offsets are at the top of docs/chase-normal-route.md.
  Latest chase-intermediate-roof-jump-normal:190.187s,allownedexits0,
  gameplaymax40.1513ms,0audio recoveriesafter5s,20747events/lost0;hostmax61%,
  queue0. Full route still fails. All games/collectors stopped at this handoff.

- **New priority isolation evidence (later September18):** diagnostic runner now
  offers `--game-priority normal`, setting NORMAL_PRIORITY_CLASS at creation for
  only its own game child. Default remains inherit; production binaries/ZIP,
  parent processes, collectors, other processes and host power/affinity unchanged.
  SM1 matched lower-roof fixtures use identical EXE/input/snapshots/witnesses;
  all8 sampled player states/counters equal. BelowNormal/Normal gameplay maxima
  33.9059/33.5601ms,0recoveriesafter5s, but hostmax50.05/57.29%,queue0: quiet
  runs do not settle saturation behavior. See sm1-normal-priority-comparison.json.

  SM2 Normal-priority movie/rooftop run106.5s,allownedexits0,19025events/lost0:
  1440movie presentations max80.612ms,614positivecounter max58.2822ms,
  no intervals>100ms in either group,0audio recoveriesafter5s. Host reaches100%,
  12samples>=90%,queueMax52. Critically, during a1221.007ms BelowNormal heartbeat
  delay, the Normal game continues36positivecounter presentations,max38.7319ms;
  host100%,queue39. During another1139.897ms witness delay,31gameplay frames
  continue,max58.2822ms. This disproves treating that witness's long gaps as a
  universal host stop and strengthens inherited-priority confounding of earlier
  game stalls. It is not a controlled matched-load causal A/B or proof of all
  historical/user failures. No new images/aural/combat-hit acceptance. Exact
  overlap: sm2-normal-priority-movie-rooftop/priority-pressure-overlap-review.json.
  Keep previous failures and full P0/P1 acceptance requirements. No ZIP rebuild
  needed: these tests use the already packaged binaries unchanged.

- **Latest retained failure: chase-raised-roof-jump-witness.**
  Canonical priority-logging SM1 completed204.625s;all owned exits0.
  3562 positive-counter presentations,13>100ms,max1537.0901ms;48 audio
  recoveries after5s.20688 runtime events,lost0. At1537ms,counter2352,
  1535.694ms is after the render callback; hostCPU100%,queues48/39,
  shared heartbeat delays, non-CPU lower estimate1162.09ms, full trace with
  no managed suspension/contention. Later menu2360.8123ms is inside callback,
  CPU98.4-100%,queues40/29/18,non-CPU lower estimate2142.062ms,no managed
  suspension/contention. These reproduce failures after the earlier clean runs;
  they do not negate the seam repair or establish a new game-clock defect.
  Exact evidence: largest-stalls-review.json and full witnesses in that run.
  Ordinary route jumps off raised contact but lands, then walks too long and
  loses Venom. Next untested ordinary-input fixture jumps at2980 instead3068:
  native-chase-lower-roof-relaunch-script.txt. Full chase and P0/P1 remain open.
  No games/collectors left running; no new images. Do not request laptop testing
  yet: user prefers local resolution first. Next work is route relaunch and
  unresolved normal-launch/sustained behavior acceptance, not another duplicate
  binary build or repetition of the same host-load attribution alone.

- **Current full private ZIP:**
  `proof_render/OpenSpidey-Gateway-Chase-Diagnostics-2026-09-18.zip`,
  1,090,938,879bytes,2074 manifest files verified. SHA256
  `5BF46AD12E951A2B86BC5C334C291EC81E894A3A6DBE8FF71175114BFA54267E`.
  Both games/data/movies/audio included, ordinary Chase launcher,30FPS.
  Only both EXEs differ from SeamFix ZIP; all other bytes/launchers identical.
  Exact EXEs validated natively; no new extracted-ZIP game run. Verification:
  `priority-log-bundle-verification.json`. No immediate laptop test request.

- **Latest completed work: priority logging in both production builds.**
  `PerformanceLog.cs` now writes read-only Windows process priority in system
  metadata and per-second process samples. Unsupported/unavailable queries return
  null without stopping logging. No priority, affinity, or power setting changes.
  The existing performance/profile regression passed. Both ReadyToRun publishes
  succeeded; `priority-log-validation.json` matches all168 SM1 and55 SM2 samples
  to independent Windows BelowNormal observations. Exact candidate EXEs were
  promoted; previous seam-fix binaries remain in the partial-corners folders.
  SM1 max positive-counter interval48.7326ms, none>100ms, one audio recovery at
  7.320s (later Playing). SM2 bounded58.406s logger check:495 positive-counter
  presentations, max41.1095ms, none>100ms, no recoveries after5s. This adds no new
  visual/aural/combat acceptance. The retained SM1 recovery maps to counter0
  before gameplay; nearby presentation maximum78.8236ms and independent
  heartbeat delays61.422/55.678ms. The eight256-frame audio buffers hold46.44ms
  at44100Hz. No mixer scheduling/refill trace proves its exact onset or cause;
  do not infer continuous audio merely from all gameplay frames being<100ms.
  Evidence: chase-west-heading-priority-witness/audio-recovery-review.json.
  Full Chase still fails after the building;
  see `docs/chase-normal-route.md`. No images generated this iteration.

- **Host-load comparison:** `priority-host-load-comparison.json` preserves an
  observational comparison. Clean `chase-south-wall-end-witness` had median host
  CPU8.29%,max30.11%,zero sampled queue, gameplay max37.6878ms and no recoveries
  after5s. The failing west-wall-jump and SM2 movie/rooftop runs reached~100%
  CPU with queue maxima69/36 and long stalls. Routes/builds differ; this is not
  a controlled A/B or proof covering historical reports. Inherited BelowNormal
  priority makes our hidden tests susceptible to Normal-priority worker load.
  Preserve captured failures and verify normal launches before generalizing.

- **Diagnostic scheduling caveat (September 18):** during
  `chase-west-relaunch-witness`, read-only inspection found the game, runner,
  Codex/ChatGPT ancestor chain and all four diagnostic collectors at Windows
  Below Normal priority; observed MSBuild workers were Normal. Independent
  heartbeat processes therefore were not independent of inherited priority.
  This limits generalization from diagnostic-host stalls to normal launches.
  Older runs did not record priority: preserve their failures without asserting
  historical priority. Exact PIDs and limits are in that run's
  `priority-observation-review.json`. Future native runs write read-only
  `process-priorities.json`; the bounded process collector records priority at
  both CPU-sample endpoints. No process priority, affinity or host power policy
  was changed. The shipping ZIP is unchanged; this is runner-only metadata.


- **Latest priority-witnessed recurrence:** `chase-west-wall-jump-witness`
  reproduced21 positive-counter intervals>100ms, maximum1657.6732ms;52audio
  recoveries after5s. This failure supersedes any clean-run implication above.
  At1657ms, hostCPU97.7-100%,queues67/25; sampled Normal-priority cl.exe workers
  used multiple cores while the game remained Below Normal. Heartbeat overlap
  1587ms,non-CPU wall lower estimate1220ms,no managed suspension/contention.
  At1192ms,900ms was after the render callback; heartbeat overlap1140ms.
  Latermenu2611ms was measured mostly inside the limiter, with2599ms matching
  heartbeat delay and2533ms non-CPU lower estimate. These differing measured
  locations support process descheduling, not a single proven rendering or
  limiter defect. Full trace coverage;19840events,lost0. See
  priority-stall-review.json for exact evidence and attribution limits.
  Process collector cost1.6875CPU seconds over189seconds;all owned processes ended.
  Three existing collector tests pass; broad unittest discovery hit unrelated
  missing Blender bpy, so the exact collector test file was run successfully.
  No new production binary or ZIP was made; no images generated this iteration.

- **Current wall seam reproduced and narrowed:** `chase-current-wall-seam`
  uses canonical SM1 hash83FAC5... and native-chase-wall-seam-script.txt.
  Its single inspected frame03946 shows the broken yellow/dark horizontal seam.
  `chase-current-wall-seam-solid` repeats the pose with existing diagnostic
  palette3104 drawn untextured opaque white; its sole inspected image retains
  the same gaps. Thus this occurrence is not a texture cutout. Forward geometry
  frame3943 has opposing V0/V254 boundary endpoints(38.070507,81.78674,Z986.7947)
  versus(37.81868,81.65112,Z986), both native(37,81). Other segment endpoints also
  differ. Read seam-edge-review.json and both visual-review.json files.
  The shared all-three-corners validity gate caused visible edge precision to
  fall back when an unrelated corner was unavailable; the repair below proves
  the concrete seam case. Original asset-face mapping is not a removal license.
  Gte.Rtp's homogeneous host projection retains transform fractions, whereas
  rounded camera-space reprojection may not. Trace source frame3941 corresponds
  to submitted frame3943 here. Do not inflate all triangles or remove scenery.
  Both image runs exited/cleaned up; only two images produced and both inspected.
- **Ordinary post-script route branch rejected:** delayed R2 after wallCross
  avoids immediate state40000 reacquisition but drops to the lower roof and
  still loses Venom. Read chase-post-building-attachment-source.json and route
  handoff. No production timing/movement change justified. That fully witnessed
  run retains10 positive-counter intervals>100ms,max1520.2595ms,21audio recoveries
  after5s. Largest stall has full heartbeat overlap,hostCPU96.2%,no coveredGC or
  mainJIT,non-CPU lower estimate1332.76ms. All collectors exited;20486events,lost0.

- **Shared seam repair (promoted):** WorldSubpixel.SetSubdivisionCorners
  previously discarded all three source projections if any one was unavailable.
  Both games share this file. The candidate records valid corners individually
  and preserves an edge when its nonzero barycentric contributors are known;
  points requiring an unknown contributor keep native fallback. It also removes
  the small temporary address-array allocation from every corner setup.
  New native-routine regression fails before the change; both SM1/SM2 rendering
  regressions now exit0 with49PASS lines each, including unchanged packet words
  and CPU registers plus no stale-corner reuse. See seam-partial-corners-*.log
  and seam-partial-corners-candidate.json. SM1 candidate802DA2... has now run:
  chase-partial-corners-wall-seam exits0; sole native image inspected, broken
  wall seam absent, textured wall/player/sky/HUD present. Five V0 boundary edges
  in candidate submittedframe3943 match baselineframe3945 exactly inXYZ; all
  five opposingV254 edges now match too, while baseline edges differed.
  The image pose differs by two console ticks; do not claim pixel-identical
  screenshot comparison. Exact aligned evidence:that run's seam-repair-review.json.
  SM2 publish/native rooftop check also completed; candidates are now canonical.
  SM1 SHA802DA2FEE2225469F4E83F1629675BF1FEB948EA398082B587E194B8D8BFC4AD;
  SM2 SHAC0DB780030CCBD14F9069D922595D6EF3DF5621C253EDA6B41D3A2E6B4056EB6.
  Old canonical EXEs remain in *-callback-boundaries; promotion hashes verified.
  Full OpenSpidey-Gateway-Chase-SeamFix-2026-09-18.zip now verified:
  2074files,1090933381bytes,SHA BBC4E10FC4EBDFB855EAE7694DA8AA9D899D28C64251EB7274BBB5F714B226E1.
  Only the two EXEs changed from the previous full verified ZIP. Launchers/data
  are identical; no additional extracted-ZIP game run this iteration. Exact EXEs
  exercised in the candidate runs above. No laptop testing requested yet.

- **SM2 native check of shared seam repair:** sm2-partial-corners-movie-rooftop
  completed5600ticks112.297s,all five owned exits0. Sole native image inspected:
  textured rooftop/city/player/help marker/shadows/HUD present.1437movie and616
  positive-counter presentations.16intervals>100ms,22audio recoveries after5s
  retained: movie1163.6372ms and gameplay612.0557ms both fully traced,noGC/mainJIT.
  Heartbeat overlaps1160/570ms,hostCPU93.6/99.8%,Normal-priority compiler workers
  consume multiple cores while game staysBelowNormal. This reinforces diagnostic
  scheduling pressure in BOTH games, not proof of all user/historical failures.
  See review-summary.json/largest-stalls-review.json;17509events,lost0.
  One rooftop image does not prove all movie frames, combat hits, costumes,
  sustained or audible correctness. No P0/full-Chase/hardware acceptance claim.

- **Verified local repairs:** independent30FPS presentation/60Hz console phase;
  hot-path, GL33 copy and vertex-stream changes; bounded async logging; OpenAL
  queue rebuild after underrun; movie lock/worker lifetime fixes; SM1 CdControlB
  binding; publish-time ReadyToRun removes measured first-update JIT hitch in
  BOTH games. Latest tests:332 timing/XA/input;72 movie streaming assertions.
  Active StSetRing/StClearRing/StSetStream now retire the producer before reset.
- **Newest witnessed P0 recurrence:** chase-camera-aligned-witness captured
  1713/2618/906/1976ms moving stalls with complete runtime/host coverage. Host
  CPU92-100%, queues up to71, multi-core compiler load, independent heartbeat
  delays and low game CPU strongly implicate host scheduling pressure for THIS
  recurrence. Largest prior after-render-callback interval2514ms vs callback0.222ms;
  no covered GC or main-thread JIT.7audio underruns all recover and later play.
  This does not attribute unwitnessed historical failures or prove other hardware.
  Source data and limits: that run's witness-review.json and final section below.

- **P0 evidence:** retain chase-input-phase-b (2069ms moving interval), historical
  sm1-present-phase-stalls, sm1-stall-correlation and sm2-stall-correlation. Some
  later recurrences correlate with host saturation; that is not attribution of
  every original stall. sm1-ring-transition-movies has840/467ms gameplay gaps,
  mostly prior render dispatch outside callback. sm2-ring-transition-movies has
  up to894ms movie gaps, mostly outside-present. Neither has full host/GC/JIT
  witnesses. Witnessed repeats sm1-ring-movie-witness/sm2-ring-movie-witness are
  clean (max56/75ms), which does NOT erase the failures or establish their cause.
- **Audio:** both failing movie runs logged15 underruns after5seconds. All15 in
  each have recoveryPerformed=true and later Playing observations. Pre-recovery
  stopped states are not proof recovery failed. Preventing interruptions remains
  open; sampled resumption is not audible correctness.
- **Next P0 diagnosis:** before/after render callback boundary metrics now in
  shared source, opt-in with --present-phases; profile regression passes. Both
  sm1-partial-corners/sm2-partial-corners builds are now canonical;
  their witnessed moving runs are recorded at the end of this file. No blind
  movement/clock changes or repeated idle soaks. Build/run state in final section.
- **Earlier clean SM1 route/performance check (retained comparison):**
  `chase-west-relaunch-witness`: 3445 positive-counter presentations, maximum
  37.1825ms; the only >100ms interval was startup177.0439ms atcounter0.162audio
  observations,0recoveries after5s. All game/trace/three collector exits0;20064
  runtime events,lost0. Second post-building jump/swing works but catches a wall
  nearcounter2909 and loses Venom. No images; this is not visual/audio/full-route
  acceptance. Read its review-summary.json and priority-observation-review.json.
- **Earlier clean SM2 movie/transition check:** sm2-current-movie-boundaries uses the
  previous callback-boundaries build and full callback/runtime/host/audio witnesses.1441
  movie presentations (max39.3159ms),613 positive-counter gameplay presentations
  (max77.7304ms),no intervals>100ms,97audio observations/0underruns after5s.
  One inspected native image confirms the textured rooftop/player/help marker/HUD.
  This is a bounded transition check, not sustained combat or audible acceptance.
- **P1 route:** native-chase-north-wall-end-script.txt activates all three
  interior triggers and runs the authored player script via ordinary controls.
  Long run shows script advancing, then flag0 and normal30Hz cadence returning
  nearcounter2778. Continuous Up then loses Venom atframe7996. Full chase remains
  unverified; handoff is wallattached near2780. R2-only climbs then walks; next
  jump from the roof before swinging, not a script timing change.
  Two native captures inspected: broken-window cutscene, thenYouLostVenom.
  See [route audit](chase-normal-route.md). No forced triggers/actor writes.

- **P1 rendering:** subset of suspected spikes matches original mesh data,
  including rooftop antennae (chase-paired-native-coordinates geometry report).
  The reproduced late-wall horizontal seam is now repaired as detailed above.
  Other unmatched geometry/material/camera behavior remains unverified; no
  justification to remove original scenery.
- **Builds/package:** canonical sm1/sm2 now contain active ring-transition repair
  and callback-boundary probe plus shared partial-corner precision repair.
  Previous canonical EXEs retained in sm1-callback-boundaries/sm2-callback-boundaries. Promotion is for local
  diagnostics, not P0/P1 acceptance. Current full private package is
  OpenSpidey-Gateway-Chase-SeamFix-2026-09-18.zip (2074files hash-verified; two
  exact candidate EXEs tested natively; data/launchers unchanged). Earlier ZIP is
  outdated. New package is ready for later testing, not a resolution claim.
- **Remaining acceptance:** sustained movement/combat, costumes, movies,
  transitions, audible correctness and rendering in BOTH games; full ordinary
  Chase completion; then refreshed unzip-and-play ZIP with direct Chase launcher.
  User-operated Intel/AMD and integrated/discrete tests, including powerful
  machines, remain required before cross-hardware resolution claims.

## Implemented

- Shared clock preserves scheduled 60 Hz console phase across up to two late edges,
  without an unbounded IRQ backlog. Presentation starts remain capped at 30 FPS
  independently of display refresh. This removes the demonstrated extra wait
  when useful work takes 20–30 ms and the extra-IRQ cliff above 33.3 ms.
- Metrics separate IRQ wait, presentation limiter wait, presentation CPU call,
  and outside-present wall time. The latter is not pure game CPU time.
  Heartbeats label console ticks accurately.
- Bounded asynchronous local logging captures executable hash, CPU, installed
  RAM, actual GL renderer/backend, timer, settings, frame intervals, native game
  update counter/epochs, process CPU/memory/GC, upload bytes and fence waits.
  Optional RECOMP_PERF_GPU=1 adds nonblocking GL batch timer-query records;
  these are not display latency or GPU utilization measurements.
- Both modern GL backends use the shared fixed 36 MiB fenced vertex stream.
  See [ownership and tests](bounded-vertex-streaming.md).
- GTE vertex provenance uses sparse RAM pages; ordinary subword reads avoid
  extra aligned reads without a tag. Certification and partial-write
  invalidation remain enforced.
- RAM-write heatmap updates are disabled when its diagnostic panels are closed.
- Texture presence checks use volatile snapshots; a small thread-local front
  cache avoids repeated memo dictionary locks while retaining version
  invalidation and VRAM checks.
- Actor material descriptors are indexed once per ordering-table submission
  in both games; stock suits remove the inactive resolver callback. Explicit
  invalidation handles suit changes and loader lifetime changes.
- Both games opt into an empty movie-ring polling yield after 32 empty polls;
  queued frames remain immediately available, and yielding releases the ring
  lock and services runtime progress. Log tee writes no longer forward one
  character at a time.
- Shared audio underrun recovery rebuilds a stopped queue instead of replaying
  already-consumed buffers after a mid-refill stall. Actual OpenAL loopback PCM
  regression covers normal refill and both underrun paths. This fixes stale
  replay after starvation; it does not prevent the underlying scheduling stalls.
  Recovery can discard at most 46.44 ms of ambiguously played fresh samples.

## Completed checks

- TimingRegression, including 25 ms useful-work case, IRQ/presentation
  separation, stalls, Chase script lifecycle, and existing audio/CD checks.
- RenderingRegression and Sm2RenderingRegression.
- SuitModRegression and Sm2SuitModRegression (223 assertions in the latter).
- PerformanceRegression: RAM/scratchpad/page-boundary provenance, same-value
  partial writes, both actor tables, CLUT scoping, relocation/free, and real GL
  framebuffer wraparound test.
- Both release publications compile. Candidate A builds reached SM1 sewer and
  SM2 rooftop gameplay; one native capture from each was inspected. SM2's known
  floating polygon remains, so these are not complete geometry acceptance.

## Evidence and pending acceptance

Local logs: proof_render/performance-repairs (ignored artifacts). sm1-first
accidentally selected the old executable and is baseline evidence only.
Candidate A hashes/environments are recorded in each run directory. The final
candidate adds heatmap/texture-cache changes and optional GPU timing; identify
its acceptance runs by their own executable hashes.

The first standalone loaded-clock probe showed approximately 30 FPS at 20,
25, and 30 ms of work. Its 15/18 ms cases overlapped another build/test and are
contaminated; rerun in isolation before reporting the entire matrix.

Pending: final native GL runs and clean clock matrix; GPU-query log verification;
sustained gameplay/audio evidence; Venom mesh diagnosis and ordinary-player full
Chase completion. The final Chase diagnostic reproduced visible distorted
Venom limbs; that is not resolved by the performance changes.

Cross-hardware runs must cover Intel and AMD, integrated and discrete GPUs,
including powerful machines. The logger collects hardware rather than requiring
user-supplied specifications. No development-machine run supports a 99% claim.

## GL 3.3 offender found during implementation

The first explicit SM2 GL 3.3 run (`sm2-final-gl33`) timed out at the harness's
155-second limit. In the 60–90 second window it managed 10.56 native updates/s,
with median presentation interval 93.31 ms, mean present-call wall time 48.87 ms,
and mean outside-present wall time 39.47 ms. This is a failed acceptance run.

`GlCore.Flush` requested a destination snapshot for every batch, although the
modern shader samples it only for mask testing; modern blending uses dual-source
blend factors. GL 3.3 turns that request into a framebuffer copy. The repair
skips unused destination reads for modern display-target batches, retains mask
reads and VRAM feedback synchronization, and fixes the subtract-pass snapshot
source to refer to the actual target rather than the prior snapshot.

PerformanceRegression now checks actual GL 3.3 framebuffer colors for opaque,
average transparency, mask protection, and reverse subtraction. All passed.
The post-change native run is `sm2-gl33-copyfix`; use its final report instead
of extrapolating from the earlier GL 4.5 result.

Clean clock matrix (`clock-load-clean.log`): 0/8/15/18/20/25/30 ms useful work
produced 29.928/29.993/29.962/29.982/29.951/29.726/29.990 updates/s respectively.
All median intervals were 33.333–33.334 ms. The 25 ms case had a 42.103 ms maximum
host scheduling delay; the fix does not claim to eliminate OS scheduling tails.

`tools/performance_report.py LOG_FOLDER --start 60 --end 90` summarizes bounded
logs with the Python standard library. GPU records are excluded from frame
interval statistics. The latest logger aggregates GPU queries by presentation,
records the native main-thread CPU time when available, and defaults to logging
beside the executable (rather than its single-file extraction directory).

## Urgent disk cleanup and current stopping point

The user interrupted implementation for critically low disk space. Native game
processes were stopped. Deleted only disposable duplicate test executables and
compiled libraries under proof_render, plus the regenerated .NET extraction
cache contents for SpiderMan and SpiderMan2. Preserved source, logs, snapshots,
images/audio evidence, and both latest canonical publications in
proof_render/performance-repairs/sm1 and sm2. Approximately 8.7 GiB of identified
artifacts were removed; C: free space measured 11.42 GiB afterward. Old run
folders no longer contain runnable copies; their environment hashes remain
historical evidence. Rebuild helper projects before running them again.

Post-copy-fix SM2 GL33 run completed normally in 106.016 seconds. At 60–90 s:
29.652 native updates/s; interval p50 33.3345 ms, p95 33.4696 ms, p99 51.9487 ms,
max 59.7588 ms; 13 intervals over 50 ms and none over 100 ms. Present call mean
0.4789 ms versus 48.8737 ms before the copy fix. No dropped log records.
Main-thread CPU 0.4852 core; process CPU 0.5474 core. Private memory decreased
from 571.7 MB to 566.7 MB in that window. This is a short scripted run, not
cross-hardware or full visual/audio acceptance.

The next Chase solid-color diagnostic was intentionally interrupted for cleanup
at 39.812 s (exit 4294967295), before acceptance. Do not count it as a pass.
Venom remains unresolved. The opt-in RECOMP_MODEL_DUMP records bounded per-mesh
input/output vertices and GTE controls; prior model.jsonl has 42 mesh records
at frames 2199/2201. Venom uses CLUT 1248; stock Spider-Man uses 864. Traced actor
vertices have ordinary positive depth (roughly 8k–11k), so near-plane clipping
is not supported as the sole cause. The intended next diagnostic renders Venom
CLUT 1248 as solid geometry to distinguish outline distortion from surface
texture/depth ordering. No completed solid-color capture has been reviewed.

Before further native runs, change artifact retention: retain only the two
canonical executable publications, remove disposable run binaries after exit,
and prune superseded SpiderMan/SpiderMan2 extraction caches after republishing.
Do not create another full copied binary per diagnostic and leave it behind.
No new soak or publication is authorized to consume the reclaimed space without
bounded artifact retention. Implementation still needs SM1 and SM2 final checks,
sustained/audio and cross-hardware acceptance, and the unresolved Chase geometry
and ordinary traversal checks. Do not close P0/P1 merely because cleanup ended.

## Resumed work

The native harness now checks for at least 3 GiB free before launch, stops at
2 GiB free, and removes its copied executable, VC runtime files, and its own
DOTNET_BUNDLE_EXTRACT_BASE_DIR cache on exit. Cleanup validates the cache stays
inside that run's directory. `chase-solid-resume` completed in 47.172 s and its
copied executable/cache were removed; logs and evidence remain.

The single native solid-color Venom capture was inspected. The white actor's
silhouette is largely coherent; multiple blue spikes remain outside it. All
traced CLUT 1248 actor triangles are classified as world geometry and therefore
participate in the solid diagnostic. This weakens the prior attribution to a
corrupt Venom mesh. Separate thin primitives include world scenery and untagged
quad objects submitted by RenderObjectList (DrawPrimSet return address
0x8002F248). Their source records match exact native screen coordinates. Do not
claim the visible defect fixed; next classify the remaining spikes precisely.
The opt-in RECOMP_GEOMETRY_IDS diagnostic encodes palette low/high bytes in
R/G and world classification in B, without editing source positions.

A second clock boundary was found by source analysis: keeping only one late IRQ
edge imposed another 16.7 ms wait when useful work barely exceeded 33.3 ms.
The current source instead retains at most two overdue edges (one normal game
update) while preserving the independent minimum presentation interval.
A 40 ms workload regression was added. These newest changes are NOT in the
currently running SM1 GL33 soak executable and need their own validation/build.

The durable native runner is now tools/performance_native_run.py (the ignored
copy was used for the in-progress soak). --suit installs/selects a fixed local
sample fixture for the chosen game, using mods/samples/magenta-man or
magenta-man-sm2. --ids selects the palette-ID diagnostic. Normal runs take no
images. docs/performance-validation.md documents hardware coverage and evidence
interpretation. These are additions to the uncommitted working tree.

### Follow-up evidence (supersedes the pending checks above)

TimingRegression passed with the new overload boundary: 25 ms of work averaged
33.33 ms per update and 40 ms of work averaged 40.03 ms, with no catch-up burst.
PerformanceRegression --gl passed the bounded-ring wrap/readback and opaque,
average, mask and subtract blending checks. Both game publications succeeded.
The SM2 custom-suit soak uses this clock; the earlier SM1 soak does not.

SM1 GL33 stationary sewer soak completed with exit zero after 614.719 seconds.
After the first 60 seconds, 16,231 records span 550.988 seconds: 29.456 native
updates/s, interval p50 33.3351 ms, p95 33.3393, p99 33.4073, maximum 2134.8459.
There were 37 intervals over 50 ms and 16 over 100 ms. These are NOT excluded.
No vertex-fence wait or dropped records occurred. Private memory was 556.8 MB
initially and 566.7 MB at the end/peak. Typical pacing is substantially improved,
but these pauses prevent calling this a clean sustained-performance pass.

The independent Python process monitor also missed its 0.5-second sampling
schedule during the long-pause clusters, with gaps up to 2.5 seconds. This points
to a broader host stall/descheduling event, rather than proving a game-only clock
fault. Present calls, waits, and outside-present time all have affected samples.
No recent Windows Display event 4101 was returned; that does not rule out driver
or host problems. Evidence: sm1-gl33-soak/result.json, report.json and frame logs.

The completed Chase palette-ID diagnostic and normal presented-frame check both
exited normally; every resulting image was inspected. Major blue spikes outside
the solid Venom silhouette map to scenery palette 230, distinct from Venom 1248.
The ID raster capture quantizes color through RGB5A1, so attribution combines
pixel values with traced palette IDs rather than decoding an exact 16-bit ID
from the PNG alone. Other thin untagged objects originate in RenderObjectList.
Actual presented output confirms the visible artifacts; their correct rendering
and ordinary Chase completion remain unresolved. Do not rename this as a proven
Venom mesh defect or count the follower fixture as normal play.

SM2's latest GL33 custom-suit soak has one actual presented capture at tick 4500.
It was inspected: the magenta costume is visibly active, rooftop/HUD/background
are present, and the pre-existing floating orange scenery polygon remains.
This establishes the cache is exercised with visible replacement materials;
it does not establish movement/combat or audio acceptance.

The logger now records AC/battery status and tolerates a thread disappearing
during CPU sampling. A later source audit also fixed stale rotation part-one
files surviving a fresh session and changed process metrics to use their actual
sampling timestamp instead of a queued frame's timestamp. Those last two fixes
postdate the currently running SM2 soak and require a final publication check.
The native runner rejects reused run folders and paths outside its evidence root.

SM2 GL33 custom-suit soak completed normally in 616.360 seconds. After 60 seconds,
16,260 records span 553.611 seconds: 29.369 native updates/s; interval p50 33.3351
ms, p95 33.3399, p99 33.6868, maximum 1677.0439. There were 63 intervals over
50 ms and 29 over 100 ms, with no dropped records. Private memory decreased from
605.1 MB to 581.9 MB, with a 635.3 MB peak. Main-thread CPU averaged 0.3555 core,
whole-process CPU 0.4352. No meaningful vertex-fence stalls were recorded.
The temporary executable and extraction cache were removed. This is sustained
stationary custom-material coverage, not a clean hitch-free or gameplay/audio
acceptance result. Logs and report remain in sm2-final-custom-soak.

Final shared-logger build checks: PerformanceRegression, RenderingRegression and
Sm2RenderingRegression passed; SuitModRegression passed 189 assertions and
Sm2SuitModRegression passed 223, including full sixty-slot capacity. The first
SuitModRegression invocation omitted its required repository argument and exited
before tests; rerunning with `-- .` passed. Both final publications succeeded.
Existing generated-code/MonoMod single-file warnings remain in publication logs;
native runs provide additional execution evidence, not a claim of warning-free builds.

The final SM1 GL33 custom-suit/GPU-query diagnostic exited zero in 109.422 s.
Its single presented image at tick 4500 was inspected: magenta materials are
visibly active, with the sewer, shadow and HUD present. GPU records aggregate
by presentation: the complete frame log is 1,725,038 bytes, with no dropped
records. However, 650,327 batch queries were skipped, so GPU duration is partial
coverage and MUST NOT be presented as full-frame GPU cost. After 60 seconds,
native progress averaged 27.211 updates/s, p99 interval 67.7025 ms, max 2370.0263
ms, with 21 intervals over 50 ms and six over 100 ms. This instrumented run is
not a baseline performance pass. Memory fell from 665.0 MB to 648.2 MB, peak
691.3 MB. Both canonical publications include the final logging fixes; all
three final run folders were verified to contain no copied executable or runtime
extraction cache. `git diff --check` and Python syntax checks passed.

Current acceptance status: P0/P1 remain open. Typical steady pacing, regression
coverage, bounded memory and retained evidence have improved; rare long stalls,
normal movement/combat/audio/transition acceptance, other CPU/GPU classes, and
ordinary Chase completion/rendering are not resolved by these runs. Do not claim
99% confidence or close the GitHub reports on this evidence.

### Geometry continuation: concrete boundaries

- `RenderObjectList` at native 0x8002EED4 calls DrawPrimSet with return address
  0x8002F248. It projects four corners through GTE, stores SXY on the stack, then
  copies X/Y separately with WriteU16 into the buffer pointed to by 0x800B58F0.
  Those partial writes intentionally invalidate provenance; its traced quad
  objects therefore have no world-depth tags. This is a specific investigation
  target, not evidence that restoring tags will fix scenery palette 230.
- `WorldGeometryTrace.cs` records exact native packet coordinates and source
  primitives. Pair those with GeometryTrace triangles before changing projection
  or painter ordering. SM1's loader table is 0x800A0904; slot 7 venom2 uses CLUT
  1248, while stock Spider-Man uses 864. Do not identify a mesh by blue color.
- Model snapshots already compare original VENOM/VENOM2 streams and hierarchy;
  tested retail-asset overrides did not establish a fix. The actor's traced depth
  is positive and not close to the near plane. Avoid repeating these broad tests.
- An older evidence folder mentions RECOMP_PROOF_NATIVE_XY, but the current source
  has no such flag. Setting it now does not run a native-coordinate A/B. Any new
  comparison must have an explicit, verified implementation and build hash.
- Ordinary Chase acceptance must use only player controls, without the follower's
  position writes or region-trigger pulses. `Capture.cs` accepts process-local
  scripted buttons; no host input is permitted. Its control-file button commands
  currently also schedule captures, so do not use many commands and accidentally
  generate an image flood. Static SPIDEY_SCRIPT sequences avoid that behavior.

## Goal continuation: correlated stall evidence (September 17 evening)

Added a low-volume `--stall-trace` mode to the native runner, using runtime
GC/contention events without a CPU sampling profiler. The latter itself produces
runtime suspension events and is unsuitable as the sole test for GC pauses.
The runner records Windows QPC before/after each independent process sample.
`tools/PerformanceTrace` exports absolute-QPC events and coverage/lost-event
metadata; `tools/performance_stalls.py` correlates them with native frame logs.
`tools/performance_heartbeat.py` provides an optional independent 20 ms scheduling
witness with bounded delay records. It sends no input or game-memory operations.

Both five-minute native correlation runs completed with game and collector exit
zero, no dropped log records, and no lost EventPipe events. Their copied executable
and extraction caches were removed by the runner. No images were requested.

- **SM2 stock, GL33:** `sm2-stall-correlation`, 317.468 seconds. After 60 s:
  29.783 native updates/s, p99 40.2679 ms, max 102.0723 ms; 29 intervals over 50 ms,
  two over 100 ms. Both >100 ms intervals have full trace coverage and no managed
  suspension. At 71.761 s, 101.574 ms was outside presentation. At 73.368 s,
  84.092 ms was in the limiter. No matching long monitor gap/collection. These
  are narrower observations, not proof that the previous multi-second stalls ended.
- **SM1 stock, GL33:** `sm1-stall-correlation`, 318.734 seconds. After 60 s:
  29.105 native updates/s, p99 33.5244 ms, max 1760.8556 ms; 30 intervals over
  50 ms and eleven over 100 ms. All eleven have full trace coverage and no
  managed suspension. At 165.747 s, the 1494.032 ms game interval overlaps an
  independent 1505.947 ms heartbeat delay across its entire interval. This is
  direct evidence of simultaneous delay in a separate process, not a GC-only
  explanation. The following cluster includes previous presentation calls of
  1078.594, 703.851, 598.326, 1747.635 and 604.187 ms. Some have no heartbeat delay,
  so do not attribute every outlier to the same global host pause.

The SM1 heartbeat used 0.484375 CPU seconds over approximately 216 elapsed seconds,
retained 40 delayed wakeups without truncation, and exited with its target. Its
coverage starts partway through the run; exact bounds are retained in stalls.json.
These measurements narrow the next action; they do not close P0 or prove a driver
cause. The broad PresentCall measurement also includes title updates and audio
attachment, so assuming it means GPU time would be incorrect.

Added opt-in `RECOMP_PERF_PHASES=1` (`--present-phases` in the runner) to record
event-service, render-dispatch, render-callback, title-update and audio-attachment
wall time in each frame's PresentationStages. RenderCallbackMs is contained in
RenderDispatchMs in the normal flow; do not sum them as independent work. The
render wrapper includes framework/swap/scheduling time, not an isolated GPU timer.
PerformanceRegression --gl and both publications passed after this change.

**Live follow-up at this checkpoint:** `sm1-present-phase-stalls` is running,
native runner session 5829, game PID 16640, with GC/contention trace and a separate
heartbeat (session 62811). It has no captures. Poll the existing handles/process before continuing;
do not restart merely because the observation window expires. On completion,
export cpu.nettrace using tools/PerformanceTrace, regenerate stalls.json/report.json,
and inspect previous_present_stages for the large intervals. Current P0/P1 status
is still open; Chase geometry and ordinary completion were not changed this turn.

### Follow-up: presentation split and paired geometry

The above `sm1-present-phase-stalls` run and heartbeat completed normally
(runner 327.109 s, game/collector exit zero); those old sessions are terminal.
The trace exported 1,222 GC/contention events, zero lost. After 60 s: 27.624
native updates/s, p95 41.1222 ms, p99 80.2183 ms, maximum 1701.6495 ms, with
221 intervals over 50 ms and 50 over 100 ms. Preserve these failures.

The first presentation-specific outlier is now localized: present 3436 spent
157.493 ms servicing events, 0.244 ms in render dispatch, and negligible time
in title/audio. It caused the next 166.979 ms presentation interval at 115.635 s.
It had full trace coverage, no managed suspension, and no delayed heartbeat.
The next event service took 99.094 ms. Later outliers include both event service
(1214.551 ms) and render-wrapper time (1018.577 or 882.786 ms with small render
callbacks), amid simultaneous independent heartbeat delays. There is more than
one observed stall location; do not label all of it GPU cost or GC.

Extended the opt-in presentation breakdown with EventPumpMs, InputPollMs and
ControllerEventsMs. These are nested timings, not independent additive totals.
Source/decompiled installed Silk.NET 2.22.0 shows its GLFW input context updates
16 gamepad and 16 joystick slots on ProcessEvents, while our InputManager also
uses SDL for controllers. This is an investigation lead, NOT proof that either
poller causes the stalls. Do not disable controller support as a supposed fix.

For Chase, source-packet matching across the previous separate dumps was not
unambiguous (no matches for the selected scenery palette); do not use that failed
comparison as evidence. GeometryTrace now records NativeX/NativeY alongside each
submitted vertex in the exact same triangle. `chase-paired-native-coordinates`
completed in 53.766 s, exit zero, without images. At frame 2200, CLUT 230 has
330 submitted vertices, max |X-NativeX| 1.01007, max |Y-NativeY| 0.98288; Venom
CLUT 1248 has 1,731 vertices, maxima 1.03003/1.02942. None exceed two pixels.
Thus the large suspect silhouettes are not created by large fractional-coordinate
displacements in that sample. This still does not establish normal Chase visuals.

The native runner now supports `--level l5a1` without follower/region pulses and
`--extra-script` for additional process-local buttons. That is the appropriate
path for ordinary-control Chase testing. `--chase` remains the diagnostic follower.
No ordinary-completion claim has been made.

PerformanceRegression --gl passed the latest event/geometry instrumentation;
SM1 was republished with it. SM2's canonical publication still has the earlier
presentation breakdown and needs republishing before testing the new fields.

**Completed:** `sm1-event-input-phases`, runner session 58110, game PID 23172,
heartbeat session 85932 are terminal. Run/collector exited zero after 308.078 s;
656 GC/contention events, zero lost. After 60 s: 7,272 frames over 242.388 s,
29.997 native updates/s, p50 33.3351, p95 33.3382, p99 33.3403, maximum
38.1793 ms; zero intervals over 50 ms. Heartbeat: 13,522 intervals, maximum
38.6035 ms, no delays over 50 ms, 0.828125 s CPU. No functional timing fix
preceded this clean result: intermittent stalls remain unresolved, and this
does not supersede the previous failing evidence. New nested event/input fields
are present. SM2 was subsequently republished with the same instrumentation;
`publish-sm2-events.log` records success.

### Gateway bundle and ordinary Chase entry

User's independent machine is Gateway GWTN156; they requested one full private
ZIP, ready to unzip/play, with direct Chase entry and performance logging.
`tools/build_local_test_bundle.py` creates
`proof_render/OpenSpidey-Gateway-Chase-Test.zip` from current published games,
local original disc manifests/files, WAD entries read directly from CD.HED/CD.WAD,
and current bundled assets. No experimental loose override folders or saves are
copied. Both self-contained EXEs include their native runtime dependencies.
ZIP: 1,025,641,556 bytes; all 2,074 manifested files verified by length/SHA256.
Do not upload this personal data bundle as a public release.

Launcher 1 sets l5a1 plus four title-relative start/cross pulses, then leaves
normal controls enabled. No follower, forced region triggers, or proof triggers.
All launchers enable performance and nested presentation phase logging. Initial
settings: 4x, FXAA, 1280x720, widescreen, VSync off, memory cards disabled.
Shared 30 FPS cap remains. Launcher 2 opens normal SM2, launcher 3 normal SM1.

`chase-ordinary-first` (runner 4819, PID 15312) completed, exit zero, 68.594 s.
It used ordinary up+R2 at l5a1_t.trg+400 for 1,600 ticks. Inspected presented
frame 2000: rooftop, Spider-Man, Venom distance bar, objective and suspect blue
railing/framework scenery. It FAILED the route: "You Lost Venom" around frame
2190, failure script 152 by 2450. Do not count this as ordinary completion.

`tools/verify_local_test_bundle.py` extracts the actual ZIP into an isolated
folder and starts its EXEs without any data-directory argument. It parses the
packaged launcher environment, overriding only hidden capture, exclusive
process-local test input, bounded exit, and log/cache paths. SM1 reached the
Chase introduction (presented frame 1700 inspected), exit zero after 49.422 s.
The scene contains the Chase objective, Spider-Man, Venom and rooftop scenery;
suspect protrusions remain visible. This verifies automatic scenario entry,
not corrected geometry, ordinary completion or audio listening acceptance.
Original source data paths are not used; packaged game and builtin asset paths
are confirmed by the logs. The goal and P0/P1 remain active/open.

SM2 isolated ZIP verification also completed: exit zero after 88.187 s,
presented frame 4500 inspected. Spider-Man, rooftop tutorial question marker,
skyline and HUD are present. Normal new-game startup reached e1m0 with only
process-local menu/dialogue buttons. No level override was required. Audio was
not listened to in these portability checks, and neither run is sustained
performance acceptance. Logs/configs/results and the two inspected captures
remain under `proof_render/gateway-bundle-verification/{sm1,sm2}`. Temporary
extracted game copies and native extraction cache were removed after completion;
the verified ZIP remains. No game/test process from this packaging check is live.

### Follow-up: SM2 event/input breakdown and an attribution gap

The preceding goal turn made progress: the requested portable bundle was built,
verified file-by-file, tested from isolated extracted copies, and delivered.
No independent-laptop result has been received yet; do not treat that as a
blocker to source investigation or a reason to repeat already completed geometry
replacement experiments.

`sm2-event-input-phases` completed 18,000 console ticks in 304.797 s, game and
trace collector exit zero. Runner 29365, heartbeat 72037 and game PID 14832 are
terminal. No captures requested. Exported 602 GC/contention events, zero lost.
After 60 s: 7,258 presentations over 241.919 s, 29.998 native updates/s;
p50 33.3349, p95 33.3378, p99 33.3388, maximum 36.0695 ms. Zero intervals over
50 or 100 ms. Independent heartbeat: 14,179 intervals, zero over 50 ms, maximum
22.183 ms; 0.78125 s CPU. No dropped/partial frame records. Private memory
570,609,664 -> 554,319,872 bytes, peak 570,609,664 in the selected process window.
Results/trace/heartbeat remain in the named folder. This clean run does not
identify or fix the earlier intermittent stalls. It exercised the same SM2
tutorial fixture already visually inspected in the ZIP check, not sustained
manual movement/combat or audio acceptance.

Source audit found an instrumentation gap: ServiceOnly and the non-presenting
vblank call ServiceEvents too, but PresentationProfile.Reset cleared their
accumulated timings immediately before rendering. Added Begin/EndPresentation
boundaries and nullable `BetweenPresentationEvents` to the frame log. It holds
the current interval's event-pump/input/controller costs outside presentation;
current `PresentationStages` still describes the current rendering call. The
stall correlator pairs previous-row presentation stages with current-row
between-presentation events. Controller cost is nested inside input cost.
This is diagnostic attribution only; no pacing or input behavior was changed.

`PerformanceRegression --profile --gl` with RECOMP_PERF_PHASES=1 passed, including
tests that a synthetic 75 ms outside event stall is retained separately from
the current 2 ms pump and that costs do not leak into the following interval.
All existing GL pixel/blending/mask/stream assertions passed too. Evidence:
`between-event-regression.log`. The user's delivered Gateway ZIP remains the
previous verified candidate; this diagnostic-only change does not alter it.

Both canonical games were published successfully with the new fields (logs
`publish-sm1-between-events.log`, `publish-sm2-between-events.log`). Native log
checks `sm1-between-events-check` and `sm2-between-events-check` finished with
exit zero in 46.390 and 64.907 s, respectively. Runner session 10982 is terminal;
no SpiderMan/SpiderMan2 process remained when checked after the user's audio
complaint. SM1's 1,199 noninitial rows all retained between-frame event data,
with no nested-parent or outside-wall-budget violations; see attribution-check.json.
These checks produced no images and are diagnostic-field checks only.

User complained that further hidden diagnostic games were audible and wanted
to test the ZIP. Re-deliver the existing full Gateway ZIP promptly. Do not launch
more audible games while the user is testing; continue source/log work instead.
The already delivered ZIP has not been replaced by these newer diagnostic builds.

User subsequently authorized necessary local diagnostics and asked to be called
on once there is a stronger candidate or a specific hardware-dependent question;
they have limited test time. This supersedes the preceding temporary instruction
to avoid game launches. Runner now defaults to the existing `Muted` setting,
which changes only OpenAL source gain, retaining mixing and the live device queue.
`--audible` restores listening runs; `--audio-state` records bounded queue/SPU state.
No shared audio behavior or user's ZIP settings were changed. Runtime.Initialize
applies mute before game execution; Audio.FillBuffers still mixes/uploads/queues
PCM. Do not substitute a null audio driver, disabled mixer, or queue state for
actual audio correctness acceptance.

`sm1-between-event-stalls` finished in 185.344 s; game/collector exit zero.
Runner 77595, heartbeat 25585 and PID 32760 are terminal. No captures.
Exported 241 GC/contention events, zero lost. After 60 s: 3,632 presentations,
121.042 s, 29.998 native updates/s; p50 33.3352, p95 33.3385, p99 33.3406,
maximum 33.7919 ms, zero intervals over 50/100 ms. Between-present maxima in
that window: event pump 0.1266 ms, input 0.1750 ms, controller subset 0.1641 ms.
Heartbeat: 8,096 intervals, no delays over 50 ms, maximum 21.865 ms,
0.4375 s CPU. Audio state: 180 observations over 179.316 s; none reported a
stopped queue after five seconds. Settings confirm Muted=true; this is evidence
that muted speaker output preserves queue processing, not a listening pass.
Preserve earlier failures: the rare stalls did not reproduce in this fixture.
Do not repeat more stationary SM1/SM2 soaks just to obtain additional clean runs;
the next acceptance work must cover meaningful movement/transitions and Chase.

SM2 movement fixtures (muted, normal controls, no actor position writes):

- `sm2-movement-event-check`: 6,000 ticks, 110.344 s, exit zero. Appended
  e1m0_t.trg+2600 up:400, +3100 cross:30, +3200 up+r2:1000. Inspected the only
  capture, frame 4500: it is the main menu, not gameplay. Shell reloaded at
  4192. This route FAILED sustained traversal; do not label the run a gameplay
  acceptance pass or infer the exact failure cause from a menu screenshot.
- `sm2-jump-swing-event-check`: 4,300 ticks, 78.656 s, exit zero. Appended
  e1m0_t.trg+2600 cross:12, +2612 up+r2:180. Inspected its only capture, frame
  3900: Spider-Man against the building wall, camera moved from the rooftop,
  building geometry and HUD present. This establishes changed gameplay content,
  not full traversal/combat completion or the animation's correctness over time.
  After 60 s: 421 presentations, 14.052 s, 29.889 native updates/s; p99
  33.3381 ms, one 84.6211 ms interval and none over 100 ms. Preserve the hitch:
  present 1951 immediately after the frame-3900 screenshot spent 84.3327 ms
  outside presentation, versus 0.2868 ms previous presentation and 0.1080 ms
  between-present event/input work. Screenshot readback/encoding is a specific
  candidate, not yet a measured breakdown. No GC trace or heartbeat was taken,
  so their absent overlap rows must not be read as proof of no GC/host delay.
  `stalls.json` uses threshold 50 ms. Repeat this exact route without capture
  before attributing its outlier to game movement.

The exact no-capture repeat `sm2-jump-swing-no-capture` finished 4,300 ticks in
76.546 s, exit zero. After 60 s: 414 presentations over 13.768 s, 29.998 native
updates/s; p50 33.3351, p95 33.3382, p99 33.3404, maximum 33.4520 ms, zero
intervals over 50/100 ms. This supports screenshot work as the cause of that
specific 84.6 ms observation; it does not explain the earlier multi-second
stalls. Retain both runs rather than silently discarding the captured outlier.
All runs above are terminal (44904, 66786, 38463); no game processes remained.
Only two images were produced this continuation, and both were inspected.

Next work: extend a verified ordinary-control route and meaningful transition/
combat coverage, especially Chase completion; do not substitute the follower.
Use the new event timing fields if a long stall recurs. The duplicate GLFW/SDL
polling remains an unproven lead: don't remove input support based on it. Local
tests remain insufficient to close P0/P1 or request a broad user retest yet.

### Chase source matching: frame delay resolved

The last goal turn made progress (silent diagnostic setup, completed timed runs,
and a visually inspected ordinary SM2 motion fixture). The next investigation
uses existing Chase evidence rather than repeating more idle runs.

`tools/chase_geometry_join.py` joins the native source primitives and submitted
triangles in `chase-paired-native-coordinates`, retaining ambiguous matches.
The prior zero-match result used the wrong frame relationship: this fixture's
GPU triangles match source records TWO console ticks earlier, not the same tick.
The tool requires the fixture's verified full-frame draw origins (0, 0/256),
compares all three native XY pairs, CLUT 230, and carried depth within 0.02.
It identifies 92 of 330 triangles (46 each at frames 2200/2202; no source 2196
was captured for frame 2198), with 172 candidate matches due to coincident faces.
All matches use native caller 0x80077E40, inside func_80077D64's world-mesh path.
Depth mismatches among these position candidates: zero. This corrects the earlier
failed comparison; it does not identify every palette-230 primitive.

All 43 matched primitive addresses map to one load base, 0x800E956C, of
`spiderman/extracted/wad/l5a1_g.psx`, meshes 44, 45, 113, 115, 116, 117, 118.
Each mesh's complete original vertex byte range equals RAM snapshot 2202.
Native loader changes face flags/UV/CLUT/lighting fields, so source matching uses
length plus four vertex indices to infer the common base, then exact vertex
byte comparison. Do not mislabel the expected face-loader transformations as
corruption. Asset verified byte-for-byte against CD.WAD at offset 46,336,000,
length 216,592, SHA256 1479ac0e53138474bd7cfb2bc4610dd65d9cfdff40299ad991756abd4d32e9ab.
Some largest traced palette-230 strips (up to 83.6 screen pixels in frame 2200)
are in this matched subset. Source meshes already contain long thin pole-like
quads; for example mesh 115 vertices 0..3 span 250 units vertically and only
12/20 units in X/Z. Treat scenery/poles as authored geometry unless the remaining
camera, material sampling or ordering comparison proves an actual defect.

Reproducible report: `chase-paired-native-coordinates/geometry-source-report.json`.
This is not a scene-wide rendering pass, proof of camera correctness, or ordinary
Chase completion. No rendering behavior changed and no images were generated by
the offline source analysis. Python compilation and diff whitespace checks pass.

`chase-jump-swing-ordinary` finished 3,600 ticks in 66.093 s, exit zero, runner
32828/PID 6012 terminal. Ordinary cross at l5a1_t.trg+400 for 12 ticks, then
up+r2 at +412 for 1,600 ticks. Its one capture at 2300 was inspected: the actual
"YOU LOST VENOM" scene, not completion. Source/log chronology exposes the test
problem: trigger load 1189, intro scripts 1570, jump 1589, more intro scripts
1638/1690, release/transition scripts 1742, failure 2202. The jump occurred
during the scripted introduction and cannot establish a jump/swing defect.
Next ordinary attempt delays jump until +650, after this observed boundary.

`chase-post-intro-jump` completed 3,600 ticks in 65.390 s, exit zero; runner
38281/PID 26948 terminal. Cross +650:12, then up+r2 +662:1300. Its only capture
(2300) was inspected: Spider-Man airborne between buildings with the Chase
distance bar and HUD, unlike the earlier failure cinematic. At 2339 the native
FallFog/billboard scripts ran, then script 152 at 2407 freed Venom. This is a
failed fall, not Chase completion. It improves the input timing but does not
validate continuous traversal. There is no evidence yet that holding R2 once
performs the sequence of swings needed by this route.

Consulted the reproduced original PlayStation manual's control table to confirm
R2 is Web Swing Forward and R1 Zip-Line; source:
https://oldgamesdownload.com/wp-content/uploads/manuals/spider-man_ps1_manual_en_wb1.pdf
(text transcription: https://manuals.plus/m/f16b356a6af28ab4812761073c33e91b733833b4a80afb8bed7a103522974c52).
Do not infer timing/holding semantics beyond the control table. Source
func_8006B208 maintains separate held and newly-pressed fields; a repeated-pulse
route is the next controlled experiment, not a proposed gameplay modification.
Runner now exposes --snapshots for at most eight native RAM snapshots within a
bounded run. They use the existing process-local RamSnap hook, no host input.
Snapshot runs are route diagnostics, not timing acceptance (disk writes add cost).

`chase-pulsed-swing-route`: 3,600 ticks, 65.172 s, exit zero (runner 8568,
PID 26240 terminal), no images. Cross +650:12; up +662:1600; R2 +700 through
+2200 in 60-tick increments, held for 30 ticks. Four 3 MB RAM snapshots are
retained, with decoded `route-state.json`. At 1900 player (722.86,579.40,-7661.72)
and Venom (462.56,1167.34,-3175.04); at 2100 player (932.48,538,-5926.12) and
Venom (1331.17,1448.67,-365.31), in native fixed-point coordinates divided by
4096. Player script field +0x1A8 was zero in both. The gap grew and the native
lost-Venom scripts ran at 2148; by 2300 the failure script owned the player,
and by 2500 both actor pointers were gone. Repeated pulses did not solve this
route. Do not call the failure a timing defect without further evidence.

Recovered the earlier visually inspected swing fixture from
`proof_render/issues-1-3/web-wrap-fixed/environment.json`: it used **up+cross
together**, then up+r2, shortly after the introduction. The newer vertical-jump
attempts omitted up during jump and began later. Next route uses +575 up+cross
for 12 ticks and +590 up+r2 for 1,500 ticks, with state snapshots rather than
additional screenshots. Its outcome must be inspected before any claim.

`chase-forward-jump-route` completed 3,600 ticks in 66.593 s, exit zero; runner
46470/PID 21148 terminal, no images. Six 3 MB snapshots and decoded
`route-state.json` preserve the outcome: FAILED ordinary chase, not completion.
At 1700 the player script is active. At 1850/2000/2200 it is inactive and the
native per-button records show Up=held and R2=held. Player Z progresses from
-9325.69 (1850) to -6165.13 (2000), then only -5616.87 (2200); meanwhile Venom
reaches Z=1420.11 and the distance is 7323.09. At 2400 the failure script owns
the player; at 3000 actor pointers are gone. This proves inputs reached native
gameplay and that the constant-direction route stalls in space. It does not
prove a physics/clock fault, nor establish why the player slows there.

`tools/chase_route_state.py` decodes existing SM1 snapshots without modifying
RAM. It retains positions, raw rotations, native player-script state, Venom's
task/script words, distance and per-button held/pressed bytes. Verified the
native input order in func_8006B514: buffer byte 2 is shifted HIGH, so these
records use Up index 10, Cross index 3, R2 index 7 at 0x800A4DF4. A preliminary
decoder incorrectly assumed host Controller.State ordering and reported zeros;
that decoder was corrected and route-state.json regenerated. Do not interpret
the preliminary zero fields as an input bug. Shared input code is unchanged.

Next ordinary route work should use landing/position feedback or a planned
second jump/turn near Z=-6165..-5617, not another single held-up/R2 variant.
Do not change chase speed or broad timing to accommodate an unverified route.
All native runs in this continuation are terminal. Two new game captures were
produced and inspected; offline geometry and RAM analysis needed no images.

### Ordinary rooftop obstruction and re-jump (latest continuation)

`chase-second-jump-route`: 2,900 ticks, 53.812 s, exit zero, PID 22464
terminal. Initial up+cross +575:12, up+r2 +590:1500, extra cross +800:12
(relative to l5a1_t.trg). Eight native snapshots, one capture at 2100.
Inspected that complete capture: Spider-Man is in a running pose on a roof,
with antenna/pole scenery directly ahead, Venom on the left and the Chase HUD.
Player position is exactly (994.7600,539,-5616.8677) at both 2100 and 2200,
script inactive, Up/R2 held. The second jump at 1992 occurred before landing;
player at 2000 was still (678.6274,736.3972,-6165.1338). Lost-Venom scripts
at 2206 and actor removal at 2466. Failed route, not completion. The capture
explains the constant-forward obstruction better than an unsupported clock fault.

`chase-rooftop-rejump-route`: 3,200 ticks, 65.250 s, exit zero, PID 25468
terminal. No images. Up+cross +575:12; up+r2 +590:310; up +900:1100;
cross +920:12; r2 +935:1050. Trigger load 1150; jump at 2070 follows the
2050 snapshot showing the same stationary rooftop position. By 2130 player
moved to (1365.5896,502.5984,-4973.6267), and by 2200 to
(1895.3745,2364.8799,-4054.9941). Up/R2 reached native button records.
Fall-related scripts fired at 2243; removal script 152 at 2311. This jump gets
past the prior obstruction but the route falls; it is not successful Chase
traversal, proof of a swing defect, or a timing acceptance run. No runtime
behavior changed in either attempt. Both runners cleaned copied binaries and
runtime extraction caches. All diagnostics in this continuation are terminal.

Route work should now establish the correct rooftop turn/web attachment path,
using the native position and camera evidence, instead of further varying jump
start times blindly. Do not slow Venom or alter global physics to make this
particular script succeed. The remaining rare-stall investigation is separate;
these snapshot/capture runs provide no evidence of a performance fix.

### Conservative CPU brackets for retained long stalls

`tools/performance_stalls.py` now uses the independent runner's collection
start/end QPC bounds to choose CPU reads wholly before and after each stall.
Whole-process cumulative CPU includes other threads and surrounding time, so
using it is conservative when estimating whether main-thread computation could
account for the interval. Subtract an additional 31.25 ms quantization allowance.
Do not interpolate coarse CPU counters, interpret null coverage as zero, or call
this a direct measurement of scheduler/driver/GPU waiting.

New `stalls-cpu-brackets.json` preserves results alongside the old reports:
- sm1-present-phase-stalls: 8 of 50 >100 ms stalls have positive non-CPU lower
  estimates. At 302.343 s, the 1701.6495 ms stall is enclosed by a 5632.875 ms
  sample span with just 703.125 ms of whole-process CPU: at least approximately
  967.274 ms is not explained by computation after the allowance.
- sm1-stall-correlation: 6 of 11 have positive estimates. At 165.747 s, a
  1494.0317 ms stall has 234.375 ms whole-process CPU across a 2396.159 ms
  enclosing collection span, giving a 1228.407 ms lower estimate.
- sm2-stall-correlation: neither >100 ms stall has a positive estimate with these
  coarse brackets. This is insufficient resolution, not proof of CPU saturation.

This strengthens the waiting/descheduling lead for some SM1 long stalls, but
identifies neither the responsible API nor the OS/driver cause. Five focused
CPU-envelope tests pass (boundary overlap, missing coverage, multiple CPU threads,
reset counters, exact boundaries). Run the test file directly: broad unittest
package discovery also imports the unrelated Blender addon, which requires bpy.

The terminal session is not elevated; WPR reports no recording in progress.
No system-wide recording was started and no elevation or host settings changed.
Per-process .NET stack sampling remains available. The next bounded native run
uses it with event timings and the independent heartbeat; sampling suspensions
must not be mislabelled as spontaneous GC stalls or clean acceptance.
`performance-system.json` source now includes native mainThreadId (null where
unavailable) so future builds can correlate the specific game thread without
inferring it from thread ordering. Existing binaries/ZIP do not yet include it.

### Reproduced stalls with event phases, raw stacks and CPU brackets

`sm1-thread-stack-stalls` completed 18,000 ticks in 326.422 s; game and collector
exit zero, runner 92510 and heartbeat 91930 terminal (game PID 29812). Muted,
GL33, stock l5a3, no captures or snapshots. This is a diagnostic run, NOT clean
acceptance: after 60 s, 27.727 native updates/s, p95 39.4253 ms, p99 71.1544 ms,
max 1235.9953 ms, 234 intervals >50 ms and 45 >100 ms. Retain all failures.
Trace exported 461,958 GC/contention events with zero lost. Many are the sample
profiler's SuspendOther events; do not relabel these as spontaneous GC pauses.

Specific newly narrowed failures:
- 163.565 s: 1206.0906 ms interval, 1192.3468 ms IRQ wait, tiny event/input
  timings, no managed suspension. Independent heartbeat delayed 1252.8721 ms.
  Whole-process CPU bracket gives a 1018.591 ms non-CPU lower estimate.
- 164.872 s: 1235.9953 ms interval, preceding render wrapper 1224.3715 ms,
  render callback just 0.2298 ms; event/input timings remain tiny. Only 5.853 ms
  of overlapping sampling suspensions. Non-CPU lower estimate 1032.870 ms.
- 171.482 s: 1097.2341 ms interval, outside-present 1095.267 ms, again tiny
  event/input timings and a concurrent independent 1022.4626 ms heartbeat gap.
- Later 905.9202/753.1193 ms stalls overlap substantial SuspendOther pauses
  (886.386/639.362 ms), plus heartbeat delays. Do not merge these with the first
  pause-free stall or claim sampling had no effect.

This weakens duplicate input polling as the cause of THIS failure cluster; it
still does not explain the old isolated 157 ms event-service stall. Evidence now
requires distinguishing blocking/descheduling/host or driver delays. Do not
optimize DrawPrimSet based on the misleading converted stack span described next.

Critical profiler interpretation: main thread 25784 is identified by Program.Main
in the converted profile. `PerformanceTrace --sample-thread 25784` exports 115,125
raw provider events, zero lost. The first 1206 ms stall has ZERO raw samples; a
2381.8304 ms sample gap spans it and most of the next stall. Speedscope stretches
the previous DrawPrimSet stack over that gap and labels it CPU_TIME. That cannot
be literal measured CPU: phase timings show IRQ wait and the independent CPU
bracket shows little computation. `long-stall-sampled-stacks.json` is explicitly
labelled unreliable for that attribution; use `long-stall-sample-coverage.json`
and `stalls.json`. At 171.482 s there are seven raw samples and a 1072.9067 ms gap.
The raw exporter plus report coverage prevents a false hotspot diagnosis.

Eight Python CPU/sample-envelope tests pass. Shared Runtime release build passes
with four existing warnings, zero errors (`runtime-thread-id-build.log`); the
new mainThreadId hardware field is compiled but canonical game publications and
the full user ZIP are not yet rebuilt with it. Normal git diff --check passes.
An attempted check with autocrlf forcibly disabled treated repository CRLF lines
as whitespace errors; no files were converted or rewritten to satisfy that check.
All test binaries/runtime caches were cleaned by the runner. No game remains
running from this continuation. P0/P1 are still open; no gameplay or speed fix
was justified by this diagnostic evidence.

Audio corroboration for that same run: the bounded audio trace has 184 records,
ending at 179.497 s. Nine observations after warm-up report AL_STOPPED/playing
false, at 163.420, 163.563, 163.769, 164.393, 164.576, 164.899, 165.712,
166.175 and 171.482 s; the final observation is playing again. This overlaps the
long-stall cluster and shows queue starvation/recovery, despite diagnostic output
gain being muted. Do not claim uninterrupted audio, nor infer exact audible gap
length from these sparse observations. The trace ends before the later stalls.
The run retained about 183.85 MiB; C: has 130.52 GiB free. Target PID 29812 is
absent after normal exit. No images were generated during this continuation.

### Confirmed shared audio underrun recovery defect and repair

Source review after the audio starvation evidence found a separate recovery bug:
FillBuffers sampled BuffersProcessed once, unqueued/refilled that many buffers,
then called SourcePlay on a stopped source. If the device consumed the remaining
old buffers during a mid-refill stall, restarting replayed those already-consumed
buffers from the queue front. This is not the cause of the scheduling stall.

Deterministic reproduction uses the installed OpenAL Soft 1.23.1 loopback device
(no speaker device, desktop capture, OS input or timing sleeps). Eight buffers
contain distinguishable constant PCM values. Consume buffer 1000, unqueue it,
advance the loopback until the remaining old queue drains, queue fresh 9000,
and run the old restart. Actual rendered median was 2000/32768 = .06103515625,
not 9000/32768: stale playback is proved, with no OpenAL errors.
Reproduction retained as `proof_render/performance-repairs/audio-underrun-repro.py`.
Vendor source corroborates restart semantics:
https://github.com/kcat/openal-soft/blob/master/include/AL/al.h
and the vendor example explicitly resets/clears an underrun queue before recovery:
https://github.com/kcat/openal-soft/blob/master/examples/alffplay.cpp

Shared `AudioBufferQueue.Refill` now handles a stopped source before ordinary
refill, and checks for a second underrun after refill. A stopped queue is detached
and rebuilt from fresh samples before Play. This avoids replaying consumed audio.
On a mid-refill underrun, up to one queue of freshly mixed, ambiguously played
samples may be discarded (8*256/44100 = 46.44 ms). This is a bounded recovery
tradeoff, not added steady-state latency, clock retiming, an increase in buffering,
or a fix for host starvation. AudioCapture records generated SPU samples, including
those discarded during recovery; do not treat it as exact device output.

`AudioStreamingRegression` calls the actual shared refill code with an actual
OpenAL loopback context. It verifies normal PCM order, stopped-at-entry recovery,
and deliberate mid-refill starvation using rendered PCM values, playing state,
exact queue length, expected producer calls, no OpenAL errors, and zero managed
allocations across 1,000 idle polls. All pass. TimingRegression also passes with
the change, including CD/XA behavior and the existing clock/Chase invariants.
Logs: audio-recovery-regression.log, audio-recovery-timing-regression.log.
Native audio-state records add `recoveryPerformed`; `state`/`playing` describe the
state observed before recovery, while the record timestamp follows that operation.
No claim of uninterrupted audio is supported by a recovered event.

Both game publications and native integration checks are the next gate. The user
ZIP has not been replaced. P0/P1 remain open pending the main stall cause and
ordinary Chase completion, as well as the wider acceptance requirements.

Both canonical native publications now include the recovery fix and native
main-thread ID metadata. Published successfully with the existing warnings:
`publish-sm1-audio-recovery.log`, `publish-sm2-audio-recovery.log`.
27 OpenAL streaming assertions and 304 TimingRegression assertions passed.

Native integration results (muted, no captures, no injected process delays):
- `sm1-audio-recovery-integration`: 3,600 ticks, 66.047 s, exit zero; 62 audio
  observations, none nonplaying after 5 s. Canonical executable SHA256
  5551EA9CC39FF53E3A908409478B0E5B918C3C766D1C83D1AC9B3EC48FF8D9F7.
  Uses the already-known failed ordinary Chase route; NOT Chase completion.
- `sm2-audio-recovery-integration`: 4,300 ticks, 76.703 s, exit zero; 74 audio
  observations, none nonplaying after 5 s. Canonical executable SHA256
  8A9F863B1DFFEA4BAC725064E30F33C474424608EDD22723BD05440A013DBE48.
  Uses the previously inspected short jump/swing movement route.

Each run retains `integration-summary.json`. Neither supplies new visual/audible
acceptance or proves uninterrupted playback between sparse observations. Exact
underrun PCM behavior is covered by the loopback test, not a natural underrun in
these short integration runs. Both runners exited and removed copied EXEs and
runtime caches. No new images. The earlier full ZIP remains unchanged and does
NOT contain this fix; rebuild and verify it before asking the user to test a new
candidate. The rare scheduling stalls and remaining P1 verification are still open.

### Ordinary Chase turns and route-state decoding

The read-only route decoder now reports absolute tick, most recent l5a1 trigger
load tick, and relative level tick. It also decodes Left/Right/Down/R1, verified
against the masks in native func_8006B514 at 8006BA54..8006BB18 (native records
Left 8, Right 9, Up 10, Down 11, R1 6, R2 7, Cross 3). No runtime input mapping
or game timing was changed. Compare relative ticks because title/level load
frames differ between runs; those differences are not a movement regression.

- `chase-left-rooftop-route`: 3,500 ticks, 63.703 s, exit zero; PID 31724
  terminal. Up+Cross +575:12, Up+R2 +590:290, Up+Left +880:65,
  Cross +920:12, Up+R2 +945:1100. No images. It passes the first obstruction,
  reaches (-680,24,-3477) at absolute 2200, then moves almost only vertically
  to Y=-1930 by 2500, consistent with climbing the next building. That animation
  was not visually verified. At 2800 player is (-2007,-2877,-2712), distance
  8325; lost-Venom script at 2832, removal at 3092. Venom advanced to script
  0x800E23D2 (words 0,11,16,0,11,18), beyond earlier attempts. Failed route.
- `chase-wall-zip-route`: 3,900 ticks, 73.235 s, exit zero; PID 29680 terminal.
  Same turn plus R1 +1100:12, fired at 2268. No images. Position trajectory stays
  near (-680,Z=-3478) while moving vertically, then reaches the upper surface.
  Lost-Venom at 2858, removal 3118. The R1 press occurs between retained snapshots;
  logs prove harness firing but not a sampled R1 edge. It did not materially
  change the resulting trajectory. Do not infer a broken zip-line control.
- `chase-short-left-route`: 3,800 ticks, 69.531 s, exit zero; PID 24608 terminal.
  Left shortened to +880:28, Up +908:37, same Cross +920 and Up+R2 +945.
  One capture at 2350 inspected in full: Game Over menu. At relative 892 player
  (1002,539,-5577), yaw2037; at992 (1113,495,-4737), yaw2183; at1092
  (1463,4876,-3073). Fall scripts at2254, removal2322. The shorter turn avoided
  the long vertical path but fell. It does not establish a physics/timing fault.

The next controlled comparison delays R2 from +945 to +980 while maintaining
Up. Hypothesis only: the earlier edge press may have occurred while still over
roof geometry; later press may permit attachment over the gap. Inspect its result
before making a claim. These are route diagnostics with RAM snapshots, not
performance acceptance; no follower, region pulses or actor-position writes.

### Delayed swing reaches the next roof; full Chase remains open

`chase-edge-swing-route` completed 3,800 ticks in 68.704 s, exit zero, PID30532
terminal, no images. Relative +980 R2 replaces +945, with uninterrupted Up after
+908. At relative1049 player (1531,1367,-3434), at1149 (2144,907,-1885),
and at2349 (2206,906,-1772), distance4056, player-script inactive. Venom remains
on 0x800E23D2 (0,11,16,0,11,18). No fall/loss script occurred before the run
ended. This proves a better ordinary-control route to a second-roof checkpoint,
not full completion or visual verification. It narrows the previous fall to
control timing: a 35-tick later swing press succeeded without a physics change.
The exact native swing eligibility check was not traced; pressing over roof
geometry is only the current explanation to test, not a proved engine condition.

`chase-third-roof-route`: 5,200 ticks, 91.625 s, exit zero, PID27428 terminal.
Keep confirmed route, end R2 +980:220; add Left +1200:28, Cross +1240:12,
R2 +1300:4000, continuing Up. Eight snapshots, one capture at3000 inspected:
Game Over menu. At relative1286 player (2207,436,-1443), yaw2183; at1436
(2680,5271,800), Venom gone. Fall scripts at2594, removal2662. Another failed
crossing; the first roof's sequence cannot be assumed correct for the second.
Inspect the second roof/edge before further repeating jump offsets blindly.

`docs/chase-normal-route.md` now holds the compact confirmed sequence and failure
comparisons. The native route-state decoder's relative-tick and extra-button
fields compile and were exercised on real snapshots. No game code/timing changed
in this continuation. Five native route runs are terminal and cleaned their own
copied EXEs/extraction caches. Two images were produced and both inspected;
failed runs remain retained. P0/P1 and cross-hardware acceptance remain open.

### Second-roof inspection and ordinary progress beyond waypoint 16

`chase-second-roof-inspection`: 2,700 ticks, 59.422 s, exit zero, PID29744
terminal. One native capture at2500 inspected in full: Spider-Man runs into a
large rooftop wall/building face, with a visible route around its left side.
This is a different obstacle from the first roof's antenna. Three RAM snapshots
confirm the existing checkpoint at approximately (2213,906,-1765), script inactive.
Do not assume its edge is in the same place relative to the obstacle as roof one.

`chase-second-roof-walk`: 3,800 ticks, 68.594 s, exit zero, PID22796 terminal;
no images. Stop R2 at+1200, add Left +1200:90 while Up continues. Atrelative1228
player (2181,906,-1747); at1328 (1585,2274,-947); later fall/game-over scripts.
The diagonal walk leaves the narrow roof edge. This brackets the edge for a
jump, rather than proving collision or frame timing broken.

`chase-second-roof-edge-jump`: 4,000 ticks, 71.906 s, exit zero, PID17940
terminal; no images. Confirmed base route, R2 +980:220, Left +1200:60,
Cross +1260:12, R2 +1320:2600, uninterrupted Up. It advances naturally past
Venom's wait at point16: script progresses through 18,22,23 in the snapshots.
Atrelative1407 player (1648,1669,1477), Venom (4347,1165,3695); by1557
player is (1518,1233,3093) with yaw0 and almost fixed X/Z, then moves vertically.
Venom continues rightward/ahead; the player loses him around absolute2997 and
removal occurs3257. This is meaningful ordinary route progress, not completion.
No follower, artificial region pulse or position write was used.

`chase-third-swing-right`: 5,200 ticks, 97.906 s, exit zero, PID29480 terminal;
no images. Adds Right +1350:60 during the already-started +1320 swing. It does
not materially change the route: atrelative1413 (1635,1689,1580), yaw1964;
by1563 (1518,1194,3093), yaw0. Lost-Venom at2991. The nearest retained
snapshot is just after the Right interval, so do not claim an observed native
Right edge from these snapshots. The log records the harness press. Next useful
comparison is the heading BEFORE the swing begins; do not infer unrestricted
air steering or keep extending this same held-swing direction into the wall.

All four runs are terminal, caches/copied EXEs cleaned. Full route-state reports
were regenerated after completion. Only one image was produced and inspected.
No game/runtime behavior changed; ordinary full Chase and P0 remain open.

P0 read-only follow-up: queried Windows System events for 2026-09-18
03:03:30Z..03:07:30Z, covering the prior stack-sampled long-stall cluster.
Get-WinEvent returned NoMatchingEventsFound, not access denied. Report retained
as sm1-thread-stack-stalls/system-warning-events.json with explicit status and
bounds. No logged power/driver/reset event was found; absence does not exclude
OS/driver stalls. The current token privilege listing lacks SeSystemProfilePrivilege;
WPR profile enumeration is available, but no recording/elevation or host-setting
change was attempted. Per-process diagnostics remain the available evidence.


## Refreshed full package: audio recovery and direct Chase startup

`proof_render/OpenSpidey-Gateway-Chase-Test-AudioRecovery.zip` is 1,025,645,926
bytes. The builder verified all 2,074 manifest entries. Both full local games,
movies/audio and launchers are included. The older ZIP remains preserved.

Isolated verification in `proof_render/gateway-audio-recovery-verification`
launched the actual packaged EXEs from a foreign working directory: SM1 exited0
in46.672s, SM2 exited0 in88.672s. Both executable hashes match BUILD.json and
the current canonical audio-recovery builds. SM2 frame4500 was inspected: player,
rooftop, surrounding buildings, tutorial question mark and HUD are present.
The initial SM1 frame1700 showed only the intro rooftop; it was insufficient
for player/gameplay acceptance, so a targeted follow-up was required.

`proof_render/gateway-audio-recovery-chase-review` used the new verifier's
`--game sm1` option and frame1900. It exited0 in47.922s; its inspected capture
shows Spider-Man, the rooftop/city, Chase HUD and "CHASE VENOM TO HIS HIDEOUT".
This verifies packaged startup into the requested scenario, not its completion.
No physical input, full movie playback or speaker audio was verified here.
Only scratch extracted settings were muted; the ZIP retains normal sound.
The verifier automatically removed its owned extraction/cache after each run,
retaining logs, hashes and the three inspected captures. No game is left by
these completed verification runs. CLI output paths must be fresh and inside
proof_render; unsafe ZIP paths are rejected. Python compilation passed.

Ordinary-route `chase-before-swing-right` failed after a confirmed pre-swing
Right hold changed heading but overshot toward the opposite wall. See the
ordinary-route handoff for exact positions, input states and the next shorter
turn comparison. No gameplay/runtime change was justified by that experiment.


### Shorter pre-swing turn comparison

`chase-before-swing-right-short` changed only Right +1280 from35 to15 ticks
and ran4200 ticks (74.985s, exit0). Eight RAM snapshots, no images. At
relative1289 Right was held, yaw2408, player(2038,513,-1258); by1319 yaw2404,
player(2273,1436,-754). At1419 the player was(3415,1591,1052) and Venom had
naturally reached script waypoint18. At1569 player(4021,1367,2102), yaw2389,
Venom waypoint22, distance5281. By1719 a failure script owned the player and
Venom was gone. This is another failed ordinary route, not completion evidence.

The shorter turn avoids the previous far-right wall position near(5299,1731)
and advances Venom naturally. It does not establish a physics defect. Next
inspect the player/collision state around relative1450–1600 before choosing the
next jump/release; the later position alone does not prove a grounded landing.
All these route tests include RAM capture overhead and are not performance
acceptance. The package verification and this diagnostic have exited; owned
runtime/executable copies were cleaned up. No diagnostic game remains running.


### Third roof: observed running, edge bracket and next jump

`chase-third-crossing-inspection` (2900 ticks,53.484s,exit0) repeats the
15-tick pre-swing Right route. Its only image, frame2700, was inspected: the
player is running on a narrow roof beside a tall building, with city/HUD intact.
At relative1497..1607 Y stays1367 while X/Z advance from(3658,1527) to
(4207,2479). By1627 Y1376 and1647 Y1873 indicate leaving that roof edge.
This supports timing the next jump before the drop; no native grounded flag
has been identified, so the position is not advertised as a decoded flag.

`chase-third-roof-jump` shortens R2 +1320 to230 ticks, adds Cross +1600:12
and a fresh R2 +1660:3000, retaining earlier buttons. 4200ticks,76.047s,
exit0; eight snapshots, no images. It naturally advances Venom through
waypoints23,24,26,28,30,31, farther than the previous route. Atrelative1801
player(6276,2107,6007),1951(6277,1403,6009),2151(7090,3091,7422),
2451(7090,1137,7422); later Lost-Venom and actor removal. Vertical movement
at fixed X/Z still suggests intervening walls; inspect the next consequential
position before adding more turns. This is failed-route progress, not completion.
Neither run changes game physics, trigger regions, or actor positions.


## September 18: general diagnostic logging blocking risk

Source review found both games' Diag.Write still synchronously acquiring a lock
(up to2000ms), writing and flushing spidey.log, with Tee also writing the original
stdout/stderr sink on the calling thread. The already-bounded PerformanceLog did
not cover this independent general logging path. These calls can block gameplay
on a slow file/redirected pipe. This source finding does NOT explain the observed
IRQ-wait stall or prove the recorded multi-second cluster was caused by logging.

The shared AsyncDiagnosticLog now owns file AND console writes on one background
worker. Producers use TryWrite into1024 entries; text is capped at16384 characters,
and unterminated Tee lines are emitted in bounded chunks. Overload is counted,
not silently interpreted as complete evidence. The latest fatal report has a
reserved slot. Accepted ordinary records retain order; fatal output can precede
them. Prefixes preserve enqueue-time/frame metadata. ProcessExit flushes partial
Tee lines and drains for at most2s. Hard termination, a blocked sink, or an
exhausted shutdown interval can still lose pending output. No host priority,
clock, game script or rendering setting changed.

`dotnet run --project tools/RecompOne/tests/DiagnosticLogRegression -c Release`
passed20 assertions (diagnostic-log-regression.log): deliberately blocked file,
blocked console,10000 producer submissions with exact overflow count, bounded
shutdown, recovery/drain, fatal retention under saturation, ordinary ordering,
oversized text cap and throwing-file/working-console independence. This exercises
the actual shared source, including concurrent producers and an idle fatal report.
Native integration results follow below.
The existing AudioRecovery ZIP does not contain this newer logging change.


### Background general logging: both native integrations complete

Both canonical publications succeeded; logs are publish-sm1-diagnostic-log.log
and publish-sm2-diagnostic-log.log. Existing nullable/unused-field and MonoMod
single-file warnings remain; no new build error. Executable SHA256:

- SM1:5AC632E462DE77FFF0F8CBEEDB7A8560F9C0999DD5BF47F8230FDE834225D185
- SM2:B1D878849F1554305FB3CF8851871382427F0E9376F98DB2F6AF42140E700E47

`sm1-diagnostic-log-integration`:4200ticks,75.828s,exit0. All203 nonblank
post-install console lines occur in order in spidey.log, including the final
exit record. The three pre-install initialization prints are intentionally
outside the logger's scope. No loss/truncation/error report.72 audio observations,
none nonplaying after5s. Its only image,frame3400, was inspected: player is
wall climbing against the later tall building with city/Chase HUD present.
The ordinary-control route later fails; this is not Chase completion.

`sm2-diagnostic-log-integration`:4300ticks,76.000s,exit0. All112 post-install
console lines occur in order in spidey.log, including exit; three pre-install
prints excluded. No loss/truncation/error report.74 audio observations, none
nonplaying after5s. Repeats the previously inspected short jump/swing fixture;
no new image, so no new visual/full-gameplay acceptance claimed for this run.

Both are muted speaker-gain runs with real mixing/device queues active. Queue
state is not audible correctness. These short integrations do not prove the
rare-stall problem fixed. Per-run integration-summary.json records the checks.
Owned EXEs and runtime caches were removed by each runner. All four native runs
in this continuation are terminal; two images total, both inspected. Regression
has20 passing assertions; git diff --check passed. The available full-game
AudioRecovery ZIP remains verified but predates this general-logging change.
Next P0 work must continue from the retained IRQ/render/scheduling stall evidence;
removing synchronous general I/O is a verified blocking-risk fix, not its cause.


## September 18: independent host counters and render-wrapper audit

The retained long stalls have little whole-process CPU, raw sample gaps and
independent heartbeat delays, but no contemporaneous host busy/DPC/frequency
counters. New `tools/performance_host_counters.py` reads ten local Windows PDH
counters at1s intervals without elevation or host-setting changes. The actual
self-check produced valid uncapped performance values (for example115.922%),
3samples in~3s and0.015625s collector CPU. First-sample rate errors are retained.
Neither psutil's nominal3801MHz nor this provider's aggregate percentages are
measured per-core clock speeds. The stall join preserves collection envelopes,
endpoint-gauge limitations and all errors rather than interpolating a frame value.
Eleven correlation tests pass. Usage/API references are in performance-validation.

Read-only renderer audit: Runtime pins Silk.NET2.22.0; HostWindow sets
FramesPerSecond=0 and leaves automatic buffer swapping enabled. The exact upstream
v2.22.0 ViewImplementationBase.DoRender executes pending invocations, GL context
checks/optional make-current, swap-interval update, Render callback, then buffer
swap. No artificial render-period sleep appears in that method. Source:
https://raw.githubusercontent.com/dotnet/Silk.NET/v2.22.0/src/Windowing/Silk.NET.Windowing.Common/Internals/ViewImplementationBase.cs
(lines158–185). Local host source has no window.Invoke callers. The old1224ms
render wrapper with0.230ms callback therefore is not evidence of expensive game
rendering inside OnRender. Swap/context work and thread scheduling remain distinct
possibilities; source inspection alone cannot attribute the recorded delay to
SwapBuffers. No render/swap behavior changed in this continuation.


### Host-counter reproduction result — clean, not resolution

`sm1-host-counter-stalls` repeated the earlier SM1 GL33 l5a3 stall scene using
the current background-logging/audio-recovery build.18000 console ticks,
304.829s, game/GC-contention collector exit0. No screenshots, RAM snapshots or
CPU-sampling profiler; real audio queue with muted output gain. Target PID23428
is terminal. Independent host-counter and heartbeat processes also exited after
target exit. Owned executable/runtime cache cleaned; retained evidence45,706,083
bytes. No native timing/rendering/game behavior changed in this continuation.

After60s:7231 presentations over241.015s,29.998 native updates/s;
p9533.3380ms,p9933.3406ms,max34.1713ms,zero intervals over50/100ms.
Frame logs:zero dropped/partial records. Trace:387 GC/contention events,zero lost.
Heartbeat:13699 intervals,max21.5937ms,zero delays over50ms;0.6875s own CPU.
Audio:180 observations,none nonplaying after5s; coverage is bounded to180s,
not the entire run, and speaker output was not assessed.

Host collector:286 samples,1.984375s own CPU,max collection cost11.8133ms.
Initial unprimed rates are unavailable. One later processor-performance sample
returned status2147485654 and is preserved as null/error; no substitution with0.
Valid whole-run ranges: busy3.61–54.42%, performance85.88–118.27%, reported
maximum-frequency fraction38–100%, DPC0–0.873%,interrupt0–1.747%,
processor queue0–1,available memory11317–14422MiB,disk queue0–7.
These whole-run extrema are not matched measurements during a stall: no stall
recurred. They show that provider-reported frequency variation can coexist with
stable updates here, not that clock/power behavior is irrelevant on all machines.
The original multi-second failures remain unresolved and must not be superseded
by this clean run. Reports:report.json,stalls.json,host-counter-summary.json,
host-counters.jsonl,heartbeat.jsonl,runtime-events.jsonl and cpu.nettrace.

Next P0 gate: use this collector on the next meaningful movement/movie or known
failing reproduction; do not keep repeating clean idle soaks as acceptance.
The render-wrapper audit narrows possible native work but still does not split
buffer swap/context wait from thread scheduling. Full ordinary Chase, unresolved
geometry and both-game/cross-hardware acceptance remain open. Eleven analyzer
tests and Python compilation passed; git diff --check passed.


### Pre-third-swing Left comparison

`chase-third-swing-left` adds Left +1620:8 to the prior third-roof jump
(R2 released+1550, Cross+1600, R2+1660).4800ticks,85.469s,exit0,
eight snapshots,no images. Atrelative1631 player(4376,991,2874),yaw2122;
1731(5410,2116,4661),yaw0;1931(4998,1596,5801),yaw1822,
Venom waypoint24/26. By2131 failure owns the player and Venom is gone.
The short turn changes the approach and reaches a different roof/edge, but loses
Venom earlier than the straight route. The first snapshot is after the8-tick
hold: heading changed, but no held-button sample is claimed. Next inspect or
bracket the roof aroundrelative1900–2000 before timing another jump. Do not
interpret a single Y1596 sample as a decoded grounded flag. Best advancing
attempt remains chase-third-roof-jump (waypoint31); this comparison is retained.


## September 18: unskipped movie shutdown exposes missing SM1 CD binding

Added explicit `--keep-boot` to the native runner. Defaults remain the existing
quick-start diagnostics; this option removes SPIDEY_BOOT_SKIP_UNTIL so the
startup movies run without automatic Start pulses. Title-anchored inputs remain
armed until the title loads. Python compilation passed.

`sm1-boot-playback`:7200ticks,125.953s,exit0. Its sole frame1500 was inspected:
the Neversoft movie/logo is visible, not a blank capture. The log shows two CD
timeouts during movie transitions: CD_sync atframe799 and CdlPause at828,
roughly0.48s apart. Startup proceeds to shell/title and selected l5a3 gameplay.
This establishes a reproducible transition fault, not full movie/audio acceptance.
Movie-phase timing:803 presentations across28.726s (phase spans can include
intervening content),p99 40.9861ms,max73.9054ms; one interval over50ms.

Source cause: main.json called address80086F18 `func_80086F18`, even though
psyq_main.json.syms.json identifies CdControlB. The332-byte retail wrapper
calls CD_cw then CD_sync and compares completion with2. Generated movie teardown
atRA8002B02C calls it immediately after StUnSetRing with command9 (Pause).
Disc-status loops atRA8002B1A4/8002B1D8 call the same wrapper. Because its name
was missing, it polled retail CD IRQ state while other commands used HLE state.
SM2's corresponding80093878 is already named CdControlB and generated as the
SDK forwarder. This is a specific transition fault; it does not explain the
retained long stalls during ordinary SM1 l5a3 gameplay.

SM1 main/manual maps now name CdControlB; the manual seed records why. Regeneration
through build.recompile (including costume/texture transforms) succeeded and
all three callers plus dispatch80086F18 now use the SDK forwarder. The existing
shared SDK implementation was sufficient; no clock or global CD behavior changed.
TimingRegression now has307 passing assertions, adding movie teardown blocking
Pause completion, status/result bytes and subsequent CdSync completion. Publish
succeeded; SHA2560F09514586FA7016898FF1D04D515618E94C9AD8E461D38DA33184F4C8803D5E.
Logs:recompile-sm1-cdcontrolb.log,cdcontrolb-timing-regression.log,
publish-sm1-cdcontrolb.log. Native retest results follow after completion.

A broader symbol audit saved cd-sdk-name-audit.json: SM2 has no mismatched supported
Cd*/St* names; SM1 additionally has unnamed CdSyncCallback80086C80,
CdReadyCallback80086C94 and CdDataCallback800870C4. Their bodies are identified
(two pointer swaps and one DMA callback wrapper), but reachability/state ownership
needs review before changing them. Native legacy reader routines call these;
many callers may be replaced wholesale by the SDK. They are candidates, not
three more demonstrated defects. Existing mappings remain for that audit.


### SM1 blocking-CD fix: unskipped native retest

`sm1-boot-cdcontrolb-fixed`:4200ticks,77.985s,exit0; zero CD timeout lines,
versus the two retained baseline timeouts. Title anchor2154 (baseline2208),
then selected l5a3 loaded and trigger scripts executed. The sole frame1500 was
inspected: Neversoft logo during its closing graphic transition. This differs
from the baseline's movie instant and is not a claim of pixel-matched animation.
73 audio observations,none nonplaying after5s. Both runs mute speaker gain;
audible correctness and every movie frame are not verified. boot-review.json
and report.json retained with the baseline. The specific timeout/binding defect
is locally fixed; full P0/P1 and ordinary Chase completion remain open.


### SM2 unskipped boot check and remaining movie pacing gate

`sm2-boot-playback`:6000ticks,107.219s,exit0. No CD timeout messages.
Frame1500 inspected: Vicarious Visions logo/movie on a lit brick wall. Logs
continue through title and e1m0 level load; intro-confirmation inputs fire.
105 audio observations,none nonplaying after5s. This is a decoded-video checkpoint
and transition/logging check, not full animation, speaker-audio or combat acceptance.
SM2 already had the CdControlB SDK mapping and its executable is unchanged.
Owned runtime cache/EXE cleaned; boot-review.json and report.json retained.

Preserve the timing outliers: movie phase1438 presentations,p9973.427ms,
max89.3942ms,30 intervals over50ms,none over100ms. Nonmovie/menu/loading
phase has5 intervals over100ms,max369.8732ms. The phase spans are disjoint;
its7.794 native updates/span second is NOT gameplay speed (menus and intervening
movies are included in the span). One screenshot contributes overhead, but cannot
be assumed to explain all30 movie outliers. Next compare unskipped playback
without captures, then localize remaining movie wait/decode/queue costs. Do not
mark movie pacing accepted from rendered content or a playing audio queue.

This continuation produced three images total (SM1 before/after and SM2 boot),
all inspected. Four native runs are terminal: failed Left-turn Chase comparison,
SM1 movie baseline, SM1 CD fix retest and SM2 movie check. No diagnostic game
remains running. Tests307 timing assertions, Python compilation and diff check
passed. P0/P1 remain open; retained normal-game stalls, full Chase, remaining
geometry, complete movie/audio behavior and independent hardware acceptance
still require work. Available full ZIP predates the CD-binding/logging changes.


## September 18: full movie ring slept while holding the consumer lock

`sm2-boot-no-capture` repeated unskipped boot with no screenshots/snapshots.
6000ticks,114.281s,exit0. Movie phase1425 presentations,p9945.7283ms,
max133.588ms,14 intervals over50ms and2 over100ms. Hitches therefore persist
without image capture. This run overlapped local compilation/testing near its
end; it is not an uncontended performance baseline. Retain all outliers.
Its1,229.891ms maximum is at1.516s after the first retained presentation,
present3, outside-present1220.347ms. This occurred BEFORE local test compilation;
the initial commentary's "later" characterization was corrected after inspecting
stalls.json. It cannot be attributed to that compilation. No heartbeat/host/GC
trace was collected here; missing overlap evidence does not exclude host or GC.
This startup interval is separate from prior mid-game l5a3 stall clusters.

Shared LibCdStream.StreamLoop inspected under full-ring pressure: the producer
called Thread.Sleep(1) INSIDE `_lock` when its next slot was busy. StGetNext
needs that same lock to dequeue/release a frame, so the consumer could wait for
the producer's sleep and repeated reacquisition. The sleep now occurs after
leaving the lock. Availability check, payload collection, publication and frame
ordering remain unchanged; both games share this fix. No timing/clock change.

New MovieStreamingRegression uses the real producer/consumer and a synthetic
single-slot movie ring. It forces repeated full-buffer encounters, consumes12
frames, checks every payload/header/order and forward progress, then stops/joins
the worker.40 assertions passed before and after. Observed maximum StGetNext call
was12.701ms before and0.583ms after (single-run observations, not universal timing
bounds). The assertions validate contents/progress, not fragile sub-ms deadlines.
Logs:movie-ring-before.log, movie-ring-after.log.307 timing/CD assertions also
passed (movie-ring-timing-regression.log). Both publications succeeded with existing
warnings:publish-sm1-movie-ring.log and publish-sm2-movie-ring.log.

The runner now supports `--host-counters` to start both bounded host-counter and
heartbeat witnesses immediately with its owned game process, record their exit
codes and join/terminate only those children during cleanup. This avoids missing
manual setup on later runs. No desktop control or host-setting change. New native
post-fix comparisons run serially after builds, with witnesses and no captures.


### SM2 movie-ring post-fix native comparison

`sm2-boot-ring-unlocked`:6000ticks,107.875s,game and both witness exits0.
No captures/snapshots or concurrent builds. Movie phase1437 presentations,
p9937.7524ms,max52.9635ms,one interval over50ms andnone over100ms,
versus14/2 in the no-capture pre-fix run. This is consistent with the lock
improvement, but host variability and the earlier run's overlapping late test
compilation prevent attributing every difference to the fix. No universal pacing
claim. Nonmovie/menu/loading retains12 intervals over100ms,max384.0162ms.

The remaining movie outlier is7.938s,present193:52.9635ms interval,
51.7093ms outside presentation,previous render callback1.0611ms; event/input
costs small. Its coarse host envelope (~1011ms) reports17.16% total busy,
112.13% performance,70% reported frequency fraction,DPC0.194%,interrupt0.291%,
queue0. No heartbeat delay; whole-process CPU envelope is too coarse to assign
main-thread cost. No GC trace in this SM2 run, so no managed-pause exclusion.
Heartbeat5281 intervals,max42.3768ms,zero delays over50ms.104 audio observations,
none nonplaying after5s; muted output, not audible verification. Both witnesses
self-ended and the runner recorded their zero exit codes. Reports/logs retained.


### SM1 movie-ring integration and final state of this continuation

`sm1-boot-ring-unlocked`:4200ticks,78.968s,game/GC collector/both witness
exits0. No images or concurrent builds.611 GC/contention events,zero lost.
Movie phase801 presentations,p9943.0733ms,max80.7252ms,one over50ms,
none over100ms. That80.7252ms interval occurs0.256s after the first presentation,
with74.971ms outside-present time and no managed suspension in trace coverage.
No claim that movie startup is fully smooth. Nonmovie/menu/loading retains
five intervals over100ms,max631.7694ms. The~51s cluster aligns with the
l5a3 level-load log around00:39:02; it is not a recurrence established during
steady gameplay. No managed suspension overlaps those five intervals; some
host heartbeat delays overlap the631.8ms interval. Do not infer exact CPU/driver
cause from coarse envelopes or exclude these loading outliers from acceptance.
Heartbeat3734 intervals,ten delays over50ms,max58.6041ms.74 audio observations,
none nonplaying after5s. Zero CD timeout messages; prior SM1 CD fix retained.

Latest canonical SHA256:
- SM1:191B4719676A480F92473547F8C4BB216FA6517E68EC36EAFB5A646FF622CD32
- SM2:D7F3B3326514FF6090DE2A2E07FCBD0BABAF4700B155C56B4205B1707165E738

Both post-fix integration-summary.json files confirm owned executable/runtime
cleanup and witness exits. No new visual or speaker-audio acceptance is claimed;
this continuation generated zero images. Three native runs are terminal. The
shared lock fix is retained with40 synthetic streaming assertions and307 timing
assertions. Python compilation/diff check passed. Native movie pacing improved
in the SM2 comparison but does not establish a universal before/after benchmark.
Rare normal-game stalls, loading outliers, full Chase, remaining geometry and
cross-hardware acceptance remain open. The available full ZIP predates this fix.

### Movie worker hard-reset lifetime: reproduced and fixed

Previous goal turn made progress by identifying the antenna obstruction and
advancing the ordinary route past it; no blocker applies. This continuation
reproduced the movie-worker reset concern rather than changing timing speculatively.
MovieStreamingRegression holds an actual disc read across Reset. Before the fix,
Reset returned while the old worker remained in that read (movie-reset-before.log,
failing assertion). This permits old work to touch cleared state and potentially
survive when a new boot sets the shared run flag true.

LibCdStream.Reset now stops and joins the producer outside the ring lock before
clearing state or dropping its reference. CollectFrame checks cancellation during
sector scanning and after disc reads, so malformed/nonvideo scanning can also
retire instead of looping indefinitely after stop. A genuinely blocked underlying
disc read must finish before reset can safely replace memory/disc; the code does
not pretend a timeout makes a live worker safe. No steady-state clock, frame cap,
priority, movie order or audio setting changed.

The regression releases its controlled read, verifies reset completion with the
old worker dead, starts a new ring/stream and checks fresh frame0. All47 assertions
pass (movie-reset-after.log); all307 timing assertions pass
(movie-reset-timing.log). Both Release publications succeeded with existing
warnings. Latest canonical executables:
- SM1 SHA256 8455F6FD1C097B57B37C965EA0190DD6EE9A6E8D05B592ECCCBC3044A55821CC
- SM2 SHA256 CD40514CAAE4BBC801155FE0782B0C4644C30FA21B6D1374A5D0EAD0253764E4

No native game or images were launched this continuation. Native hard-reset and
full transition acceptance remain unverified; synthetic regression/build success
is not that evidence. Existing full ZIP still predates recent fixes. P0/P1 and
the goal remain open. Next investigate equivalent LibCd XA worker reset lifetime
and ring/stream replacement during an in-flight collection with controlled tests,
then return to ordinary Chase onward route and retained non-GC stall evidence.

### XA worker hard-reset lifetime: reproduced and fixed

The previous turn was progress (movie-worker fix and failing/passing regression),
not a wait or blocker. This continuation applies the same controlled experiment
to the real LibCd XA producer. FilteredDisc can gate a read. Before the fix,
Reset returned while that read was held: xa-reset-before.log retains the failing
assertion. LibCd.Reset now clears the run flag, joins outside _xaGate/DiscLock,
then clears the worker reference and CD state. PumpXaLocked checks cancellation
while scanning and after the disc read, and suppresses tail pumping after stop.
As with movie reset, a blocked underlying read must finish before safe teardown.
No console clock, 30 FPS cap, steady audio buffer or power setting changed.

The first post-fix test passed shutdown but failed restart because its fixture
omitted Setloc after reset had cleared location; xa-reset-after.log is retained.
Supplying the normal 00:02:00 start location fixes the fixture. Final log
xa-reset-after-location.log has314 PASS assertions, including seven new checks:
held producer read, reset completion, no early return, old worker terminated,
stopped reads after reset, replacement worker reads, old worker stays dead.
MovieStreamingRegression retains47 passing assertions in xa-reset-movie.log.
Both canonical Release publications succeeded (publish-sm1-xa-reset.log and
publish-sm2-xa-reset.log), existing warnings only. Latest SHA256:
- SM1:1C212BFE6FB64D49BA86D01640861E195631CBA02B5EDE5BDB4766BA532DDDCD
- SM2:AF051F5A601AD93C3365685AB26F13C064AE41714DD0C78C1F1E660AC607CFCB

No native game, images or new ZIP this continuation. Diff whitespace check passed.
These changes cover a demonstrated reset lifetime defect shared by both games;
they do not establish that historical non-GC gameplay stalls had this cause.
Still open: ring replacement during in-flight collection, native transition/reset
acceptance, full ordinary Chase (next route near4003/9280), sustained movement/
combat/audio/rendering and independent hardware. Keep P0/P1 and the goal active.

### Ordinary Chase: fourth ledge inspection and failed turn comparison

Previous turn made progress (XA reset fix with failing/passing evidence). This
continuation returned to the primary ordinary-route acceptance gate using current
canonical executables and ordinary process-local buttons only.
chase-fourth-roof-inspection:3550ticks,64.610s,exit0. Sole native capture at
relative2350 inspected in full: Spider-Man running against a wall on a narrow
ledge; city and HUD present; direction arrow points right. RAM brackets arrival
at2239(4003,578,8580),2279(4003,577,8960),then unchanged(4003,577,9280)
through2319..2439. This confirms an obstruction, not a clock defect.

chase-fourth-roof-right-jump:4800ticks,90.312s,exit0,eight snapshots,no images.
Released R2 at2220, Right2250:35, Cross2280:12, R2 resumed2340. It still loses
Venom. At2271 the player is still climbing, unlike the old route's roof position;
earlier release changed climb timing. By2461 player(3589,318,9817),Venom30/31,
distance7447. Next preserve climbing input until the verified roof stop before
attempting the onward turn/jump. Exact route is in chase-normal-route.md.

Both native runs terminal, owned runtime/executable copies cleaned by runner.
One image generated and inspected this continuation. These snapshot/capture runs
are route evidence, not sustained-performance or audible-output acceptance.
No new runtime changes or ZIP. P0/P1 remain open; retained long stalls and both-
game movement/combat/transition/audio/cross-hardware gates are unchanged.

### Ledge timing: native input verified and later jump rejected

Previous turn made progress by identifying a wall obstruction and showing that
an earlier release altered climbing. This turn's chase-ledge-late-right retains
R2 to2320, turns Right2325:35, jumps2340 and resumes R2 at2400.4500ticks,
80.078s,exit0,eight snapshots,no images. At2342 player remains at the known
ledge,Right held,yaw2504; subsequent position clears obstruction and reaches
Venom script31/32 by2512, then loses him. Button and heading evidence confirm
this attempt actually turned on the ledge. It is not full completion.

chase-ledge-long-right extends Right to70 and delays Cross2380/R2+2440.
4600ticks,81.969s,exit0,eight snapshots,no images. At2369 Y1338 and2399
Y3022 show descent before that delayed Cross, followed by fixed-X/Z vertical
movement near4928/9782. This cannot establish whether the longer heading would
work with the earlier successful jump. Next change only Right duration while
retaining Cross2340/R2+2400. Exact coordinates are in the ordinary-route handoff.

Both games' runtime source remains unchanged this continuation. Read-only
FrameClock/IdleTick callsite review did not establish a new root cause for the
retained rare stalls; no speculative timer changes. Both native runs terminal
and runner cleanup completed. Zero new images. P0/P1 and all broader acceptance
requirements remain open; no new ZIP or claim of performance resolution.

### Isolated ledge heading comparison

Previous goal turn was progress: it distinguished a registered ledge turn from
a jump issued after leaving the ledge. This continuation ran
chase-ledge-heading-only with current canonical executable. Only Right duration
changes35 to70 relative to late-right; Cross2340 and R2+2400 are preserved.
4500ticks,80.156s,exit0,eight snapshots,no images. Relative2371 reaches
(4455,228,9512),yaw2662;2411(4930,1170,9782),yaw0;2461 remains near
that X/Z while Y falls to988. Failure owns the player by2561. This avoids the
delayed-jump attempt's initial deep drop but encounters another wall; no full
completion or improvement beyond prior script31/32. Next isolate R2 initiation
with the earlier jump, or inspect that crossing. Do not change console speed
based on failed navigation. Detailed route-state retained with original logs.

Native run terminal and owned runtime/executable cleanup completed. Zero images,
no new runtime edits/build/ZIP, and no performance acceptance claim. Read-only
movie-worker reset review confirms the recent join fix remains in source; it
makes no new statement about ring replacement races or old gameplay stalls.
P0/P1 remain open; both-game and cross-hardware requirements remain unchanged.

### Movie StUnSetRing teardown: reproduced, fixed, native integration in both games

Previous turn produced route evidence, so it was progress rather than a blocker.
This turn reproduced another shared lifetime defect. The actual movie producer
was held inside a synthetic disc read while StUnSetRing ran. It returned before
the read was released (movie-unset-before.log). This proves teardown did not
retire the worker, not that a particular historical gameplay stall was corruption.
StUnSetRing now uses StopWorker, shared with hard reset, joining outside the ring
lock before returning ownership of ring memory to the caller. Existing cancellation
checks stop in-flight collection after its disc read. Underlying blocked I/O must
finish before safe teardown; no unsafe timeout abandons a live writer. Active ring
replacement/StClearRing/StSetStream races are not claimed fixed by this change.

MovieStreamingRegression now passes51 assertions including blocked-read teardown
and no surviving producer; TimingRegression passes314. Logs movie-unset-after.log
and movie-unset-timing.log. Both canonical Release builds succeeded, existing
warnings only. Latest executable SHA256:
- SM1:7337E933E90B86218461BB5BA6FDF058E3460C8E639E62174D3789AF635B9F5D
- SM2:4F2671214A6F84F3F3FBDA40692507A1D6B80914FC2594C903D02488DCC1FAC8

After both builds completed, serial native runs retained full boot playback:
- sm1-movie-unset-integration:4200ticks,76.531s,exit0; title anchor2158,
  first-level l5a3 scripts execute.805 movie presentations,p99 38.6711ms,
  max70.3372ms,two>50ms,zero>100ms. Game/menu/loading max283.5194ms,
  six>50ms,three>100ms.73 audio observations,none nonplaying after5s.
- sm2-movie-unset-integration:6000ticks,106.234s,exit0; title2465,
  e1m0 anchor3168.1440 movie presentations,p99 37.8187ms,max54.0554ms,
  one>50ms,zero>100ms. Game/menu/loading max499.5015ms,eleven>50ms,
  four>100ms.104 audio observations,none nonplaying after5s.

Both console logs have no CD timeout. Reports and raw outliers retained. No
concurrent builds, screenshots, GC trace or host witnesses; do not assign outlier
causes or call this a controlled performance comparison. Logs prove transition
progress, not visual/functional correctness of every frame or audible playback.
Speaker gain muted; real audio device/mixer remained active. No native hard-reset
was invoked: that path's coverage remains synthetic. Both games are terminal,
owned runtime/executable copies cleaned. Zero images and no ZIP this continuation.
P0/P1 remain open, including full ordinary Chase and retained long gameplay stalls.

### Earlier ledge swing clears the wall, then hits the fall-fog failure

Previous turn made progress by reproducing/fixing unsafe movie teardown and
checking both games' native boot transitions. This turn isolated one ordinary
Chase input: R2 starts2370 instead of2400, with Right2325:70/Cross2340 unchanged.
chase-ledge-earlier-swing:4600ticks,81.625s,exit0,eight snapshots,no images.
Player passes the prior wall at4930/9782 and moves from(4992,578,9903) at2412
to(5394,577,10201) at2462. Atrelative2551 scripts800E438C/800E4DFC fire;
the latter contains FallFog, followed by player-script control and removal.
This differs from the earlier Lost-Venom failure and is not completion evidence.
Next bracket/inspect that roof edge after2462 before scheduling the next jump.
All buttons were process-local ordinary inputs; no actor/region writes.

The opening handoff status was also corrected: prior antenna-only summary was
stale, and the ZIP caveat now includes all newer reset/teardown fixes. Route
handoff remains authoritative for newest isolated comparisons. Native run terminal,
owned runtime/executable cleaned; zero new images or runtime code changes.
Full Chase, retained gameplay stalls and both-game/cross-hardware acceptance
remain open. No goal completion or laptop-test request is justified yet.

### Roof edge inspected; route comparison exposed prior divergence

Previous turn was progress (wall cleared and fall-fog endpoint identified).
chase-next-roof-edge:3670ticks,66.453s,exit0,eight snapshots,one image.
Native capture atrelative2480 inspected: player airborne across gap,Venom on
roof ahead,city/HUD intact. Y578/578/577 at2422/2442/2462 becomes1007 at2482,
1945 at2502,3150 at2522,4495 at2542 before FallFog. This brackets the drop.

chase-next-roof-jump releases R2 at2430,Cross2450,R2+2480;5000ticks,88.765s,
exit0,eight snapshots,no images.2472 player(5185,541,10107),then Lost-Venom
at2520/2524. Crucially, the route had already diverged before the changed input:
both runs report executable SHA7337E933...,level anchor1088 and the same applied
buttons before2430, but trigger800E48D8 occurs at3133 versus3094 (39ticks).
Both first snapshot/capture points are later than that trigger. This invalidates
strong one-run causal interpretations of input-duration changes in earlier notes;
those remain observed outcomes, not controlled deterministic comparisons.

NEXT: identical-input/snapshot control runs, localize first divergence, inspect
console-tick/native-update/input scheduling and any random state. Do not blindly
add more jump timings, modify positions/triggers, or assume global clock changes
are justified. This may be harness/game variability; cause is not established.
Both runs terminal and cleaned; one image generated/inspected. No runtime source
change or new ZIP. P0/P1 and full acceptance remain open.

### Identical-input controls: checkpoint alignment and counter evidence

Previous turn made progress by discovering divergence before the intended route
change. This continuation ran identical control scripts/snapshot schedules serially:
chase-control-a3250ticks/60.219s, chase-control-b3250ticks/60.453s;bothexit0,
eight snapshots each,no images. Anchors1088/1090 mean their absolute snapshots
are offset two relative console ticks. Paired native button states all match.
Positions differ, but those are not equal-relative-time samples; do not claim
that alone proves nondeterministic physics. chase-control-comparison.json retains
positions,relative ticks and counters. Read-only decoder now emits game_counter
from800B4F38, already used by the runtime performance logger.

chase-control-counter-alignment.json aligns presentation indices to level anchors
(two console ticks per presentation; samples are before IRQ delivery, not native
update-entry traces).801 matched samples across relative500..2100 show constant
B-minus-A counter=-1. No accumulating rate difference is demonstrated.
chase-prior-counter-comparison.json compares the disputed next-roof-edge/jump
pair at matching presentation indices550..1570: zero differing game counters.
This weakens a broad clock-rate explanation but does not identify the39tick
trigger discrepancy or rule out input phase, native state or random initialization.

Source audit: Capture DriveInput applies holds per delivered VSyncEvent; LibEtc
emits that event after Runtime.PresentFrame has sampled pads/delivered IRQ. Host
polling applies scripted controller overrides. No change to that order or to the
clock was made. NEXT: level-relative snapshots (current RamSnap accepts absolute
numbers only) or a native-update-entry trace to compare matching phases before
more route timing changes. Do not paper over divergence with actor/trigger writes.
Both runs terminal and cleaned. Zero images, no native rebuild/ZIP. P0/P1 remain
open; broader both-game/performance/visual/audio/hardware acceptance unchanged.

### Level-relative RAM checkpoints and matching native controls

Previous turn made progress by identifying the absolute-checkpoint alignment gap
and excluding a growing counter-rate difference in the sampled controls. Added
SM1 RamSnap archive+positive-offset requests, resolved once from Capture's first
archive-load hook. Runner --snapshot-offsets requires explicit SM1 --level,
rejects mixing absolute/geometry captures and keeps the eight-checkpoint limit.
No game RAM/input/clock changes. Unused normal-play path has no pending anchors.
Invalid game/zero-offset CLI checks reject before creating a run. Python compile,
SM1 Release publication and whitespace checks passed. Existing warnings remain.
Latest SM1 canonical SHA256:
660879860688A228E1B4C812526A6637035EB172EF6C0A3C045A57A3E7F3E61F
SM2 remains4F2671214A6F84F3F3FBDA40692507A1D6B80914FC2594C903D02488DCC1FAC8.

chase-relative-control-a/b:3250ticks,61.719s/61.812s,bothexit0. Sequential,
identical scripts, offsets590,890,1190,1490,1690,1890,1990,2090;no images.
Both anchors1088. Eight checkpoints resolve exactly in each run, and every
player XYZ, native button record, Venom state and game counter matches exactly.
Counters105,255,405,555,655,755,805,855. Full comparison retained as
chase-relative-control-comparison.json. At2090 both player(4038,1753,8449),
so use these measured states instead of borrowing absolute times from older runs.

This is repeatability evidence for this sampled control interval only. It does
not explain the earlier39tick divergence, prove every frame identical or validate
performance with snapshot overhead. Next extend an identical-offset control pair
through the disputed ledge before changing the jump. Both native runs terminal,
owned runtime/executable cleaned; no ZIP/images. P0/P1 and broader acceptance
remain open. Shared fixes still require both-game validation; this diagnostic
checkpoint feature is intentionally SM1-specific for the Chase investigation.

### Extended identical controls still diverge before the ledge

Previous turn made progress by implementing/validating archive-relative snapshots
and obtaining an exactly matching early control pair. This turn extends controls
through the disputed ledge: chase-ledge-control-a/b,3650ticks,66.000s/66.703s,
exit0,eight identical relative checkpoint offsets each,no images. Anchors1088/1089.
Sampled game counters755,855,925,970,995,1025,1045,1065 and native button records
match, but positions differ from first sample1890. A(5078,1203,5577) versus
B(5227,1220,5264). By2510 A remains(5179,578,10102),B falls(6050,2404,10680).
Full evidence:chase-ledge-control-comparison.json and both runs' route-state.json.

This contradicts any broad determinism inference from the prior matching pair.
No first divergence or cause is yet established. One-tick anchor parity and input
phase are candidates. NEXT: trace actual native pad processing (8006B208/8006B514)
and actual update entry at button edges, using raw RAM access so diagnostics do
not invoke the memory idle breaker. Comparing sparse held-button snapshots cannot
prove identical input history. Do not use TriggerPass as a frame hook: its stale
GameTrace comment was corrected to match prior evidence (level-load calls;
LogicFrames is a historical name). No executable behavior changed this turn.

Both games from these diagnostics are terminal,owned runtime/executable cleaned.
Zero images, no new build/ZIP. Clock remains unchanged, and P0/P1 remain open.
The failed ordinary traversal fixture is not evidence of a game clock defect;
retained long performance stalls and both-game acceptance still require work.

### Native pad post-hook and two traced controls

Previous turn was progress: extended controls disproved a broad determinism
claim. Added optional Recompiled.PadTrace.AfterRead post-hook to native8006B514
through the durable config; regenerated using build.recompile including costume
viewer and texture-registry transforms. Uses PSMemory raw span/BinaryPrimitives,
not ordinary reads, preserving registers/RAM/input. Existing background console
sink handles output. Cap4096 records with explicit limit message; disabled branch
only during normal play. Runner --pad-trace rejects SM2 (this is SM1 route tooling,
not a shared optimization). Build, Python compile and whitespace check passed.
Canonical SM1 SHA9F2758F65A0A6183817636685B631E0D5D02F74175DC14D40438C2DF0339E61D.
SM2 executable unchanged. No new ZIP.

chase-pad-trace-a/b run identical scripts serially without snapshots/images:
3250ticks,59.344s/59.406s,bothexit0,anchors1088/1091.2281/2280 records,each
contiguous from1; cap not reached. Native decoder is called multiple times per
update. Comparing keys(gameCounter,caller,occurrence) afterrelative400 produces
1760 common keys,zero differences in held/pressed/script/controller/XYZ/yaw.
A has two unmatched terminal records; they remain outside the comparison rather
than being treated as equal. Raw parsed pad-trace.json per run and aggregate
chase-pad-trace-comparison.json retained.

This traced pair does not reproduce the prior divergence. It does not identify
its cause, prove instrumentation-neutral timing or establish full traversal.
NEXT: carry this trace into the next controlled ledge comparison/recurrence;
use the first native input or position mismatch to localize cause. Do not return
to attributing outcomes to a changed jump without matched preceding state.
Both games from these diagnostics are terminal,owned runtime/executable cleaned.
Zero images. P0/P1 and remaining both-game/geometry/audio/performance/hardware
gates remain open; no claim of99percent confidence or resolved rare stalls.

### Native trace captures a consumed-input phase difference before route divergence

Previous turn made progress by installing bounded native pad tracing. This turn
runs traced-roof-baseline/jump serially,3800ticks,68.328s/67.907s,bothexit0,
no images/snapshots. Anchors1089/1088;2829/2831 contiguous records,cap not reached.
2310 matched(native gameCounter,caller,occurrence) keys afterrelative400;
two unmatched keys each.1440 matching keys differ. The intended input change
startsrelative2430, but first consumed-input difference is at counter98:
ScriptHeld16400 in both, player XYZ/yaw identical, baseline controller49135
and held/pressed1032 versus variant controller65535 and nativeheld/pressed0.
The first press reaches a different native update despite identical requested mask.

First pose difference occurs at counter458/caller80018040: XYZ still equal,
baseline ScriptHeld16,controller65519,held1024,yaw2261;variant ScriptHeld16,
controller65487,held1536,yaw2465. The variant still consumes Right when baseline
has released it. This establishes a concrete input-history difference before the
subsequent route difference; it is not proof of nondeterministic physics or a
production clock defect. Archive-relative console input phase is not sufficient
for controlled replay here. NEXT inspect/fix diagnostic delivery across ScriptHeld,
Controller.State, BIOS/libpad buffers and native decoding, or schedule consumption
against native updates; verify with these traces before more jump tuning.

Added read-only tools/chase_pad_compare.py, validated against earlier pair (same
1760 matching/zero differing keys), records missing/cap state and first pose change.
Reports/logs retained under chase-traced-roof-comparison.*. No runtime changes,
new builds or ZIP this turn. Both games terminal/cleaned,zero images. P0/P1 remain
open; shared timing/audio/rendering and cross-hardware acceptance are unchanged.

### Script input preparation fixed; moving Chase reproduces the P0 stall

Previous turn made progress by locating consumed-input phase divergence. Added
VSyncInputEvent before Runtime.PresentFrame; both capture harnesses prepare their
scripted input there instead of completed VSyncEvent. Exclusive mode immediately
applies controller overrides. Completed-tick captures/logs stay in VSyncEvent.
No extra IRQs, pad-buffer/actor writes, clock rate or production physical-input
change. Normal runs have no scripted-input preparation listener. TimingRegression
checks actual libpad press/release bytes for three requested edges plus ordering;
324 total assertions pass. Existing IRQ/pacing/XA tests remain passing.

Both canonical Release builds succeed. Latest SHA256:
SM1 75709218ACDD7636B5F94CD57084149C6C31FEE0C0C0763E9C38BBF7BF5B4713
SM2 A4B12AFDABB3B6E43D835C6F1EB37E7CF56E582EDFEAB0A2904E0B2F59A87605

chase-input-phase-a/b:3800ticks,68.828s/77.750s,exit0;anchors1087/1062,
2779/2791 contiguous records,cap not reached. Afterrelative400 stale controller
versus requested script masks are0/0 among2263/2276 records, versus50/10 in the
retained pre-fix traces. input-phase-latch-check.json retains this comparison.
The pair still diverges: atnative counter98 A already requests Up+Cross while B
requests0. Counter250 first yaw divergence. Thus consumer-latch ordering is fixed,
but archive/console offsets do not establish matching native-update requests.
Next controlled replay should use native-update scheduling; do not claim full
route determinism, completion or that production simulation was faulty.

IMPORTANT: phase-b also reproduces long stalls while moving early in Chase.
Atpresent893/counter172 (34.821s from first presentation),interval2069.1045ms;
previous present892 hasPresentCall2047.0981ms. Atpresent890/counter169,
interval1197.8707ms withoutside-present1197.4089ms. Later present906/926 have
PresentCall724.3804/1232.1302ms,seen in following743.1532/1252.4329ms intervals.
Raw reports, largest-intervals.json and stall-neighbor-records.json retained.
No detailed present phases/GC/host witnesses in these runs; cause is unassigned.
NEXT P0 run: same moving route with phases,GC-only trace and host/heartbeat
witnesses; preserve this failure. Do not explain it away as harness overhead or
claim the input ordering change caused/fixed historical stalls.

sm2-input-phase-integration:4300ticks,76.281s,exit0,e1m0 anchor1008,74 audio
observations,none nonplaying after5s. This proves script-driven transition/log
progress,not fresh visual/audible acceptance. All three games terminal and owned
runtime/executable cleaned. Zero images; no ZIP. Python/diff checks pass.
chase_pad_compare now orders first mismatches by first run's actual trace order;
rechecking older evidence still gives first input98/first pose458. P0/P1 open.


## Moving-route host witnesses and process attribution (September 18)

`chase-moving-stall-witness`: 4800 ticks,92.344s,exit0; trace/witnesses0,
186 GC/contention events,lost0. Same ordinary route, no follower/region writes,
no screenshots. A2600.7943ms early interval overlaps2595.262ms independent
heartbeat delay; host collection envelope reports99.885% CPU and queue21.
Game CPU bracket390.625ms over4083ms; conservative non-CPU lower estimate2178.919ms.
No managed suspension/contention in the covered interval. Other early pauses:
664.9043ms event pump,993.5941ms limiter,1638.783ms render dispatch with only
0.1862ms render callback. These support host scheduling pressure for these
specific pauses, not a universal OS attribution. The route-run32.108s stall
is232.1814ms (outside-present231.7279), with no GC/heartbeat delay; unresolved.
Another118.3002ms interval occurs69.534s. All-interval evidence is in
stalls-all.json; stalls.json selects after20s and MUST NOT hide startup failures.

`chase-moving-process-witness`:4800ticks,93.422s,exit0;244events,lost0.
Early2887.4268/1131.1223/1037.466ms pauses overlap heartbeat delays; 32.400s242.1806ms outside-present remains. Process names include Windows update,
builds and other apps, but coarse windows do not establish a culprit.
IMPORTANT: first psutil process collector cost38.406s CPU over93s and had very
wide collection windows. Preserve this run but do not treat it as low-overhead
acceptance or extrapolate process rates into exact stall intervals.

`sm2-process-witness`:4800ticks,86.235s,exit0;211events,lost0; all witnesses0.
Script requests movement/jump/attack buttons on e1m0, but no fresh visual output
was inspected, so successful traversal/combat is NOT established. Four intervals
above100ms:170.4751,377.7793,134.6143,247.0747. Last at38.586s is246.7796ms
outside-present, with no managed pause or heartbeat overlap. CPU/load data do
not establish a cause. Same old collector cost36.484s CPU; do not use as clean
performance acceptance. Audio logs retained, not audible acceptance.

New opt-in `--host-processes` (requires `--host-counters`) adds bounded per-process
CPU attribution and normal owned-helper cleanup. No process termination/priority/
power changes outside owned diagnostics; no command lines or executable paths
are logged. New collector uses Windows limited-query handles and GetProcessTimes,
matching PID+creation FILETIME; inaccessible/new/exited processes explicitly
limit coverage. Native standalone snapshot7.55ms versus earlier~0.8s collection;
native CPU burn yielded125ms/126ms wall. Three focused process tests plus11
stall-correlation tests pass. Broad unittest discovery also tried Blender's
package and failed for unavailable bpy; targeted suites pass. Correlation output
retains complete overlapping collection windows, never per-frame interpolation.


Phase clarification: the repeatable232.1814/242.1806/233.5405ms SM1 intervals
all have native GameCounter1 and zero uploaded bytes/batches, as does SM2's
247.0747ms interval. They occur at gameplay entry, before established moving
play. Do not describe them as the same mid-chase issue as retained
`chase-input-phase-b` counter172/2069ms. First-update warmup/loading remains
unattributed; preserve these intervals separately instead of dropping them.

`chase-fast-process-witness`:3800ticks,69.171s,exit0;158events,lost0;all helpers0.
Uses Windows limited-handle collector (this first v2 run preceded its explicit
collector-version metadata field).68process samples,0.578125s collector CPU,
max collection12.456ms, inaccessible processes explicit. Five>100ms intervals:
163.197,428.0411,151.0199,233.5405,114.7097; no heartbeat overlap in these.
No multi-second recurrence in this run does not erase failing evidence. No
rendering/completion claim; no images, no new ZIP, game binary unchanged.


Next bounded investigations (preserve these distinctions):
1. Attribute the reproducible first native-update hitch in both games: counter1,
   zero upload/batches,~230�250ms outside-present. Correlate level transition,
   native update and first-use runtime work before proposing another timing fix.
2. Keep the mid-Chase counter172/2069ms failure open; latest host-pressure
   evidence concerns different early intervals and cannot assign its cause.
   Use fast process witnesses for future reproductions; never stop unrelated
   workloads or change OS priority/power settings to manufacture a clean pass.
3. Native-update-relative process-local input scheduling remains necessary for
   controlled Chase replay. Fixed pad preparation alone did not make authored
   route input identical at native counters. Full ordinary completion is open.
User testing remains deferred while local resolution work is possible; package
only a stronger candidate. Existing AudioRecovery ZIP is older than current fixes.


Final paired fast-collector check: `sm2-fast-process-witness`,4300ticks,77.172s,
exit0,trace/helpers0,213events,lost0.76process samples,0.640625s collector CPU.
Intervals>100ms:176.5537,361.1194,127.3217,253.3470;last again GameCounter1,
253.0478ms outside-present. No fresh visual/combat/audio acceptance is implied.
Audio queue observations across the five runs respectively99/91/82/65/74;
nonplaying after5s counts19/8/0/0/0. Preserve the two SM1 failures and recovery
logs; later healthy queue observations do not establish audible correctness.
All five owned game processes/helpers terminal; their EXE copies and runtime
caches cleaned. No images generated, no ZIP rebuilt, no issue/goal closure.


## First-update JIT attribution (September 18)

Previous turn made progress: host-pressure evidence narrowed some early pauses,
but left the first-update hitch unassigned. This turn adds `--jit-trace` to
`--stall-trace` (provider0x4019,verbose; no CPU sampling). Exporter retains Method
and Loader events as well as GC/contention. `performance_jit.py` pairs JIT start
and method-load by methodID on the recorded main thread, clips to actual native
presentation gaps and unions overlapping spans. These are wall spans, not CPU
samples; trace loss/coverage and unmatched starts remain explicit.

`chase-first-update-jit`:2400ticks,46.031s,exit0;28476events,lost0.
Counter1 interval290.7448ms;262 paired main-thread compilations,239.6632ms union.
Largest Recompiled.SpiderMan.ptr_8003EC34=77.4341ms;func_8003C40C=12.2989ms.
`sm2-first-update-jit`:2800ticks,52.047s,exit0;23617events,lost0.
Counter1 interval269.5765ms;183 paired main-thread compilations,253.5613ms union.
Largest Recompiled.SpiderMan2.ptr_80045C3C=107.9682ms.
Both largest methods QuickJitted, not an optimized-background-code pause.
Raw runtime-events and jit-summary.json retained. This evidence supports moving
first-use compilation out of play, not reducing console speed or resolution.
No generated game function was executed for prewarming, and no gameplay state
was modified. Next experiment uses publishing with PublishReadyToRun=true in
separate sm1-r2r/sm2-r2r directories. Runner --executable accepts explicit
candidate without overwriting canonical EXE; hash/source recorded as before.


SM1 ReadyToRun experiment: publish succeeds with existing MonoMod single-file
warning. Candidate `sm1-r2r/SpiderMan.exe`,125127051bytes versus91978147bytes
canonical; SHA25666B100F2703F9CADEF74151DEE03734377DAEC09BAC33291D8214717AFF709EF.
`chase-first-update-r2r`:2400ticks,50.468s,exit0,trace/witnesses0;
17168runtime events,lost0. Counter1 occurs atpresent735 with33.3341ms interval,
versus290.7448ms in traced baseline. No first-update gap>100ms. Whole run max
190.6012ms still retained (startup/loading phases require separate accounting).
The formerly77ms method can still be tier-compiled on background thread27804,
not main13840; precompilation does not disable all runtime compilation.
This is targeted timing evidence only; no fresh rendered output was inspected.
SM2 candidate comparison remains required before adopting publish default.
Runner explicit-candidate CLI initially had an undefined Path alias; corrected
to pathlib.Path before any game launched, then --help and actual candidate run pass.


SM2 ReadyToRun result: `sm2-first-update-r2r`,2800ticks,56.656s,exit0,
trace/witnesses0,15465runtime events,lost0. Counter1 interval33.3346ms at
present1107 versus269.5765ms traced baseline. No first-update gap>100ms;
whole-run max136.9988ms remains retained, not a comprehensive performance pass.
Candidate100651618bytes,SHA256b10706e1044793272a54e97ac8dc1cf146e884dd130d06e7c3f7cfc4302c423f.
Native frame02607 inspected: Spider-Man and blue character on rooftop, textured
utility structure, ground/shadows, skyline and sky present. This is a scene
render check, not proof of successful combat, audio or full sequence completion.
Both csproj publish groups now set PublishReadyToRun=true. Ordinary development
build configuration remains distinct. Both MSBuild publishing property queries
returntrue. Existing MonoMod single-file warning remains; these native runs did
not exhibit a corresponding failure. Runtime timing and 30FPS settings unchanged.
Three process tests,11stall tests, Python compile and git diff checks pass.


Final SM1 native render check: `chase-r2r-render-check`,2400ticks,45.438s,
exit0. Frame01986 inspected: Spider-Man on rooftop beside antenna structure,
Venom ahead, textured skyline/buildings, Chase HUD/arrow and character textures
present. Route completion/collision correctness is not established by this
single scene.41audio queue observations,none nonplaying after5s; muted, not
audible acceptance. Exactly two generated images this turn, both inspected.

Canonical diagnostic builds now copied from tested R2R candidates, hash verified:
SM1 66B100F2703F9CADEF74151DEE03734377DAEC09BAC33291D8214717AFF709EF
SM2 B10706E1044793272A54E97AC8DC1CF146E884DD130D06E7C3F7CFC4302C423F
Old exact canonical binaries preserved as sm1-pre-r2r/SpiderMan.exe and
sm2-pre-r2r/SpiderMan2.exe. The publish property-only change is equivalent to
the tested CLI -p:PublishReadyToRun=true. All five native runs and builds
terminal; owned copies/caches cleaned. No new ZIP, no issue/goal closure.
Next: validate moving Chase and SM2 sustained first-use paths with these builds,
retain rare-stall failures; solve native-update input scheduling for full ordinary
Chase completion. First-update JIT repair is a local fix, not cross-hardware
acceptance or a diagnosis of every historical multi-second pause.


## Post-R2R moving Chase stall recurrence under compiler pressure

Previous turn was progress: both publish defaults changed after paired native
JIT evidence and inspected scenes. This turn retains a new failing moving run,
not just another clean startup comparison.
`chase-r2r-moving-validation`:4800ticks,96.657s,exit0;trace/helpers0,
20800runtime events,lost0. Same normal route, no follower/forced regions,
no images. JIT all-gap report now includes gamecounter/phase and explicit
trace coverage, and excludes optional GPU-batch records.

Counter410/present1144:1285.0581ms at40.034s,1284.1314ms outside-present.
Independent heartbeat delay1579.6433ms overlaps1260.053ms of the interval;
conservative non-CPU lower estimate941.308ms. Host CPU89.66�100%,queue45/38.
Counter412/present1146:1364.7042ms at41.444s;previous render dispatch1347.7351ms,
render callback0.2244ms. Non-CPU lower estimate849.079ms;heartbeat only short
75/73ms delays here, so do not claim an equal full heartbeat pause for this case.
Counter453/present1187:980.5189ms at45.260s,980.0982ms outside-present;
heartbeat966.2753ms overlaps almost all of it.
All three have full zero-loss runtime-trace coverage, no managed suspension,
no managed contention and zero paired main-thread JIT overlap.

Coarse process intervals capture several cl.exe compiler processes, each using
roughly1�3.7cores; four simultaneously report3.43/3.21/2.99/2.88cores in a
shared interval. Windows host CPU approaches100%,queued work up to45. This
supports host contention for this specific moving failure, including an
outside-present recurrence, rather than blaming JIT or low GPU specifications.
The 1.35s render-wrapper pause still lacks an exact wait/driver stack; evidence
is not sufficient to assert every old stall has this cause. No unrelated
processes were stopped; no priorities, affinity or power settings changed.
Fast process collector92samples,0.96875s CPU over96.657s;raw process windows
and inaccessible-process counts retained. Audio98observations,19nonplaying
after5s; preserve failures/recovery records. No audible acceptance.


Paired SM2 post-R2R run: `sm2-r2r-moving-validation`,4800ticks,89.375s,exit0,
trace/helpers0,17905events,lost0. Only>100ms interval is146.161ms atpresent2,
GameCounter0,with2.10ms paired main-thread JIT. No later>100ms interval recorded.
81audio observations,none nonplaying after5s. Requested process-local movement/
attack/jump script is identical to prior process-witness run; no new image or
behavior capture, so this does not prove successful combat/traversal or audio.
Both games terminal,owned EXE copies/caches cleaned. Zero images this turn.
No binary/ZIP changes; no issue/goal closure. Eleven correlation tests,Python
compile and diff checks pass. Updated JIT report records per-gap trace coverage;
missing/uncovered events must never be described as zero compilation.

Next route-control implementation can use the existing VSyncInputEvent to read
native gamecounter directly from raw PSMemory (SM1 0x800B4F38), choosing held
mask by native-update interval instead of decrementing hold each console tick.
Require explicit Chase-load arming/reset handling and preserve console-tick
scripts for menus. Keep controller-state input plus normal LibPad/BIOS refresh;
do not inject pad RAM, advance counters, force triggers or actor positions.
Compare actual PadTrace consumed masks and player state at matching native
counters before accepting a repeatable replay. This is a proposed next step,
not implemented or verified in the current worktree.


## Native-update Chase replay implementation

Previous turn produced progress by correlating a new moving failure with host
compiler pressure. Current work addresses the separate replay alignment gap.
New SM1 test-only NativeInputSchedule and SPIDEY_NATIVE_SCRIPT; runner accepts
--native-script only with sm1 --level l5a1. Inputs sampled from raw native
counter at existing pre-pad VSyncInputEvent; no game-memory mutation. Native
counter0 must be observed after archive arming, counter decrease terminates
schedule, and further archive loads cannot revive it. Overlapping step masks
combine; duration measured in gameplay updates, never decremented perconsole tick.
Eight new regression checks pass,total332; SM1 publish with R2R succeeds
(existing warnings),candidate sm1-native-input. No SM2 runtime change needed:
this fixture addresses SM1's Chase path. Both-game performance scope remains.
Two sequential3800tick native traces are pending comparison. Recipe derives
requested masks from retained phase-a native counters98 onward; do not assume
it reproduces that route until consumed masks/positions are compared.


Native replay pair result: a3800ticks74.234s, b3800ticks68.094s,bothexit0.
2780/2782contiguous native pad records,cap not reached. From nativecounter98,
2046matching(counter,caller,occurrence)keys,0unmatched,0differences in script,
controller,native held/pressed,playerptr,XYZ oryaw. This establishes this paired
fixture repeatability, not universal determinism or ordinary Chase completion.
Compared with old phase-a requested recipe, start98 is consumed at99; first
pose divergence250. The controller pipeline delay is measured, not repaired by
pad writes. `native-chase-aligned-script.txt` shifts every preparation start by-1
while preserving durations; it is prepared for next comparison, NOT yet run.
Keep menu archive script unchanged. See native-update-pair-comparison.json and
native-update-golden-comparison.json. No images;owned copies/caches cleaned.

Canonical SM1 now uses tested sm1-native-input candidate, SHA2568fa3009f444a6c95130d2cc8f53a41d2904d801c75f9140bf808ec46fffdfb1d.
Previous R2R binary remains in sm1-r2r; SM2 canonical unchanged. Production
behavior is unchanged unless SPIDEY_NATIVE_SCRIPT is explicitly supplied.
Next run aligned native recipe twice and compare consumed input/player states;
then isolate the next failed roof transition using native update offsets.
P0/P1 remain open; no package, commit or issue closure.


## Aligned native replay and controlled roof-jump progress

Previous turn was progress: native scheduling plus exact paired traces.
Aligned-a/b now match on2044native keys with0unmatched/0differences;aligned-a
also matches retainedphase-a on2044keys. This confirms the -1recipe shift.


## Controlled next-roof jump result

`chase-native-roof-jump`:4100ticks,73.438s,exit0. First input difference versus
aligned baseline is native1020 (R2release),first pose difference1031(afterCross
consumed1030). No earlier difference: this is a controlled route change.
At1040 player(5498.9,192.3,10277.3),1050(5695.2,254.7,10436.6),
1080(6804.3,263.7,11621.2),1110(8167.8,328.2,13071.2). Unlike baseline,
no FallFog fall immediately after1040. By1120 position(8246.9,289.1,13100.2),
yaw0,then roughlystationary XZ whileY decreases. Atcounter1134/frame3731,
script800E4CAC contains UTF16LE words decoding `You Lost Venom`;1135teleports
to(312,1097,-1793),a failure sequence,not success. No screenshot this run,
so obstruction type/next-roof landing itself is not visually verified.
Milestones saved in run/native-route-milestones.json;rawtrace/comparison retained.

NEXT: use the same native-chase-next-roof-jump-script.txt with one native capture
near native1100�1120 (VSync approximately3663�3703 for the observed run; prefer
archive-relative shot after checking actual anchor), plus bounded RAM checkpoints
if needed. Inspect obstruction and Venom destination before changing direction.
Keep earlier recipe fixed; choose the next local turn/jump from evidence.
Do not revert to console-tick control for the route or broaden the authored
building-cadence fix. Current progress is route traversal,not a new production
performance fix or fullChase acceptance. All three runs terminal,copies/caches
cleaned,zeroimages. P0/P1 open,canonical binaries and ZIP unchanged this turn.


## Next obstruction inspected; right-turn trial

chase-native-next-obstruction:3850ticks,69.032s,exit0. One native screenshot
frame03686 inspected: Spider-Man swinging beside a tall building corner, with
Chase HUD/arrow and surrounding textured buildings/sky visible. He has not
landed on an open roof at this point. Four snapshots at native1080/1100/1110/
1120 give Venom(12044,824,10937),(13951,1324,10758),(14266,1375,10758),
(14908,1403,10760),while player drifts toward(8247,289,13100). Thus route heads
too far+Z while Venom advances mostly+X; this motivates turning right before
the next swing instead of merely extending the existing heading.

Control comparison with prior jump run:2190matched native keys,0differences;
144unmatched keys only in the longer original run. Snapshot/capture overhead
has not changed the compared input/pose records. This is a scene/route check,
not timing acceptance. Exactly one image generated and inspected.

Prepared native-chase-next-roof-right-script.txt changes only
1019:up:10 ->1019:up+right:10 and
1029:up+cross:6 ->1029:up+right+cross:6.
Jump consumed1030 and R2resume1044 stay fixed. Trial chase-native-next-roof-right
uses4400ticks,PadTrace,twoRAMcheckpoints,zeroadditionalimages. Do not infer
success from a farther coordinate; inspect native failure/region scripts.


Right-turn trial: chase-native-next-roof-right,4400ticks,78.266s,exit0.
First input/pose differences native1020,exactly where Right was added.
Player1030(5264,578,9998),yaw3187;1040(5584,192,9951),yaw2847;
1070(7396,1006,10709),1071Y898.03,1072onwardY898.0.
By1110(8075,898,11037),Venom(14266,1375,10758). Player advances onlyabout
2X/3.5Z units perupdate late here,consistent with another collision; no image
of this later surface yet,so do not call obstacle geometry verified.
Script800E4CAC again triggers You Lost Venom atframe3762;failure sequence
teleports to(312,1097,-1793). This is an improved heading,not completion.

Next isolated trial native-chase-lower-roof-jump-script.txt keeps priorinputs
through1077: replace terminal1043:R2hold with
1043:up+r2:34;1077:up:6;1083:up+cross:6;1089:up:6;1095:up+r2:3000.
Normal Cross consumed1084 after measured landing1071; run
chase-native-lower-roof-jump,4400ticks,PadTrace/twoRAMsnapshots,noimages.
Do not combine this with an untested additional turn when assessing the result.


Lower-roof jump result: chase-native-lower-roof-jump,4400ticks,78.265s,exit0.
First input difference1078(R2release),first pose difference1085(afterCross1084).
No earlier difference from right-turn baseline.1100(8370,579,11120),
1120(8902,595,11280),1140(9320,898,11451),1160(9562,898,11532),
1180(9735,898,11612);XZstops there through1200. Script800E4CAC again says
You Lost Venom atframe3878;by1210teleportedfailure. So secondjump advances
past earlier obstruction but does not yet solve next collision or chase pacing.
All comparisons retain exact rawpad state;no position-following or forcedregions.

NEXT BASELINE: native-chase-lower-roof-jump-script.txt. Inspect next collision
around native1160�1190 with one native game capture and bounded RAM state.
Prior run counter1160 corresponds roughlyVSync3788, archiveoffset2700;
confirm anchor and nativecounter in actual capture. Venom at1110 remains
(14266,1375,10758)whileplayer(8736,595,11210);heading still drifts+Z. Use
observed next geometry to decide whether to turn more toward+X or jump.
Do not declare traversed roofs/rendering correct from coordinate distance alone.
All three runs terminal,ownedcopies/caches cleaned,oneimage generated/inspected
this turn. No binary or ZIP changes. P0/P1 and cross-hardware acceptance open.


## Third obstruction native inspection

chase-native-third-obstruction:4100ticks,73.422s,exit0. Native capture03786
atcounter1160 inspected: Spider-Man running on a roof between utility structures,
a thin antenna directly ahead,Chase HUD/directionarrow,skyline/textures present.
No confirmed corruption in this frame; do not remove authored antenna geometry.
Snapshots1140/1160/1180 retain player(9320,898,11451),(9562,898,11532),
(9735,898,11612),Venom(15567,1366,10761),(16377,1106,10178),
(16832,1316,9851). Heading continues to drift away+Z. Antenna-jump is a local
traversal trial,not evidence that heading/overallpacing is already correct.
Control against prior lower-roof run:2440matched native keys,0differences,
36unmatched only in longer priorrun. Single screenshot inspected.

Next trial native-chase-antenna-jump-script.txt retains earlier inputs,
replaces terminal1095R2hold with1095:up+r2:44;1139:up:10;
1149:up+cross:6;1155:up:8;1163:up+r2:3000.
Normal jump consumed1150 before the knownlatecollision; no extra turn added.
chase-native-antenna-jump runs4500ticks,PadTrace/twoRAMsnapshots,noimages.


Antenna-jump trial:chase-native-antenna-jump,4500ticks,79.906s,exit0.
Firstinputdifference1140(R2release),firstposedifference1151(afterCross1150).
No earlier route changes.1160(9682,512,11582),1180(10456,621,11909),
1200(11439,531,12326),1220(12332,584,12701),thenXZstuckaround
(12638,12913)whileYdecreases andyaw0,consistent withwallclimbing.
Venom1200(17096,1157,10035),1270(19209,1145,10077),so player is still
headingtoo far+Z. LostVenomscript800E4CAC atframe4032;failureteleport by1300.
This clears prior localized obstacle but does not establish correct fullroute.

Nexttrial native-chase-antenna-right-script.txt addsRight to1139Up10 and
1149Up+Cross6,keepingjump/swingtiming andall earlierinputs unchanged.
chase-native-antenna-right:4700ticks,PadTrace/twoRAMsnapshots,noimages.
This is grounded pre-jump steering based on measured Venom direction;
no actorwrites,forcedregions,or changes togame-clock/cadence.


Antenna-right result:chase-native-antenna-right,4700ticks,83.656s,exit0.
First input/pose difference1140,matchingaddedRight.1160(9782,512,11303),
yaw2999;1180(10475,683,11405),1200(10955,907,11485),
1220(11625,2377,11599),1240(12365,4955,11725),thenFallFogscript800E4DFC
atframe3948 andfailureteleport(27502,1981,-24056). This turn does not yield
a usable route: it avoids the old wall heading but falls in a gap earlier.
Do not promote this trial as improved,or confuse its FallFog failure withthe
prior LostVenom failure. Exactinputs/rawtrace/RAMstates/comparison retained.

BEST CURRENT BASELINE remains native-chase-antenna-jump-script.txt (NO added
Right at1139/1149),which passes the antenna and reaches(12638,*,12913)before
wallclimb/LostVenom. NEXT inspect that wall aroundnative1220�1260 using one
native capture and state: choose an earlier direction change or swingrelease/
reattach based on actualgeometry. The 10updategroundRight addition tested here
is rejected. No successful fullChase or authoredbuilding traversal established.
Allthree runs terminal,ownedcopies/cachescleaned,oneimage generated/inspected
thisturn. No productionbinary/ZIPchange;allP0/P1scope remains open.


## Late wall native inspection and wall-jump trial

chase-native-late-wall:4200ticks,74.953s,exit0. Capture03946 atnative1240
inspected: Spider-Man clinging to tallbuildingface,Chase HUD/arrow,window
textures and sky present. This confirms wallclinging rather than only inferring
it fromstaticXZ/yaw0. A thin intermittent horizontal line is visible across
thewallaroundimageY330; preserve this potential geometry/raster seam for
separate original-asset/render-stage investigation. Cause not established;
do not call allgeometry correct or remove authored scenery from this image.
Snapshot1220player(12332,584,12701),Venom(17801,1207,10526);
1240player(12639,511,12913),Venom(17959,1316,10636);
1260player(12638,277,12913),Venom(18699,1071,10305).
Control againstantenna-jump:2540matchingnativekeys,0differences;90extra keys
only in longerpriorrun. Screenshot/checkpoints didnot altercomparedroute.

Next isolated trial native-chase-wall-jump-script.txt retains earlierroute,
replaces terminal1163R2hold with1163:up+r2:66;1229:right:2;
1231:right+cross:6;1237:right:6;1243:up+r2:3000.
This attempts a sidewayswalljumpaftercontact followedbyreswing,notanearlier
turn into the gap that failed previously. Trialchase-native-wall-jump uses
4700ticks,PadTrace,twoRAMcheckpoints,noadditionalimages.


Wall-jump trial: chase-native-wall-jump,4700ticks,83.235s,exit0.
First input difference1230,first pose difference1232. Player1240
(12639,775,12913),yaw4021;1250(12571,1078,12795),yaw0;1270
(12571,973,12795),1280(12571,843,12795),thenLostVenom by1290.
It detaches/repositions slightly but catches the same wall; do not adopt it.
Earlier route remained unchanged. No additionalimages.

Next controlled trial native-chase-corner-release-script.txt retains the
antenna-jumpbaseline and replaces1163R2hold with1163:up+r2:36;
1199:up+right:12;1211:up+r2:3000. This releases the swing before contact,
steersright whileairborne,thenresumesR2. It does not change the earlierground
turn or successfulantenna jump. chase-native-corner-release,4700ticks,
PadTrace/twoRAMsnapshots,noimages. Result remains pending.


Corner-release result:chase-native-corner-release,4700ticks,82.906s,exit0.
First input difference1200 as intended; NO player-position/yaw difference in
any matched native record versus antenna-jump baseline. Direction/R2 masks
are demonstrably consumed, yet the same swing trajectory and wallclimb occur,
followed byLostVenom. This rules out this specific airborne release/Right
combination, rather than proving the harness failed to send input.
Captured late-wall player.script_active is0 at1220/1240/1260, so do not blame
the authored building sequence for locking these movements.

NEXT: inspect native swing/input-state handling before further timing variants.
Track pad decoding fromfunc8006B514 throughfunc80017FC8/playerinput translation
into the large player updateptr8003EC34; resolve held/pressed/state gating for
releasing swing or changing direction. Do not mistake globals at800B4E64 for
pad records at800A4E64: immediate offsets alone are insufficient (an initial
text search also hit unrelated8002BD5C globalreads). A Cross-assisted release
is only a hypothesis,not a confirmed control mapping or pending acceptedfix.
Best current fixture remainsnative-chase-antenna-jump-script.txt; neither
wall-jump norcorner-release is promoted. Allthree runs terminal,copies/caches
cleaned,one nativeimage generated/inspectedthisturn. Potentialwallseam remains
unattributed. No productionbinary/ZIPchange; P0/P1/fullhardwareacceptance open.


## Active movie ring/stream ownership repair (September 18)

CollectFrame writes outside the consumer lock. StSetRing/StClearRing/StSetStream
previously reset or replaced ring state while its worker retained an in-flight
read and old slot selection. Each now calls StopWorker outside _lock before
mutation, then restarts the producer. Existing collection cancellation prevents
retired reads writing/publishing partial frames. Normal active/read semantics
are preserved. Shared by both games; no game movement/timing changes.

MovieStreamingRegression holds the second read (collection after slot selection),
initiates each transition, verifies waiting/retirement, then checks resumed frame
ring addresses, header and payload. Before log fails at 'replace: transition must
retire the in-flight ring owner'. After:72 assertions, max consumer call0.568ms.
Files: movie-ring-transition-before.log, movie-ring-transition-after.log,
movie-ring-transition-timing.log under proof_render/performance-repairs.
This local bound is not a filesystem latency promise. Real movie/audio acceptance
and remaining stalls are separate. Earlier notes calling these races unverified
are superseded only for the reproduced/tested transitions.

Read-only airborne source audit and R1 trial in chase-normal-route.md:2630matched
keys,zero pose differences. Retain best antenna recipe; no movement patches.


### Candidate build and bounded native movie integration

Both ReadyToRun publishes succeeded (existing MonoMod single-file warning).
Candidates remain separate; canonical sm1/sm2 and Gateway ZIP NOT updated:
- sm1-ring-transition/SpiderMan.exe SHA256
  CEBEC9584C2A1EB3F71E1D1D5CDC3BCC8D4542266BC914386CE5013879385C3F
- sm2-ring-transition/SpiderMan2.exe SHA256
  C5F322B3C993BFFFE7B5C740EFA0076A0FF3951E3AF2F8F77683A74AB9504263

Each --keep-boot --frames4500 --shots1500 --audio-state --present-phases run
finished exit0 with owned EXE/runtime cache cleanup. Muted; two native images
only, both inspected: SM1 Neversoft animated logo image (partially visible
lettering in this single frame), SM2 Vicarious Visions logo. Actual images
establish those scenes, not all-frame visual acceptance or audible correctness.
Logs reach l5a3_t.trg and e1m0_t.trg respectively without logged timeout/exception.
No injected boot skip. TimingRegression332 and MovieStreamingRegression72 pass.

SM1 sm1-ring-transition-movies:84.562s,2250presentations,806movie records,
movie max38.2131ms. Gameplay intervals839.8235ms atpresent1728/counter287 and
467.0804ms at1795/counter354. Their previous RenderDispatch durations812.9845/
424.8958ms versus callback0.2187/3.2054ms localize most delay to render dispatch
outside the callback (not proof of GPU vs OS scheduling). Ten total>100ms.
Audio89samples,15nonplaying after5seconds; keep recovery failures visible.

SM2 sm2-ring-transition-movies:87.188s,2250presentations,1369movie records,
movie max893.813ms. Nine total>100ms. Large intervals:present150=872.4844ms
(game/menu),196=772.9226ms(movie),271=893.813ms(movie), mostly outside-present
871.8819/772.5679/893.4762ms. Audio93samples,15nonplaying after5seconds.
These runs lack GC/JIT/host witnesses; no causal attribution or regression claim
against the prior binary. Do not discard these failures or call movies/performance
fully fixed. Next compare/instrument the retained pauses on both games before
promoting candidates or refreshing the user package. All task-owned runs/builds
terminal. Goal remains active; ordinary Chase completion and hardware validation
remain open. No user test requested at this stage.


## Audio observation correction (September 18 follow-up)

The native audio trace's state/playing fields describe the observation BEFORE
recovery (see NativeAudioStateTrace and AudioBufferQueue), not its outcome.
In each ring-transition-movies run, all15 stopped observations after5seconds
have recoveryPerformed=true and all15 have a later Playing sample. Largest
sampled gaps to that later Playing observation:SM1 2.3381464s,SM2 1.7603628s.
Sampling is roughly1Hz when playing and event-driven when stopped; those gaps
are not measured audible outage/recovery durations. This supports resumption,
not uninterrupted playback. Earlier wording 'recovery failures' should be read
as underrun events, not demonstrated failed recovery. Keep all underruns and
performance stalls; they are not erased by later Playing samples.


## Witnessed candidate repeat: SM2 (September 18)

sm2-ring-movie-witness used the same candidate C5F322B3..., unskipped boot,
4500ticks, audio/phase/host CPU/top-process/heartbeat witnesses plus GC/contention/
JIT events starting2seconds for95seconds. Muted, no screenshots. Terminal exit0
in88seconds; trace and all3 external witnesses exit0. Export15337 events,lost0;
coverage QPC25393.105954 through25477.3333119. Native max75.0558ms;1439movie
records,max39.1344ms. No presentation gap>100ms, no stopped audio observations
after5seconds (79totalobservations). Host CPU samples17.84-62.53percent.
This repeat did NOT reproduce the preceding893.813ms movie pause, so trace
absence cannot attribute that older pause or prove resolution. Retain both runs.
Generated stall-correlation.json and jit-gaps.json cover all recorded gaps,
including startup (start0). No new user test/ZIP or canonical promotion.

Read-only installed Silk.NET2.23.0 API documentation confirms automatic buffer
swapping occurs at the end of Render and context control surrounds it. Existing
RenderDispatch minus RenderCallback therefore does not isolate CPU rendering;
next fine-grained evidence must distinguish window wrapper/context/swap and
host descheduling. Do not infer a GPU bottleneck from that residual alone.


## Witnessed candidate repeat: SM1 and callback-boundary probe

sm1-ring-movie-witness: same CEBEC958... candidate, unskipped boot4500ticks,
same phase/audio/host/process/heartbeat plus GC/contention/JIT settings as SM2.
Muted,no images. Exit0 in85.046s,trace/all3witnesses exit0. Export19557events,
lost0; QPC25524.2263651..25605.4341787 coverage. Native max56.1073ms;805movie
records,max37.8909ms. Audio77observations,zero stopped after5seconds. No gap
>100ms. This also fails to reproduce prior pauses; both earlier failing runs
remain authoritative. No causal attribution to the ring repair from a clean run.

Added opt-in phase timing around the existing OnRender callback entry/exit.
PresentationStages now includes RenderCallbacks, BeforeRenderCallbackMs and
AfterRenderCallbackMs; absent/incomplete callback boundaries are null. No change
to Silk rendering, automatic swapping, context control, FPS or window behavior.
Residual after callback may include automatic swap and host descheduling, not
pure GPU time. Before callback separates wrapper/context/pre-callback delay.
PerformanceRegression --profile passes, including synthetic distinct wrapper
intervals and missing-callback/reset cases; no GL window launched by that test.
Log render-callback-boundaries-test.log. Scoped git diff --check passes.

This new probe is SOURCE/TEST ONLY: ring-transition candidate EXEs and canonical
builds do not contain it yet. Next publish to fresh candidate folders, then use
--present-phases with witnesses on a meaningful workload. Avoid endless clean
startup repeats; use the retained ordinary moving Chase fixture for sustained
recurrence and preserve all gaps. No new screenshots this turn. All task-owned
native runs/helpers/builds are terminal; user testing is still deferred.


## Callback-boundary builds and moving validation (September 18)

Both ReadyToRun publishes passed; compilation finished before game execution.
Both games run sequentially,muted,no images. Canonical EXEs now copied/hash-checked
from sm1-callback-boundaries/sm2-callback-boundaries. Previous canonical copies
retained in sm1-pre-callback-boundaries/sm2-pre-callback-boundaries. Promotion
record callback-boundaries-promotion.json; no ZIP refresh/no issue closure.
SM1 SHA25683FAC5205A11143D9A6524ACA3B4F767992A414221D0768A174345CF5E9FA444
SM2 SHA25677439ADF72D06E9901DBC53AAD7C48912F9FF70020E4F7529660A6FE553874B7

chase-callback-boundary-witness: antenna-jump ordinary native button recipe,
l5a1,4700ticks,87.453s,exit0. Trace/helper exits0.19307events,lost0,coverage
QPC25935.9911113..26020.144695.2350presentations,all callback boundaries present;
max before0.3041ms/after12.4631ms. Positive-counter max34.1282ms,countermax1411.
Audio79observations,zero stopped after5seconds. The one>100ms interval is
startup present62,counter0,239.5835ms at2.302s,239.0336outside-present. Covered
trace:noGC/no managed contention;main-threadJIT0.3917ms;noheartbeatdelay;host
CPU32.74percent in enclosing1.012s window,queue0. Coarse CPU bracket cannot
exclude compute. This startup stall remains unattributed, not discarded.
No fresh pose/visual/completion proof; failed route fixture is not full acceptance.

sm2-callback-boundary-witness:4800ticks,88.281s,exit0;ordinary e1m0 button
requests Up180ticks,Square/Circle/Cross12each at retained offsets1500/1800/1860/
1920. Requests alone do not prove combat/visual correctness. Trace/helpers0;
15309events,lost0,coverageQPC26061.4694519..26146.0033935.2400presentations,
all callback boundaries present. Positive-counter max33.4208ms.81audio samples,
zero stopped after5seconds. Only>100ms interval:startup present2,counter0,
135.0975ms,130.1702outside-present;main-threadJIT1.6471ms,managed contention
0.351ms,noGC/noheartbeatdelay. Those small spans do not explain all startup cost.

All gaps preserved in each run's stall-correlation.json (start0) and jit-gaps.json
(all-gaps). No repeated1-2second gameplay recurrence in these runs; do not infer
its cause or resolution. New probe works natively in BOTH games. No FPS/render/
swap/cadence changes. All task-owned runs/builds/helpers terminal.

Next route action is now source-backed: saved counter1200 has globalB4F64=0,
allowing a fresh configurable Cross press in state400 to halve horizontal
velocity/enterstate4. See ordinary-route handoff; test a single ordinary-button
variant before further blind steering. Remaining wall seam/material behavior,
full Chase, movies/audio/costumes and hardware acceptance still open. The top
handoff was rewritten to remove contradictory obsolete current-status text;
chronological evidence remains below it.


## Refreshed full Gateway ZIP and ordinary Cross trial (September 18)

OpenSpidey-Gateway-Chase-Test-2026-09-18.zip is ready:1,090,930,371bytes,
2074files verified by SHA256 and length. Entire ZIP SHA256
1A5FB0523ED366227CD47833F23FC03F696538FD94F336956FFA4B8BD4269E8C.
Includes both current canonical self-contained EXEs, local original game data,
movies/audio and bundled assets. Three launchers:direct ordinary Chase,normal
SM2,normalSM1. No follower/forced region triggers. Build/result logs named
 gateway-bundle-2026-09-18-* under proof_render/performance-repairs.
No upload or distribution performed; this is the user's private local ZIP.

Packaging fix: generated CMD launchers now clear all inherited SPIDEY_/RECOMP_
variables before applying their own settings. Old fixed list missed native-script
and newer probes. Actual CMD environment tests with synthetic inherited
SPIDEY_NATIVE_SCRIPT, future probes and invalid allocation-probe DLL confirm
both chase/normal launchers clear them and retain unrelated environment. Test
substituted environment output for game launch, so no desktop input/control.
Audio queue state logging enabled; instructions preserve audio-state.jsonl.

Isolated verification used ZIP extraction, scratch muted settings, process-local
menu inputs, hidden native captures, then owned extraction/runtime-cache cleanup:
- First SM1 run2400ticks,47.359s,exit0,EXE hashmatches;frame1900 inspected but
  shows You Lost Venom after no gameplay input. Retained, NOT playable-flow proof.
- SM2 run5000ticks,87.734s,exit0,hashmatches;frame4500 inspected:Spider-Man,
  rooftop/city textures,help marker,health/ammo HUD and radar.
- Verifier SM1 checkpoint revised to archive-relative l5a1_t.trg+580 with1800tick
  bound. New isolated run42.437s,exit0,hashmatches;frame1646 inspected:Chase
  opening,Spider-Man,web,roof/city/sky and CHASE VENOM TO HIS HIDEOUT objective.
  This verifies direct launch's opening, not full completion or audible playback.
Three images total,all inspected. Each run has visual-review.json recording
exact scope; two isolated verification directories retained logs/images only.
Both EXE hashes match current83FAC520.../77439ADF... manifest entries.
No user testing requested now. No full-frame/movie/audio/performance acceptance
claim from these package checks. Remaining P0/P1/hardware work stays open.

Cross trial chase-native-corner-cross uses ordinary buttons only. Fresh Cross
atcounter1200 changes yaw;counter1209 state4 proves the predicted native action.
It hits a different wall and triggers LostVenom atframe4027. No production
movement changes; antenna-jump remains best. Exact comparisons/positions are in
chase-normal-route.md. All task-owned games/builders/verifiers terminal.


## Geometry-guided ordinary route progress (September 18)

Read-only mesh75 stock vertices match saved RAM; inferred wall normal agrees
with native surface normal and both earlier stops have~96unit wall clearance.
Render polygon tests explain why an attempted second jump was over empty space;
a known grounded point matches roof surface minus97units as positive control.
Reports:chase-late-wall-source.json,chase-near-corner-horizontal-surfaces.json,
chase-roofward-contact-faces.json. Render-surface subsets are not exhaustive
collision/material/camera verification; late horizontal seam remains open.

Three bounded muted input trials,zeroimages,allterminal:
near-corner failed;roofward safely landed but hit a short rooftop face;roofward-
jump cleared it and reached later Venom script35 wait. New best route candidate
native-chase-roofward-jump-script.txt still falls into fog atframe5136. Latest
snapshots player script0:actual authored through-building script not verified.
PadTrace caps4096records atcounter1799; preserve that limitation. Detailed route
checkpoints/comparisons are in chase-normal-route.md. No shipping code changed
this turn; current verified full ZIP remains OpenSpidey-Gateway-Chase-Test-2026-09-18.zip.


## Narrow roof steering and wait-point approach (September 18)

Saved native contact1331 maps exactly to original mesh98face62,flatY354;
playerY257/BC4=0 confirms grounded narrow roof,not wall attachment. Full face
coordinates and later side face in chase-next-roof-contact-faces.json. First
turn/jump trial reached another tall wall and LostVenom. HoldingRight for the
remaining8ticks before swinging redirects toward the wait point instead.
Latest best native-chase-narrow-roof-turn-script.txt:5600ticks,98.047s,exit0,
zeroimages. Player reachesX18306/Z9969 near Venom19592/Z9906,then remains
wall attached. No loss/fall marker within bound, but Venom stillscript35/36,
player script0:full completion/building sequence NOT verified. Pad traces capped;
actual saved states and comparison details in chase-normal-route.md. Both trials
terminal. No production/ZIP changes and no user testing request this turn.


### Chase entry trigger source audit and swing release (September 18)

Read-only saved RAM/original asset audit: `proof_render/performance-repairs/chase-entry-trigger-source.json`. Correct native GP is 0x800B47F4 (generated startup); trigger table 0x800E1C18 has 344 records. Trigger 34 at 0x800E2670 is type 6, registered hash 0x6C28CB52, NOT an arbitrary coordinate trigger. Native hash table 0x8011DF74 maps mesh 0 to this hash. `func_800806AC` writes object+0x16 mesh index to 0x800B58AC = GP+0x10B8; `func_800733D0` uses it to look up the hash and call `func_8005AAB0`. Original mesh 0 has four vertical trigger quads at Y=489..2185, rotated XZ footprint (19679,9487), (19043,9855), (19374,10429), (20011,10062). Object rotation is zero; translation and vertex bytes match saved RAM. This identifies the authored volume; it does not prove ordinary activation. No forced triggers, RAM writes, or production geometry changes.

`chase-native-entry-drop` (5600 ticks, 101.781s, exit 0, muted, no images) changes only the prior narrow-roof-turn tail: 1351 up+r2 for 48; 1399 up+cross for 6; 1405 up for 54; 1459 up+r2. Compared with narrow-roof-turn, 3403 native keys match, 799 differ, first pose difference at counter 1401. It clears the blocking wall: counter 1419 (17958,257,9840), 1439 (18298,257,9906), 1499 (18955,257,10091), 1599 (21904,443,10182). Player script remains 0 and Venom waits at script35/36: player crosses ABOVE the entry volume. Keep this as a useful branch, not completed Chase acceptance. Native pad logs cap at 4096; later snapshots are separate read-only evidence. Run was initially produced under a duplicated relative output prefix and moved, after exit, into the normal evidence directory; launch metadata may retain that original path.


### Side-entry trial result (September 18)

`chase-native-entry-side` completed its 5400-tick bound in 94.313s, exit0, muted, no images; all owned game processes exited. Relative to entry-drop, only tail after1405 changes to up+left20, up20, up+right20, then up+r2 at1465. 3403 comparable keys,787 differences, first pose difference counter1406. At1499 player(19127.46,257,10039.72),1549(20124.73,257,10111.60),1599(22982.32,594.81,9714.30): again above volume while crossing it, then below its top only after leaving its footprint. Player script0; Venom remains35/36. This trial did NOT solve entry; do not replace the entry-drop comparison branch with it or count exit0/no failure as completion.

Next investigation: map original supporting roof edges and the side/window access around mesh0. Both release and modest left/right steering stay on the continuous Y354 roof (player centerY257). Determine an actual route off that roof and into the Y489..2185 trigger sides before another input trial; do not keep making small heading changes above the volume. The trigger dimensions/hash source are now established, while the playable entrance path and full scripted traversal remain open. P0 long stalls/audio interruptions and both-game acceptance are unchanged by these route-only diagnostics.


### Fresh retained P0 failure and lower Chase approach (September 18)

`chase-native-interior-floor` demonstrates the ordinary lower entrance floor (Y1332 actor center over original Y1429 floor), but does not complete the route. Detailed inputs/checkpoints/failures are in chase-normal-route.md. It also records21 positive-counter intervals >100ms. Largest: present1804/counter1070 interval2507.5216ms, following present1803/counter1069 RenderDispatch/PresentCall2489.1137ms. Current outside-present18.4055ms cannot explain that interval alone; use preceding dispatch. The preceding interval itself was404.8473ms (outside404.3863). Counter1120 has622.0056ms with current PresentWait595.6765ms. Retain `long-gap-review.json`; the run lacks phase/host/GC/JIT witnesses and has pad/RAM probes, so source of the stalls is unassigned. No clean-repeat dismissal. The next bounded run, chase-interior-turn-witness, collects full existing witnesses through this same prefix.

The fresh floor-run stall correlator additionally bounds the2507.5ms gap with a3746.028ms whole-process CPU collection bracket containing328.125ms CPU (+31.25ms quantization allowance): conservative non-CPU wall lower estimate2148.147ms. The background monitor itself spent2159.849ms inside one collection call overlapping2153.795ms of the stall. This argues against sustained game computation for the entire pause; it does not distinguish OS scheduling, runtime suspension or driver/system waiting. No host/GC/JIT trace coverage exists for this failure. See stall-correlation.json, present1804.


### Full witnessed repeat of the fresh hitch prefix (September 18)

`chase-interior-turn-witness` completed5700ticks in106.219s,gameexit0,traceexit0,all3witness exits0. CanonicalSM1 SHA83FAC5205A11143D9A6524ACA3B4F767992A414221D0768A174345CF5E9FA444.2850presentations,complete callback boundaries,positive-counter interval max33.3662ms.96audio observations,0stopped after5s. Runtime export19982events,lost0,QPC29265.0974688..29367.9477003. Only >100ms interval is startup present63/counter0 at162.4003ms,161.2197outside. Covered by trace, no GC/contention/heartbeat delay; no inference that all remaining startup time is JIT. Local CPU sampling was not enabled.

The ordinary route fails atframe4824; this is not complete Chase visual/functional acceptance. It does repeat the native-input prefix through the earlier2.5s hitch location without reproducing that hitch. The earlier failure remains open and retained. No production code or ZIP changed in these route diagnostics; no new images; all task-owned games/helpers exited. Future native diagnostic runs should keep existing phase/audio/host/runtime witnesses enabled when practical so another recurrence can be attributed; do not initiate another unwitnessed performance soak. Both-game and external hardware scope remains unchanged.


### Callback-boundary P0 recurrence captured with all witnesses (September 18)

`chase-camera-aligned-witness`:6200ticks,122.5s,exit0,trace0,all3helpers0. Runtime export20497events,lost0. Exact coverage in runtime-events/stall-correlation.json. Five positive-counter >100ms intervals:931=1713.1212ms,932=2617.7935ms,946=313.631ms,948=906.4519ms,978=1975.7748ms. The932 interval follows dispatch2514.4751ms with callback0.2224ms,beforecallback0.002ms,aftercallback2514.2506ms.978 follows aftercallback1953.4192ms with callback0.2184ms. The boundary includes automatic swap/context work and OS descheduling, not pure GPU execution.

All five have trace coverage, no GC suspension, zero main-thread JIT. Only tiny other-thread contention overlaps(0.059ms/0.117ms) in first two. Conservative non-CPU wall lower bounds1385/2055/48/516/1788ms respectively. HostCPU92-100%, runqueues14-71 (one laterwindowqueue0); multiplecl.exe process windows show~2.6-3.7cores each. Independent heartbeat fully overlaps the1713 and1976ms intervals and substantial parts of the2618/906ms intervals. The background monitor also stalls. Strong evidence of shared-host scheduling pressure for these captured recurrences, rather than a multi-second game computation or covered managed suspension. Do not claim this reconstructs missing witnesses for every historical failure. No unrelated process was stopped or reprioritized.

112audio observations,7stopped after5s,all recoveryPerformed=true and later sampledPlaying. Timing/recovery samples do not measure audible outage duration. No native images this run. Route input naturally activates trigger34 atframe4773/nativecounter1653; Venom advances35/36 to37/38, then replay overshoots and FallFog at4963. Full Chase still incomplete. Canonical executables/ZIP unchanged.


### Return-corridor witnessed run (September 18)

chase-return-corridor-witness:6100ticks,111.375s,gameexit0,traceexit0,witness exits[0,0,0];19721events,lost0. Positive-counter max43.7914ms,103audio observations,0stoppedafter5s. Only >100ms intervalstartup present63=248.565ms. This repeat does not remove or refute the fully witnessed scheduling-pressure failures in chase-camera-aligned-witness. Route now activates34 and39 normally but fails the next turn; no complete ordinary Chase proof. Canonical builds/fullZIP unchanged; all owned games/helpers exited, no images generated.


### Ordinary authored-script timing and long-run limits (September 18)

chase-north-wall-end-witness naturally enters exact guarded script800E2B3E after trigger44. Before script:counter1800..1990 average33.3362ms/update; during:2220..2390 average50.0028ms/update with255presentations over170updates, preserving30FPS presentation/60Hz console. Max gameplayframe45.0093ms,114audio observations0underruns after5s. No new shipping change needed for that entry.

chase-authored-sequence-long extends same input10000ticks181.344s,all owned exits0,20587runtime events lost0. Script cursor advances and script_active returns0 bycounter2909; cadence transition inferred near2778. Replay keepsUp and losesVenom7996. Fullchase notcomplete; source/visual limits in route doc. Gameplay positive-counter max60.7531ms.179audio observations14stoppedafter5s. Retain all late game-over phase stalls:2484.4082ms atpresent4637 (hostCPU91%,queue58,independentheartbeat overlap2483.793ms) and1600.3539ms at4695 (CPU98%,queue24,heartbeat1599.172ms), both tracecovered/noGC. These pauses are outside the preceding short callback tail, reinforcing that scheduler-pressure pauses are not restricted to GPU swap. Full lists/correlation preserved; not excluded because gameplay has ended. Canonical binaries/ZIP unchanged, all owned processes exited.


### Normal-control handoff diagnostics (September 18)

chase-control-handoff-witness(8400ticks155.766s,20295events lost0) confirms script0 by2780 while wallattached; neutral stays put. Native report now includes read-only camera, wall/contact and script cursor fields, checked against known RAM values. Gameplay max43.8143ms,156audio samples17underruns; retained early menu >1s stalls have covered trace,noGC,host saturation/highqueues and independentheartbeat delays. chase-post-building-swing-witness(9500ticks167.5s,20266events lost0) showsUp+R2 climbs and walks without jumping; losesVenom7998. New inputs needed, not a clock patch. All games/helpers exited; no new images or shipping binary/ZIP changes. See per-run review-summary and route-state files. Both-game scope, wall seam reproduction and independent hardware acceptance remain open.


### Current SM2 boot movies through rooftop with callback boundaries (September 18)

`sm2-current-movie-boundaries`:5600console ticks103.5s,gameexit0,traceexit0,all3helpers0. SHA77439ADF72D06E9901DBC53AAD7C48912F9FF70020E4F7529660A6FE553874B7. Keep-boot enabled; levele1m0.2800presentations including1441movie(max39.3159ms),613positive-countergameplay(max77.7304ms). No>100ms intervals. Fullcallback boundaries present.17891runtime events,lost0,QPC31227.0163946..31326.9856451.97audio observations,0stoppedafter5s. This does NOT erase the earlier894ms movie failure or shared-host scheduling recurrences.

Archivee1m0 loadedframe3170; requestedUp4670for180ticks,Square4970,Circle5030,Cross5090. One native captureframe5000 inspected: Spider-Man on textured rooftop, surroundingcity,bluehelpmarker,shadows,health/ammoHUD/radar. It does not establish successful combat hits or every movieframe. Speaker muted; no audible acceptance. review-summary.json/visual-review.json retain exact limits. Approximately20spositive-counter gameplay is not the ten-minute sustained hardware acceptance. All owned processes exited,temporary executable/cache cleaned. No shipping code/ZIP change. Validation instructions now distinguish native-update vs archive-tick input and recommend full rare-stall witnesses.


### Post-script jump verified; later route still open (September 18)

chase-post-building-jump-witness confirms ordinary jump2815 andR2 swing2821 produce airborne state400 at2829. It lands by2869; the followingUp tail misses Venom's westward route and fails7990. No new shipping fix needed; next input fixture must relaunch/turn at that landing.9500ticks173.266s,allownedexits0,19983runtimeevents lost0,positive-counter max45.8162ms.167audio samples9stoppedafter5s; retain all late menu stalls/correlation rather than claiming an entirely stall-free run. No new images,canonical builds/ZIP unchanged.
