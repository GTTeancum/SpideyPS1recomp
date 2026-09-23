# Local performance validation


## Diagnostic priority caveat - September 18, 2026

The hidden `chase-west-relaunch-witness` game, its runner/ancestors and diagnostic
collectors were observed at Below Normal Windows priority while MSBuild workers
were Normal. Independent heartbeat delays under load therefore support shared
diagnostic scheduling pressure, not proof that normally launched games or all
host processes stalled. Older runs lack priority measurements; retain failures
without retroactive attribution. Native runner metadata now writes
`process-priorities.json` and the host process collector samples priority with CPU
usage. Both are read-only. No priority/affinity/power policy is changed.
Both newly verified game builds also record `priorityClass` in
`performance-system.json` and each `performance-process-*.jsonl` sample.
Unavailable or unsupported queries yield null. This metadata distinguishes
launch conditions; it does not prove a particular thread was descheduled.


For a diagnostic comparison, the native runner accepts `--game-priority normal`.
It sets NORMAL_PRIORITY_CLASS only when creating that one test game. The default
remains `inherit`; parent processes, collectors, production launchers and host
power/affinity policy are unchanged. The requested creation mode is recorded in
environment.json and actual class is checked in process-priorities.json and the
production logs. Sequential tests with different host load are not a controlled
priority A/B; a clean Normal run does not assign every earlier failure to priority.

Both games automatically write bounded performance logs beside the executable.
Keep performance-system.json, performance-frames-*.jsonl and
performance-process-*.jsonl together when reporting a problem. They contain
hardware/settings, build hash, frame/update timing, CPU/memory and power status.
They contain no screenshots or audio and are not uploaded automatically.
RECOMP_PERF_LOG=0 disables this logging. SPIDEY_LOG_DIR selects another folder.

On Intel and AMD systems, test integrated and discrete graphics where available,
including powerful machines. Start on AC power with normal settings; then compare
battery/power-saving behavior if that is where the problem occurs. Do not change
several settings at once. Test both games and note the approximate time/level of
each visible or audible problem.

For each game, include a cold launch and movie, skip/replay behavior, at least ten
minutes of movement/combat, a level transition, and stock/custom costumes when
used. In SM1, include ordinary traversal of Chase Venom and its building sequence.
Check scene content, animation/game speed, HUD/webbing, dialogue and music
continuity, not just a displayed FPS counter. A stationary scripted soak alone
does not cover those behaviors.

A measured presentation interval near 33.33 ms corresponds to the 30 FPS cap.
The native update counter is independent evidence of game progress; menus/loading
and the authored slower Chase sequence must be assessed separately. Outside-present
wall time includes native work and other delays; it is not CPU utilization.
Optional RECOMP_PERF_GPU=1 captures available GPU batch durations, with measurement
overhead and skipped/pending-query limits. Leave it off for baseline runs.

Summarize logs with:

    python tools/performance_report.py PATH_TO_LOG_FOLDER --start 60 --end 90

The selection is relative to the first retained presentation. Rotated logs retain
a bounded recent window. Save a run's logs before starting another run in the same
folder, which replaces them.

For development only, tools/performance_native_run.py launches hidden native
processes with input entirely inside the game. It does not send desktop input.
It requires psutil and canonical published executables under
proof_render/performance-repairs/sm1 and sm2. Use a fresh run name. Examples:

    python tools/performance_native_run.py sm1 sm1-stock --backend gl33 --frames 6000
    python tools/performance_native_run.py sm2 sm2-custom --suit --frames 6000

No captures are taken unless --shots is supplied. Temporary executable copies and
extraction caches are removed on exit. The runner refuses to launch with less than
3 GiB free and stops if free space falls below 2 GiB. Its --chase follower changes
player positions and invokes region triggers: it diagnoses the authored sequence
but MUST NOT be counted as ordinary-player completion.

For ordinary Chase route diagnosis, use `--level l5a1` with `--native-script`
and the current fixture in [the route handoff](chase-normal-route.md). Native
steps use `update:buttons:duration`, aligned before the game's pad sampling.
`--extra-script` uses archive/console ticks and is a different time base; do not
substitute those numbers for native-update steps. Never use the follower for
ordinary-route acceptance.
`--snapshots 1900,2100,2300` collects at most eight native RAM snapshots within
the requested run duration. It is mutually exclusive with `--geometry`.
Decode retained SM1 snapshots with `python tools/chase_route_state.py RUN_FOLDER`;
the report includes positions, player-script ownership/cursor, camera input angle,
player state, wall attachment, last contact and native button state. Last contact
can be stale; join it to nearby source geometry before calling it a blocker.
Snapshots write to disk on the game thread, so these runs are route diagnostics,
not performance acceptance. A jump during the introduction or before landing
does not establish a broken jump. Check actual state and the failure scripts.

Diagnostic runs set the existing `Muted` setting by default. This sets OpenAL's
source gain to zero, keeping the SPU mixer, real device queue and buffer processing
active. Use `--audible` only for a needed listening check. `--audio-state` records
bounded device-queue/SPU observations in `audio-state.jsonl`; a playing queue does
not prove correct music, dialogue or audible output. The personal Gateway ZIP's
normal launchers remain audible; this default applies only to the diagnostic runner.

Current audio-state tracing stops180seconds after its first observation. This
limits every cited run and the existing laptop ZIP; do not count missing later
observations as zero underruns. Frame/process telemetry continues independently.
The time cutoff should be replaced with bounded rotating audio evidence before
sustained audio-log acceptance. The unfinished rotation experiment was removed.

The shared queue's deterministic recovery check is
`dotnet run --project tools/RecompOne/tests/AudioStreamingRegression -c Release`.
It opens only an OpenAL loopback device and checks actual PCM for normal streaming
and underruns at entry/during refill. Recovery rebuilds a stopped queue to prevent
replaying consumed buffers; a mid-refill recovery may discard at most 46.44 ms of
ambiguously played fresh samples. Audio-state `recoveryPerformed` marks that event;
`state`/`playing` describe the pre-recovery observation, not continuous playback.

For long-stall diagnosis, collect runtime GC/contention events without the CPU
sampling profiler (which itself introduces runtime suspension events):

    python tools/performance_native_run.py sm2 UNIQUE_NAME --backend gl33 --frames 18000 --stall-trace --trace-start 15 --trace-duration 290
    dotnet run --project tools/PerformanceTrace -c Release -- proof_render/performance-repairs/UNIQUE_NAME/cpu.nettrace proof_render/performance-repairs/UNIQUE_NAME/runtime-events.jsonl
    python tools/performance_stalls.py proof_render/performance-repairs/UNIQUE_NAME

For rare-stall attribution, also enable `--present-phases --audio-state
--host-counters --host-processes --jit-trace` and cover the relevant run with
`--trace-start`/`--trace-duration`. Before/after render-callback boundaries separate
game rendering from the surrounding dispatch. The latter includes context/swap
work and OS descheduling; it is not pure GPU time. Independent heartbeat and host
CPU/queue witnesses are essential when the shared machine is busy. Fully witnessed
recurrences have shown scheduling pressure affecting the game and independent
processes together; this does not reconstruct missing evidence for older runs.

Run the exporter only after the game and trace collector exit. The runner records
Windows QPC timestamps around its independent process sampling. The report uses
the same clock as the game to correlate frame stalls, managed suspensions, and
monitor gaps, and reports trace coverage/lost events. Monitor gaps include its
intended 500 ms sleep; an overlapping gap does not establish the stall's cause.
Managed contention can belong to another thread. Keep diagnostic runs distinct
from baseline performance runs, and retain all outliers.

The stall report also brackets each interval with independent whole-process CPU
reads collected entirely before and after it. `cpu_bracket` reports the wider
collection span, its accumulated CPU time, and a conservative estimate of wall
time not explained by computation (allowing 31.25 ms for CPU-time quantization).
This includes other threads and surrounding work; no interpolation assigns CPU
to the stalled frame. A positive estimate narrows the search to waiting or lack
of scheduling, but cannot distinguish a driver wait, lock, preemption or process
suspension. Missing coverage is null, not zero. Tests:
`python tools/test_performance_stalls.py`.

For a deliberately instrumented stack run, `--trace` adds the .NET sampling
profiler and a Speedscope conversion. This itself introduces `SuspendOther`
pauses; never treat its full suspension list as GC-only evidence. After the run,
export raw sample times for the confirmed game thread (native thread ID in newer
hardware logs, or the profile containing Program.Main):

    dotnet run --project tools/PerformanceTrace -c Release -- RUN/cpu.nettrace RUN/main-thread-samples.jsonl --sample-thread THREAD_ID

Then rerun `performance_stalls.py`. Its optional sample coverage shows absent
samples and gaps. Converted flame charts can stretch an old stack across such a
gap, even labelling it CPU_TIME while independent counters and phase timings show
little CPU use and a long wait. Those spans do not identify a CPU hotspot.

An optional independent scheduling witness can observe the PID printed by a
currently running native harness:

    python tools/performance_heartbeat.py GAME_PID proof_render/performance-repairs/UNIQUE_NAME/heartbeat.jsonl --seconds 360

It wakes every 20 ms, records gaps over 50 ms, stops when that target exits or
the duration expires, and retains at most 5,000 delay records. The correlation
report includes overlaps and exact witness coverage bounds. It never changes
process priority or timer policy. Its CPU time is reported so measurement cost
is visible. A coincident delay is evidence to investigate, not a diagnosis.

Add `--present-phases` to record event pumping, input/controller polling, render
dispatch/callback, title updates and audio attachment. These are nested wall-time
measurements; do not sum parent and child timings. `PresentationStages` describes
the current presentation call, so the stall correlator uses the previous row's
stages for the interval ending at the current row. `BetweenPresentationEvents`
instead belongs to the current interval: it preserves event pumping/input from
non-presenting vblanks and service-only passes. Its controller timing is a subset
of input timing. A zero field does not measure or exclude OS scheduling stalls.


### General log backpressure regression

Run `dotnet run --project tools/RecompOne/tests/DiagnosticLogRegression -c Release`.
This exercises the shared background logger with blocked disk/console sinks,
queue saturation, a reserved fatal record, bounded shutdown and sink failures.
It does not simulate or explain every recorded performance stall. Diag prefixes
represent producer time/frame; output is asynchronous. Shutdown can lose queued
records after its2s deadline. Check any `[diag-log]` dropped/truncated/error report
before treating logs as complete; abrupt process termination cannot guarantee
that final summary is present.

### Unskipped boot movies and CD shutdown

Add `--keep-boot` to `tools/performance_native_run.py` to remove automatic boot
Start pulses. Title-anchored inputs still wait for the title archive to load.
Inspect actual native movie output and transition logs; a movie-phase counter
alone does not prove decoded video, audible output, or complete playback.
SM1's movie shutdown requires address80086F18 named `CdControlB` in the main
map and manual seed, so regeneration selects the SDK wrapper. Its former
unnamed retail implementation produced CD_sync/CdlPause timeouts against stale
CD IRQ state. SM2's80093878 already has the correct binding. TimingRegression
covers blocking Pause completion, status bytes and subsequent synchronization;
the native before/after evidence is in the performance repair handoff.


### Independent Windows host counters

For a live native diagnostic PID, run
`python tools/performance_host_counters.py PID RUN/host-counters.jsonl --seconds 360`
alongside the existing heartbeat. It reads CPU busy/performance/frequency reports,
DPC/interrupt time, scheduler queue/context switches, available memory/page reads
and disk queue through PDH. No elevation, process-name collection, host input,
priority changes or power changes. It stops when the exact target process exits
or after the bounded duration (maximum3600s), and refuses to overwrite output.
The first rate sample can be unavailable; API/counter errors are explicit.

The host collector uses language-neutral PdhAddEnglishCounterW and formatted
doubles with PDH_FMT_NOCAP100. See Microsoft's API documentation:
https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhaddenglishcounterw
and https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhgetformattedcountervalue.
Frequency/performance counters are provider reports, not measured per-core GHz.
Do not substitute nominal psutil.cpu_freq values for dynamic clock evidence.
The stall report retains coarse two-collection envelopes, including collection
costs/errors. Queue/memory gauges are endpoint observations; totals can hide an
individual busy core. Correlation cannot identify a driver or establish causation.
Missing coverage is unavailable, not zero. Eleven correlation tests pass via
`python tools/test_performance_stalls.py`, including boundary/non-extrapolation,
uncapped performance values, unprimed rates and unavailable counters.


### Level-relative route checkpoints

SM1 `--pad-trace` adds a bounded4096-record read-only post-hook on native
pad decoding at8006B514. It logs console tick, native game/IRQ/pad counters,
caller, scripted/controller masks, native held/pressed masks and raw fixed-point
player XYZ/yaw through the existing background console sink. Normal play leaves
it disabled. Raw RAM reads avoid diagnostic-triggered idle pumping. Check record
continuity and the cap; align by native counter, caller and occurrence, not row
index (there are multiple pad calls per update). Traced runs incur overhead and
do not establish uncontended performance. Both-game performance scope is unchanged.
Compare retained traces with `python tools/chase_pad_compare.py FIRST SECOND
--output REPORT.json`. The report retains anchor differences, record continuity,
cap status, unmatched keys and first input/position differences. Matching sparse
button snapshots is insufficient: a consumed press/release can differ by one
native update even while the currently requested ScriptHeld masks match.
The capture harnesses now prepare scripted input through VSyncInputEvent before
the upcoming tick's pad sampling; VSyncEvent remains the completed-tick event for
captures/logging. Exclusive scripted mode also refreshes Controller overrides at
preparation. No direct game pad-buffer/actor writes are added. TimingRegression
checks actual libpad press/release bytes on their requested tick (324 total checks).
Native repeatability still requires trace comparison; correct event ordering alone
does not prove a whole scripted route repeatable or complete.

For ordinary SM1 route diagnostics, `--level l5a1 --snapshot-offsets
590,890,1190,1490,1690,1890,1990,2090` schedules up to eight RAM checkpoints
relative to the first `l5a1_t.trg` load. It is mutually exclusive with absolute
`--snapshots` and geometry capture. Offsets must be positive. Logs retain the
resolved absolute frame and snapshots retain absolute filenames; the route
decoder reports both ticks and the native game counter. This corrects checkpoint
alignment across boot variation; it does not make simulation deterministic or
remove snapshot overhead. These runs are not performance baselines.

### Movie ring backpressure

Run `dotnet run --project tools/RecompOne/tests/MovieStreamingRegression -c Release`.
It feeds a one-slot synthetic movie ring through the actual shared producer and
consumer, forces backpressure, verifies12 consecutive headers/payloads and worker
progress, and stops/joins the worker. The maximum consumer call is observational,
not a portable timing threshold. The producer must wait outside the consumer lock.
The51 assertions also hold a real producer disc read across hard reset, verify
reset waits until the worker exits, then restart and check the first requested
frame has fresh data. The pre-fix test fails because reset returns during that
read. This verifies shared movie-worker lifetime, not native full-game reset,
XA-worker lifetime, all stream transitions or the cause of steady-game stalls.
They additionally gate a disc read across StUnSetRing and verify teardown waits
for it and returns with no live producer. The pre-fix teardown test fails because
it returns while the read is held; that establishes an unsafe lifetime boundary,
not proof that a historical gameplay failure was caused by memory corruption.
XA-worker lifetime now has its own controlled blocked-read/reset/restart checks
in TimingRegression (314 passing assertions total). It verifies the old worker
has exited before reset returns, reading stays stopped, and a newly positioned
stream reads through the replacement worker. This still does not substitute for
native full-game hard-reset or ordinary transition acceptance.
For native timing comparisons, use `--keep-boot --host-counters` on the native
runner, with no shots/snapshots or concurrent builds. The runner starts both
external witnesses and records their exits. Retain startup/loading separately
from sustained gameplay; phase-group span rate is not gameplay speed when it
contains menus and disjoint movie intervals.


### Optional process CPU witness

Add `--host-processes --host-counters` to a bounded native run when host CPU
pressure overlaps stalls. `host-processes.jsonl` records top ten accessible CPU
consumers plus game, by name/PID/creation identity, with QPC collection bounds.
No command lines, priority changes or external process termination. Missing
protected/new/exited processes are incomplete coverage, not zero CPU. One
`cores_used` means one CPU second per wall second. `performance_stalls.py`
retains overlapping coarse process windows; correlation is not attribution.
The Windows limited-handle collector replaces an expensive psutil prototype;
`chase-moving-process-witness` and `sm2-process-witness` used the prototype and
are explicitly overhead-affected diagnostic evidence. Run focused tests with
`python tools/test_performance_host_processes.py` and
`python tools/test_performance_stalls.py`.


### First-use compilation diagnostics

`--stall-trace --jit-trace` collects verbose GC/contention/JIT/loader events
without sampled CPU stacks. Export with PerformanceTrace, then run
`python tools/performance_jit.py RUN_FOLDER` for main-thread compilation spans
in first native-update gaps above100ms. Spans are event-pair wall time, not CPU
measurements; preserve raw events and trace loss/coverage. Never infer that no
reported hitch means full runtime acceptance. `--executable CANDIDATE.exe`
selects a separate published candidate and records its actual source/hash;
normal owned-run cleanup still applies. Current ReadyToRun experiment is a
candidate comparison, not a replacement for movement/rendering/audio testing.


Use `performance_jit.py RUN_FOLDER --all-gaps` to examine every>100ms interval,
including later gameplay; default remains first native-update hitches. Reports
include phase/gamecounter, explicit trace coverage, main-thread JIT union wall
time and matched method spans. GPU-batch records are excluded. Zero measured
JIT with missing coverage is not evidence of no compilation.
