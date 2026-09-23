# Performance assessment and implementation handoff: both games

**Status: performance is not signed off.** The existing repairs improve specific reproduced defects, but neither a 30 FPS cap nor the local runs establish that the reported slowdown is resolved across PCs. This audit found a reproducible scheduling problem under modest load, significant shared CPU costs, a missing implementation described by an older renderer document, and insufficient frame-time telemetry for release acceptance.

Assessment performed September 16, 2026 local time (America/Indianapolis), September 17 UTC. Repository: `C:\Programming\GitHub\OpenSpideyPS1`. HEAD: `b100572675d4caba3cf3162529b300e88be033f4`, **plus the existing uncommitted repairs**. HEAD alone does not identify the candidate.

This is the durable handoff for resuming implementation at medium reasoning. Production code, existing documentation, settings, and GitHub issue state were not changed during the assessment. New work consists of this report and isolated, ignored analysis harnesses/logs. No desktop control, OS input, screenshots, video, or audio captures were used. Process-local scripted game input was limited to opening the test scenes. These performance runs are not new visual or audio acceptance tests.

## Start here when resuming

1. Read the evidence qualifications and P01/P02 before changing the clock. The new load probe demonstrates reduced scheduling headroom; it does **not** validate a replacement clock or every native wait path.
2. Keep both games in every shared optimization's acceptance matrix. SM1 sewer results do not establish SM2 performance, and neither stationary scene establishes combat, traversal, movie, or costume performance.
3. Do not treat slowdown as a weak-machine problem. Test strong CPUs and discrete GPUs as well as integrated graphics. Timing, serialized work, driver synchronization, polling, and logging can affect any tier.
4. Preserve the prior fixes listed below. Do not restore paired simultaneous IRQs, remove provenance for speed, disable CallRing's progress mechanism, or edit generated C# as the durable implementation.
5. Implement the ordered work packages near the end. Each finding includes evidence, a bounded next step, and the behavior that must survive.
6. `TODO.md` now lists remaining work, with completed entries removed in the subsequent cleanup. Performance issues #1/#2/#3 remain unresolved until measured acceptance. Historical repair evidence remains in `docs/timing-and-audio-repairs.md`; removing completed to-do entries does not remove the invariants below.

## Reports and what they actually establish

Open issue bodies/comments were refreshed read-only and saved in `proof_render/performance-audit-2026-09-17/github-issues.json`.

| Report | Evidence relevant to performance | Interpretation limit |
| --- | --- | --- |
| [#1: Poor performance on both pcs here](https://github.com/GTTeancum/OpenSpideyPS1/issues/1) | Both PCs have Ryzen 5 5600G and 16 GB RAM; one RX 580, one Vega 7. Reporter says below approximately 25 FPS regardless of settings, with low aggregate CPU/GPU use. | A shared CPU/timing/driver path is plausible. This does not prove a particular CPU, GPU, or RAM shortage. Aggregate utilization is not main-thread headroom. |
| [#2: No dialogue in game or music in the press start screen and poor performance](https://github.com/GTTeancum/OpenSpideyPS1/issues/2) | Reports poor performance and one core in use. Attached log identifies GTX 1650 Max-Q, GL 4.5, **2x rendering with FXAA off**. Historical intervals show approximately 29.3–29.5 presentations but only 14.7–14.8 DrawOTag calls/s. | Presentation FPS concealed a low game/draw rate. A higher frame counter alone cannot close this issue. Missing dialogue also had independent CD-state/routing faults. |
| [#3: Issues related to framerate variance](https://github.com/GTTeancum/OpenSpideyPS1/issues/3) | Chase Venom order, web artifact, music interruptions, missing dialogue, and poor performance; reporter says DuckStation ran the games correctly on the same laptop. | Separate simulation cadence, rendering correctness, audio scheduling, and host throughput. A cap addresses only part of this combination. |

Issue #2's attachment is preserved as `issue-2-spidey.log` in the audit evidence directory. Issue #5's crash log also identifies an NVIDIA RTX 2000 Ada adapter, but it is a crash report, not evidence that this adapter had the same sustained slowdown. Do not conflate them.

## Priority register

Priorities here order performance work, rather than asserting every candidate is a measured release blocker. **Confirmed mechanism** means source/reproduction establishes the behavior. **Measured hotspot** means sampled attribution supports investigating the code, not that removing it would recover its full inclusive time. **Candidate** requires an isolated before/after measurement.

| ID | Priority | Offender or confidence gap | Evidence | Coverage |
| --- | --- | --- | --- | --- |
| P01 | P0 | IRQ wait reanchors to actual delivery and consumes frame headroom | Confirmed source-linked load reproduction | Shared; both games |
| P02 | P0 | Existing FPS/profile logs cannot establish pacing, CPU/GPU headroom, or stutter | Confirmed instrumentation semantics | Shared; both games |
| P03 | P1 | VBO offset-zero reuse; documented bounded stream absent | Confirmed source mismatch; synchronization/driver allocation impact unmeasured | Shared GL renderer |
| P04 | P1 | Provenance tracking and general memory/register bookkeeping on nearly every emulated instruction | Measured shared hotspot; source identifies amplification | Both generated ports |
| P05 | P1 | Texture/material resolution, dictionary work, and locks per textured triangle | Measured shared hotspot | Both games, including stock selection |
| P06 | P1 | Active custom suit scans loader/model/material tables per triangle | Confirmed multiplicative work; active-mod runtime cost unmeasured | Both games, distinct addresses |
| P07 | P1 | Movie polling repeatedly enters a lock while waiting for the next frame | Measured SM2 startup hotspot; shared stream path | SM2 measured; SM1 equivalent needs targeted profile |
| P08 | P1 | Remaining default diagnostics, eager formatting, byte-level console mirroring | Confirmed activity; prior logging/audio A/B, incremental cost unmeasured | Shared plus SM1 hooks |
| P09 | P1/P2 | Large native geometry routines and blanket NoInlining limit main-thread throughput | Measured geometry hotspot; optimization gains unmeasured | Both games |
| P10 | P2 | Per-batch destination barriers/copies, render-target feedback, multipass drawing | Confirmed paths; GPU duration/benefit unmeasured | GL 4.5 and especially GL 3.3 |
| P11 | P2 | Internal resolution, FXAA, texture uploads and incomplete memory budgeting | Source plus 1x/4x comparison; first-use/eviction hitches candidates | Both games |
| P12 | P2 | Audio polling, sample-level locks, native music scheduling tied to console time | Confirmed architecture; no major steady lock stall demonstrated here | Both games |
| P13 | P2 | Startup/loading JIT, blocking GC, synchronous disc/asset decode and MDEC | Startup profile + source; steady GC ruled out in sampled intervals | Shared with different game flows |

## New measurements

### Environment and method

Current host: AMD Ryzen 7 8745HS, 8 cores/16 logical processors, Radeon 780M integrated GPU; Windows 11 Pro build 26200; OS-visible RAM 29,832,409,088 bytes. Driver 32.0.23033.1002; game GL string `4.5.0 Core Profile Context 26.3.1.260309`. Inventory is saved in `hardware.json`. WMI's AdapterRAM field is not accepted as an accurate total graphics-memory budget, especially on an iGPU.

Tests ran one game at a time, hidden through the game's own harness, with normal production diagnostics, FXAA on, VSync off, no image/audio/geometry capture, and separate temporary configs/assets/logs. Each normal run exits at console tick 6000, approximately 107 seconds including startup. The measured steady CPU window is 60–90 seconds after launch. SM1 opens `l5a3` sewer; SM2 opens `e1m0` and advances tutorial dialogs into rooftop gameplay. No sustained traversal/combat input was supplied. Games install their embedded assets in their isolated run folders.

The initial comparisons deliberately force **16:9 in both games**. This is SM2's current default; **SM1's current default is 4:3**, so a separate `sm1-4x-native-aspect` run covers it. Both default to 4x internal rendering and FXAA. Output window is seeded at 1280x960 but the host can resize it for aspect/panel fit; it is not a claim of a constant 1280x960 framebuffer. Read the final window and render-scale logs when reproducing.

Published executable identities used throughout:

| Game | Source binary | SHA-256 |
| --- | --- | --- |
| SM1 | `proof_render/issues-1-3/native-probe/SpiderMan.exe` | `fe06b48a83ac17dcbd16c0abbac624f2c3e80a73dbd5d482fe18e4a915d6e455` |
| SM2 | `proof_render/issues-1-3/native-probe-sm2/SpiderMan2.exe` | `9c6fafd8916ef3ea1a3529b6b7438c751b55ed17a7fc364db58074b5737b22ee` |

These are the previously published local repair candidates, not a new public release or a freshly rebuilt audit binary. Record source/config/bundle hashes and publish command in the next implementation build; matching HEAD alone is insufficient.

### Resolution comparison

CPU values below are **logical-core equivalents**: 1.0 means one logical processor fully busy, not 100% of the whole machine. Main thread is identified from the process/thread samples. The denominator matters: 0.48 core is only about 3% of total capacity on this 16-thread host, while that one thread can still be the critical path. Values include spin time and any driver work charged to the process; they are not pure native-game CPU time.

| Run | Scale/aspect | Process CPU cores | Main CPU cores | Private MiB at start/end of steady window | Peak private MiB over run |
| --- | --- | ---: | ---: | --- | ---: |
| `sm1-4x-trace`* | 4x / 16:9 | 0.462 | 0.378 | 478.1 / 475.5 | 545.2 |
| `sm1-1x-clean` | 1x / 16:9 | 0.440 | 0.377 | 426.1 / 428.1 | 474.7 |
| `sm2-4x-clean` | 4x / 16:9 | 0.572 | 0.486 | 488.7 / 484.6 | 570.5 |
| `sm2-1x-clean` | 1x / 16:9 | 0.579 | 0.475 | 418.3 / 410.9 | 457.9 |
| `sm1-4x-native-aspect` | 4x / 4:3 | 0.404 | 0.339 | 474.0 / 476.2 | 536.2 |

*The misleadingly named first run attempted an unsupported Windows `cpu-sampling` trace and the collector exited 3. The game exited normally; its process/thread samples remain usable, but it is **not a valid profiler trace** or a perfectly pristine control. The collector attempt is disclosed rather than silently relabeled.

SM1's steady logs are approximately 29.9–30 game updates/presentations per second. SM2's unprofiled 4x run reports approximately 29.6–29.8 in its final three intervals; 1x reports approximately 29.9. These are coarse intervals, not frame-time percentiles. All four games exited normally, with no watchdog timeout. The comparisons are single runs, not statistically established speedups.

Reducing scale sixteenfold in pixel area barely changed main-thread CPU demand in these scenes. It reduced private memory, but did not remove the shared CPU work. This supports prioritizing clock/CPU paths; it does **not** establish that GPU performance is adequate in every scene, at every scale, or on every adapter. Current short runs do not reproduce runaway private-memory growth; they also cannot exclude a longer driver allocation problem.

The additional SM1 4:3 run exited normally after 109.375 seconds and reported approximately 29.9–30 updates/presentations in steady gameplay. Its main-thread demand was lower than the forced-16:9 run, but this single comparison does not isolate culling, scene timing, or thermal variation into a reliable percentage benefit. Widescreen and internal render scale are separate test axes.

### Steady gameplay profiles

Successful profiles: `sm1-4x-profile` and `sm2-4x-profile`, approximately 30 seconds each. Installed `dotnet-trace` 9.0.661903 used `dotnet-sampled-thread-time`, plus CLR GC/contention events. [Microsoft documents this as sampled managed thread-time attribution](https://learn.microsoft.com/en-us/dotnet/core/diagnostics/dotnet-trace), with limitations for native/kernel attribution; it is not a GPU timer or a precise CPU sampler.

| Method/path | SM1 sampled inclusive seconds | SM2 sampled inclusive seconds |
| --- | ---: | ---: |
| `FrameClock.WaitForVBlank` / `WaitUntil` | 17.337 | 17.434 |
| `LibGpu.DrawOTag` | 2.410 | 2.311 |
| `TextureResolver.Resolve` | 1.654 | 1.472 |
| `GteScreen.TryGetRamVertex` | 1.593 | 1.232 |
| `GteScreen.StoreU32` | 1.160 | 1.147 |
| `GteScreen.LoadU32` | 0.947 | 0.708 |
| `GlCore.Flush` | 0.750 | 0.854 |

`DrawPrimSet_Impl` additionally has approximately **1.630 / 1.738 seconds of managed leaf attribution**; `Gte.Rtp` approximately 0.328 / 0.445. Inclusive rows overlap; **never add them**, interpret them as exclusive CPU seconds, or derive promised percentage gains. Sleep/spin/native waits are included where the stack remains in that method. Inlining may omit intermediate methods; different stack shapes do not prove duplicate work.

Profiler overhead is material. SM1 process CPU rose from approximately 0.462 to 0.619 core in the profiled run. Sampled `PollGCWorker` accounts for approximately 1.92 seconds in each profile. CLR `SuspendOther` events associated with sampling are **not garbage collections**. Neither steady trace contains `GC/Start`; only 8/11 allocation ticks were observed. Actual CLR contention totals across threads are **3.1241 ms SM1 / 1.7471 ms SM2**, not the approximately 1.7/1.5 seconds attributed to the `Monitor.Enter_Slowpath` stack. A lock-entry hotspot can be repeated overhead without long blocking contention.

Profiles rank investigation targets. Clean runs, scoped instrumentation, and before/after tests must quantify any repair.

## P01 — Scheduling loses headroom when work crosses one console interval

**Source:** `tools/RecompOne/RecompOne.Runtime/Host/FrameClock.cs:54`, `Runtime.cs:164`, `sdk/LibEtc.cs:42`; both `patches/Timing.cs`. `WaitForVBlank` waits for `_lastVBlank + 16.667 ms`, then assigns `_lastVBlank = actualNow`. Normal native loops can request two sequential counter changes per update. If preceding work misses the first target, the late delivery becomes the starting point for another full 16.667 ms wait.

The independent `ClockLoadProbe` links the **unchanged production FrameClock.cs**. It performs a known amount of busy work, calls two sequential waits, and throttles presentation; 5 warmups then 75 measured cycles per load. No game, GPU, assets, audio, or driver rendering is involved.

| Work before waits | Cycle rate/s | Median cycle ms | p95 ms |
| --- | ---: | ---: | ---: |
| 0 ms | 30.000 | 33.333 | 33.336 |
| 8 ms | 29.994 | 33.336 | 33.354 |
| 15 ms | 29.987 | 33.335 | 33.433 |
| 18 ms | 28.838 | 34.672 | 34.680 |
| 20 ms | 27.267 | 36.672 | 36.677 |
| 25 ms | 23.995 | 41.672 | 41.678 |
| 30 ms | 21.425 | 46.672 | 46.677 |

For this work distribution, usable contiguous work budget before rate loss is approximately **16.7 ms**, despite a 33.3 ms presentation target. A driver wait, logging burst, native work, or scheduler interruption can cross that threshold on a powerful machine. This is a confirmed load-induced slowdown mechanism, **not proof that every report has the same cause**, or that the probe reproduces every native instruction/IRQ path. The native loop also contains conditional waits and DrawSync handling; preserve and test those semantics.

The previous high-resolution Windows timer repairs coarse sleep overshoot. It does not change this late-edge phase policy. `LibEtc` also restarts its elapsed clock after a pump; actual game IRQ dispatch occurs after host presentation/service work. The comments saying the clock is independently 60 Hz overstate what is guaranteed under load.

**Next implementation:** design console deadlines and presentation pacing together, using an explicit policy for late IRQ delivery, elapsed console time, and missed presentation opportunities. Investigate fixed-phase deadlines and safe-point delivery; do not apply a one-line phase change without native validation. Elapsed console time, delivered IRQ count, observed game-counter changes, and display opportunities must be separately observable. Presentation remains at most 30/s, with no catch-up burst after a long stall. Do not simply dispatch two IRQs simultaneously: that previously produced approximately 15 gameplay updates/s. Do not blindly count all missed IRQs through arbitrary memory writes or make simulation race through a pause.

**Acceptance:** repeat 0–30 ms load tests, split work before/between/after the two waits, intermittent 1–100 ms stalls, sustained overloaded work, timer fallback, suspend/restore, VSync on/off, and both native games. Establish the intended 30-update behavior where total workload fits, with correct counter semantics. Preserve SM1's script-only 20-update Chase cadence, completion/skip/retry restoration, and ordinary SM2's two-counter behavior. Inspect animation, scripts, collision, transitions, speech, and music; timing counts alone do not pass.

**Secondary clock cost:** `SpinMs=1.5` can consume up to approximately 90 ms of CPU per second for 60 edge waits when each reaches its full spin window. This is a power/thermal cost, not a measured 9% saving. Adapt only after measuring wait jitter. High-resolution timer creation/failure/fallback is currently silent; log the selected backend and failure once so a fallback regression cannot masquerade as weak hardware.

## P02 — Current counters cannot support performance acceptance

**Source:** shared `Diagnostics/FrameProfile.cs:18`, each game's `patches/Diag.cs` heartbeat/rate worker (SM1 approximately line 252, SM2 254), and shared `Runtime.cs:166–185`.

`FrameProfile.GameMs` measures time from the previous present's end to the next throttle's start. **WaitForVBlank already happened before that timestamp.** Its reported approximately 33 ms of “game” is therefore not 33 ms of game CPU. “Throttle 0” does not mean no waiting. “Present 0.3 ms” is a CPU submission/present-call interval, not GPU execution. The heartbeat labels the console tick rate as FPS (approximately 60), while presentation and game updates are approximately 30. Fifteen-second averages hide individual or clustered stalls. The worker also resets unsynchronized counters, so values are not a precision frame-time record.

`RunFrame=0` can reflect an unbound hook. Zero total-vertex diagnostic counters can mean collection is disabled. Game counters reset on transitions, producing unusable deltas; tag/reset them rather than averaging negatives. CallRing totals describe call frequency, not time spent. Separate console IRQ rate, unique simulation update rate, submitted images, presentation requests, and displayed images.

**Next implementation:** a shared, low-overhead recorder with QPC/Stopwatch timestamps, explicit scene/phase labels, bounded ring buffers, and a background writer. Capture unique update starts, pre/post IRQ delivery, wait start/deadline/end, main-thread work stages, GL upload/flush/present durations, and async GPU timer queries read only after completion. Add slow-frame records and one-second distributions, not synchronous text per instruction or per draw. Use actual frame IDs to associate delayed GPU results. Count vertices, batches, uploaded bytes, target copies/barriers, texture cache hits/misses/evictions, allocations, and audio underruns. Record dropped diagnostic records.

Collect hardware automatically as specified below. Include wall frame-time p50/p95/p99/max, thresholds and consecutive misses; distinguish CPU on-time, waits, GPU busy time, and display pacing. Do not insert per-frame `glFinish` to get a timing number. Verify recorder overhead against an identical recorder-disabled run.

Optional later cross-check: process-filtered command-line [PresentMon](https://github.com/GameTechDev/PresentMon), without desktop capture or UI automation. Its OpenGL/Runtime=Other CPU timing limitations and HAGS-related GPU metric limitations mean it complements native timestamps; it is not the sole oracle. No PresentMon measurement was made in this assessment.

## P03 — Renderer documentation and actual vertex uploads disagree

**Source:** `Gpu/Backends/Common/GlCore.cs:19,211,923`. Each flush uploads vertices to **offset zero** of a single VBO, followed by draws whose first vertex is zero. Capacity is 262,144 vertices × 48 bytes = **12 MiB**. That bounds the application's buffer size, not how much storage a driver might internally rename or how long an overwrite waits for outstanding draws.

`docs/bounded-vertex-streaming.md` describes a 36 MiB fenced triple ring, persistent mapping, a GL 3.3 fallback, tests, and historical soaks. **The described implementation is absent from this checkout:** no `GlVertexStream`, `VertexRingRegression`, `BufferStorage`, `MapBufferRange`, `FenceSync`, or `ClientWaitSync` in current RecompOne source. The referenced September 3 proof folder is also absent. That document's old 3.2 GiB reproduction and repaired soak numbers are not evidence for the current source/binaries. Current short runs did not reproduce such growth.

**Mechanism:** repeatedly writing storage still consumed by the GPU may force implicit synchronization or driver backing-store allocation. Khronos discusses invalidation/orphaning and synchronization in its [buffer object documentation](https://wikis.khronos.org/opengl/Buffer_Object). Actual impact remains to be measured on Intel, AMD, and NVIDIA; do not claim a current memory leak from source alone.

**Next implementation:** reconcile/recover the intended repair or build an explicit bounded stream with fences and a tested GL 3.3 path. All normal, subtractive, coverage, and repair passes must use the correct vertex offset; no overwrite before GPU completion. Handle wrap, oversize batches, reset/disposal, and unavailable mapping features. Measure fence wait, upload bytes/time, high-water memory, and driver/process memory over normal and forced-wrap workloads. Avoid unbounded orphaning or arbitrary `glFinish` as a supposed fix.

**Acceptance:** both games, all enabled renderer paths, texture/blend/mask correctness, 30–60 minute normal gameplay, repeated scene/costume transitions, and targeted stream-wrap stress. Existing rendering regressions plus inspected native output are required. A successful draw counter is insufficient.

## P04 — Every emulated load/store pays graphics-provenance and memory costs

**Source map:** `RecompOne.Recompiler/CodeGen/InstructionEmitter.cs:135–145`; runtime `Hardware/GteScreen.cs:114–200`; `Memory/PSMemory.cs:68–123,156–232,334`; `Context/CpuContext.cs:13`; `Memory/RamLogger.cs:11,45`.

Generated LB/LBU/LH/LHU/LW use GteScreen load helpers; SW routes through provenance-aware storage. Full-word loads look up a RAM dictionary. Byte/halfword loads can read the containing word again to validate a tag. Writes invalidate tags, with halfword paths potentially repeating aligned-word invalidation. General RAM access also performs address mapping/modulo, bounds checks, idle/watch checks, and hook handling. Each normal register assignment clears five provenance arrays in addition to storing the GPR. RamLogger writes per-byte heatmap timestamps even when the RAM panel is closed; its two timestamp arrays consume 16 MiB together. GteScreen tracking is active in 4:3 as well as 16:9.

This is supported by both steady profiles, especially `TryGetRamVertex`, `StoreU32`, and `DrawPrimSet`. It is not justification for deleting perspective/subpixel tracking: those certificates prevent cracks, snapping, invalid depths, and stale-coordinate reuse.

**Next implementation:** measure hit/miss populations and separate ordinary RAM from MMIO/scratchpad paths. Consider sparse paged tags or a direct-index/generation scheme, cheap untagged register state, one invalidation per covered word, and optional heatmap/stat tracking. Preserve the retail address window/alias rules, partial and unaligned writes, scratchpad/MMIO side effects, frozen/watchpoints when active, idle progress, overlay write notifications, and exact native-coordinate validation. Bulk load/zero operations can be specialized only with equivalent range invalidation and hooks.

Change the recompiler emitter then regenerate both games; do not hand-edit generated output. Run GTE/memory/rendering regressions and inspect moving geometry, joints, distant surfaces, shadows, HUD, webbing, and transitions in both games. Measure CPU cost under dense scenes after correctness passes.

## P05 — Repeated texture resolution on the triangle path

**Source:** `Assets/Textures/TextureResolver.cs:285–381`; `Assets/AssetReplacerManager.cs:383–417`; `GlCore.cs:460–499,623`.

Even an early “no replacements” decision reads `HasTextures` under a lock. Both ports install an ActorMaterials delegate, so the null-delegate fast exit is unavailable even with no active custom suit. Per-triangle work includes statistics, region/dirty/generation checks, cache dictionaries and locks, and hashing/decoding on misses. `EnsureRepTexture` additionally updates two dictionaries on repeated hits. Profiles confirm material work in both games. They do **not** establish seconds of blocking lock contention; measured contention was only milliseconds.

**Next implementation:** immutable/read-mostly asset snapshots, a cheap inactive-actor-material state, cached material resolution at the correct lifetime, and less frequent hit bookkeeping. Keys/invalidation must cover tpage, CLUT, texture window, UV region, dirty VRAM generation, animation, model surface identity, and mod reload. Measure no-pack, bundled-pack, and active custom suit cases separately. Preserve dynamic textures/CLUTs, transparency edges, overlays, and transitions. Do not cache by tpage alone.

## P06 — Active custom costume causes repeated table scans

**Source:** `spiderman/patches/SuitMods.cs:104` and `spiderman2/patches/SuitMods.cs:105`, `Resolve(TileRect)`.

For every relevant textured triangle, an active custom suit scans up to 40 player model entries, reads the current model layout, and scans material descriptors (guard permits up to 4096). This happens even for triangles that ultimately do not match the player's material. SM1 table base is `0x800A0904`; SM2 is `0x800ACED8`. Worst-case work grows approximately with triangles × (model entries + material descriptors), on top of P04's memory overhead. Stock inactive selection returns early; **the audit's stock runs do not measure the active-mod case**.

**Next implementation:** cache the loader-authoritative player/material mapping when its identity/layout changes, then use a bounded lookup during drawing. Invalidate on costume switch, model free/reload/relocation, level retry/transition, and relevant VRAM/material changes. JSON names alone do not own addresses. Test stock and custom costumes in each game, transitions and repeated switches, including complex models and transparent materials. Benchmark world triangles as well as character closeups.

## P07 — SM2 movie wait churn is a concrete startup hotspot

**Source:** shared `sdk/LibCdStream.cs:96` (`StGetNext`), generated SM2 `MovieNextFrame` at `0x80030D08`, surrounding `func_80030D6C`/movie loops; `Diagnostics/CallRing.cs:76`; `sdk/LibEtc.cs:61`. SM1 also binds shared `StGetNext`; locate and profile its caller before adding a game-specific hook.

The clean SM2 run reaches approximately **305 million calls / 5884 stall breaks by tick 2712**, then adds only approximately 2.85 million calls per 15-second gameplay interval. These are cumulative counters; subtract adjacent samples. A targeted 18-second startup trace (`sm2-startup-profile`, starts 12 seconds after launch) identifies `MovieNextFrame → StGetNext → Monitor.Enter` as approximately **4.723 seconds of managed sampled attribution**. Actual contention across the trace totals only **0.4325 ms**, maximum 0.0813 ms. This is repeated queue polling/lock entry, not a 4.7-second blocked lock.

`StGetNext` checks an empty ready queue under a lock and immediately returns. Native code repeatedly retries; the generic call-count breaker eventually services time. Existing yielding native-vblank hooks do not remove this movie polling loop. MDEC IDCT also contributes approximately 0.440 seconds of sampled leaf time in this startup trace; handle that separately from waiting.

**Next implementation:** identify exact retry semantics and yield until a bounded next service/data deadline at the appropriate native wait site, or introduce an equivalent queue-notification path that still pumps required console/CD/pad events. Do not indiscriminately block the SDK query if callers rely on it being nonblocking. Do not disable CallRing: it currently calls `Runtime.IdleTick`, despite stale comments claiming only ServiceOnly, and supplies progress to otherwise unserviced loops. Decouple essential progress from optional diagnostics before disabling call instrumentation.

**Acceptance:** movies played to completion, skip once/repeatedly, return to title, missing/end-of-stream handling, correct frame order and audio sync in both games. No arbitrary extra IRQs or changed CD command state. Repeat movie/menu/load profiles separately from gameplay.

## P08 — Diagnostics still have avoidable hot-path work

**Source:** SM1 `patches/GameTrace.cs` (`TriggerType8`, SpawnActor/overlay checks); both games' `patches/Diag.cs:91,141`; shared `Diagnostics/ConsoleMirror.cs`, `Log.cs:44`, `sdk/LibEtc.cs:21`, and CallRing/emitter.

Prior work made high-volume SM1 actor logging opt-in and demonstrated improved music overlap. However, normal sewer logs still contain repeated **TriggerType8** messages with verbose logging disabled. Some actor hooks still build names and enumerate locked/copied `Dispatcher.ActiveNames` for required on-demand loading. Preserve that loader behavior while optimizing checks. Not every remaining hook is merely diagnostic.

Calls such as `Log.Sdk($"VSync({mode})")` build interpolated strings before the callee checks its disabled flag. The console pipeline timestamps/splits/formats lines, synchronously flushes files, and mirrors strings through character-by-character handling/locking; bounded console row removal can also shift data. Bursts can therefore stall the critical thread on a fast PC or slow storage. These residual costs were not isolated by a new A/B here.

**Next implementation:** guard expensive formatting at the call site or use conditional interpolation; gate verbose hooks before reading/formatting state; bounded asynchronous logging with preserved fatal/crash flush behavior; whole-string console writes. Keep user-facing errors and necessary diagnostics. Retain CallRing progress until P07 has an independent replacement. Benchmark a representative busy scene and verify audio timing, not just average FPS. Avoid replacing text spam with per-frame JSON spam.

## P09 — Native geometry is a serial CPU cost

**Source:** DrawPrimSet SM1 `0x8007C4D8` (11,964 retail code bytes), SM2 `0x80088B18` (12,020); GTE transform paths; `CodeGen/FunctionEmmiter.cs:27,64`; both configs `callRing: true`; both `patches/FramePackets.cs`.

DrawPrimSet is a major measured leaf in both games. Bundled higher-detail actors increase transforms, animation, packets, and texture submissions. Expanded native frame-packet pools (SM1 two 512 KiB, SM2 two 256 KiB) permit more work; capacity expansion itself is not a performance optimization. Every generated function is marked NoInlining, including small wrappers, restricting JIT opportunities even with TieredPGO enabled. The existing runtime's mutable CPU/RAM/GPU state is largely serialized; spare aggregate CPU does not make arbitrary parallel execution safe.

**Next implementation:** profile dense combat/traversal and compare packet/transform counts, then prioritize P04 and small eligible leaf specialization/inlining or bounded HLE replacements. Preserve detours, hooks, dispatch, RA/SP, delay-slot semantics, and diagnostics that depend on boundaries. Assess LODs from authored structure and measured cost, without removing bones/terminal links or repeating the rejected animation-scratch relocation. Do not recommend a multicore rewrite before measuring this path.

## P10 — GPU barriers, copies, feedback, and painter-order overdraw

**Source:** `GlCore.Flush:868`, `Gl45Vram.BeginDestRead:54`, `Gl33Vram.BeginDestRead:64`, modern primitive shader mask check, `GlCore.PresentDisplay:1145`.

Each flush invokes destination-read setup. GL 4.5 issues a texture barrier; GL 3.3 copies a bounding destination rectangle using framebuffer blit. The primitive shader's destination fetch is conditional on mask checking, but setup happens without that narrow condition. Some blending uses multiple draws. Display RTs are written back when dirty before presentation, even when the presented source is directly available. Painter-order world rendering with limited depth rejection also makes overdraw and draw-order constraints significant.

These are candidates for conditional work reduction, **not proof every barrier is redundant**. Texture feedback and mask/transparency semantics must be traced before removing synchronization or copies. Reordering polygons for batching can change the image.

**Next implementation:** count batches, barrier/copy reasons and bytes, mask/blend modes, feedback dependencies, and asynchronous GPU durations. Elide only proven-unneeded destination preparation or defer writeback until a dependent read. Preserve all framebuffer feedback, blend/subtraction, mask, shadow, HUD, and transition behavior. Test GL 3.3 explicitly: current shipping `RequestedBackend()` returns Auto, so inventing `RECOMP_BACKEND=Gl33` does not force it. A later diagnostic backend selector must log the actual result. Current audit runs all selected GL 4.5.

`WideBackgroundCompletion` defaults false in both ports; its extra coverage/world-copy paths are not a demonstrated default offender. Preserve that distinction when reading code for optional passes.

## P11 — Resolution and replacement texture lifetime

**Source:** `Config/ViewConfig.cs:53,69`; shared NativeVideoSetup; `GlCore.EnsureRepTexture:492`, `ApplyFxaa:1353`, and `PresentDisplay`; `Assets/Textures/TextureResolver` caches.

At 4x, the 1024x512 scaled VRAM surface is 4096x2048. A 320x240 display region becomes 1280x960 internally (other native display widths differ). The window/output-resolution setting does **not** change RenderScale; choosing a small output window can retain 4x internal work. FXAA uses internal display dimensions despite a comment about host resolution. The 1x/4x results above show why this setting does not eliminate CPU work.

First use of an RGBA replacement clones/premultiplies pixels, uploads synchronously, and generates mipmaps. Its min filter is GL_LINEAR, which does not sample mip levels; [Khronos documents the filter semantics](https://wikis.khronos.org/opengl/GLAPI/glTexParameter). Thus these particular generated mips are unnecessary for the current filter. The 512 MiB replacement texture budget counts only base RGBA bytes, omitting mip storage, CPU decoded pixels, temporary upload arrays, other render targets, and driver allocations. GL eviction leaves CPU decoded data and may trigger later reuploads. Texture memo/page cache growth and mod working sets need their own bounds/accounting.

**Next implementation:** instrument cold uploads, decode time, cache growth, eviction/reupload, and real memory accounting. Remove unused mip generation or deliberately choose a mip-using policy with correct budgets. Prewarm only a bounded known working set. Expose internal scale clearly and choose any adaptive quality policy from measured limits, never CPU/GPU brand alone. Test cold/warm loads, many costume changes, large texture packs, 1x/2x/4x/highest supported scale, and actual output modes on both games. Preserve premultiplied-alpha handling; removing it reintroduces actor edge artifacts.

## P12 — Audio must be evaluated alongside the clock

**Source:** `Host/Audio.cs:98,113`; `Hardware/Spu.cs:404`; `Hardware/XaAudio.cs`; `sdk/LibCd.cs:232`; native music scheduling in each game.

The host queues eight 256-frame buffers at 44.1 kHz (approximately 46.4 ms total). Its worker polls with Sleep(3); the XA reader uses Sleep(2). Mixing holds the SPU lock over a buffer, and XA Next takes a lock per output sample. These are real overhead/scheduling candidates, but the steady traces did not demonstrate large blocking stalls. Rank them below confirmed clock and CPU work until underrun/lock-duration evidence says otherwise.

Native SPU music phrase scheduling depends on game/IRQ progress; delayed phrases can create gaps even when the host audio queue never underruns. Conversely a host underrun can occur without a game-rate problem. Track both and preserve the prior CD query/routing/sector pacing/startup buffer fixes. Audio WAV capture writes on the mix thread when enabled, so do not enable it for clean performance baselines. A separate short audio validation can compare phrase order, overlap, continuity, and device restarts under representative load.

**Next implementation:** only after metrics, consider bounded block-level XA consumption and notification-driven refill with correct locking/lifetimes. Test title music, both tutorial conversations, combat effects, Chase music/speech, movie transitions/skips, and long sessions. A transcript proves speech presence, not continuity or synchronization; waveform/listening checks are separate.

## P13 — Loading, first use, GC/JIT, and movie decoding

**Source:** both `port/*.csproj` (ServerGarbageCollection=true, ConcurrentGarbageCollection=false, TieredPGO=true, compressed single-file release); `sdk/LibCd.cs:152`; `Cdrom/Disc/LooseDiscImage.cs`; `Memory/PSMemory.LoadBytes/ZeroRange`; `Hardware/Mdec.cs:238`.

No actual GC occurred in either sampled 30-second steady gameplay interval. SM2's targeted startup trace does contain **two GC starts** and 176 allocation ticks. Start-to-stop intervals were approximately 4.32 ms (generation 0, AllocSmall) and 7.92 ms (generation 2, InducedLowMemory); these intervals are not a complete suspend-to-resume pause measurement. The event reason alone does not establish a physical RAM shortage. Measure actual pause durations before changing GC mode. Profiler SuspendOther/PollGC activity is not a production GC diagnosis. [Microsoft's GC documentation](https://learn.microsoft.com/en-us/dotnet/standard/garbage-collection/workstation-server-gc) describes the tradeoffs; do not flip server/background settings based on intuition alone.

Large generated functions incur first-use JIT cost; release optimization/PGO does not remove all warmup. Single-file extraction, bundled asset installation, file reads, PNG decoding, and texture uploads complicate cold-start results. `LibCd.MaxSectorsPerTick=400000` permits a large synchronous callback burst; callbacks often terminate it sooner, so this is a risk ceiling rather than proof 400,000 sectors are read every tick. Loose disc reads allocate sector buffers; bulk emulated RAM writes traverse ordinary byte-level bookkeeping.

MDEC's scalar two-pass IDCT and color conversion are visible during movies; startup trace attributes approximately 0.440 seconds of leaf time to IDCT in 18 seconds. Its major block buffers are reused; do not invent per-block array allocation as the cause. Optimize decode only with bit/visual correctness tests and movie audio sync, after removing P07's polling churn.

**Next implementation:** phase markers for extraction/load/decode/JIT/upload/GC, cold and warm start comparisons, first use of abilities/effects/costumes, and repeat loads. Consider range-aware copies, bounded callback service, targeted prewarming, and measured GC/JIT configuration changes. Preserve CD callback ordering, data-ready semantics, and overlay reload correctness; an arbitrary background Task around synchronous native state is unsafe.

## Prior repairs to retain

The detailed historical evidence remains in `docs/timing-and-audio-repairs.md`. It verifies specific flows; this assessment narrows the performance completion claim, not those established reproductions.

| Existing repair | Both-game relevance / invariant |
| --- | --- |
| Separate 60 Hz IRQ deliveries and overall 30 FPS presentation cap | Keep both; simultaneous paired IRQs previously halved gameplay update rate. New work must also fix behavior under load. |
| Yield native interrupt-counter waits | SM1 `8005E748`, GP+`C74`; SM2 `80069124`, GP+`CD0`. Preserve retail return/counter behavior. |
| Remove redundant artificial GPU busy delay | Retain each port's zero delay; do not reintroduce a third wait per update. |
| High-resolution private Windows timer | Retain precision benefit; add observable fallback and revised late-edge policy. |
| ReadS/command-state/mute/seek/sector pacing and XA startup buffering | Shared plus game-specific CdLastCom mappings. Clock repair alone did not fix dialogue. |
| Opt-in verbose SM1 actor logging | Keep it; P08 addresses residual costs, not a reversal. |
| SM1 Chase building script at 20 updates/s | Only the authored script; presentation still at most 30; restore normal cadence on completion/skip/retry. SM2 has no equivalent special case established. |
| Wrapped off-screen particle rejection and subpixel/provenance correctness | Preserve both games' web/geometry fixes while optimizing P04/P10. |
| Correct lizard terminal LOD/bone structure | Crash was bad converted asset structure; do not restore rejected scratch relocation or single-detail model experiment. |

Existing Venom/rooftop geometry artifacts and SFD feature issue #4 remain separate. They are not silently claimed repaired here. Full normal player completion of Chase Venom has not been established by the prior route-following fixture.

## Hardware coverage and automatic logging plan

Exact user laptop models are not a prerequisite. The next shared recorder should collect them automatically, locally, with a user-reviewable export. No telemetry upload is implied. Do not collect usernames, machine serials, full personal paths, or unrelated application inventories.

Log once per run: executable/build/runtime hashes and versions; game/disc identity and bundle/selected mod hashes; CPU model and physical/logical counts; RAM capacity; OS build; active GL vendor/renderer/version/backend and driver; all GPU adapters and the actually selected adapter; internal native/scaled dimensions, window/display dimensions, refresh rate, DPI, fullscreen/VSync/VRR settings where observable; timer backend; relevant game config and diagnostic flags. Include AC/battery and power-mode state where available without changing it.

Sample locally where reliable: main-thread/process CPU, working/private memory, managed heap/allocation/GC pauses, GPU busy/memory budget, frequency/throttling indicators if exposed without installing privileged monitoring components, wait overshoot, frame distributions, and audio underruns. Static advertised CPU speed is not actual clock residency. Mark unavailable metrics as unavailable. Do not infer boost clocks, temperatures, or free VRAM from WMI fields that cannot supply them.

| Coverage axis | Required classes | Purpose |
| --- | --- | --- |
| CPU vendor/throughput | Intel and AMD; strong desktop/mobile CPUs as well as modest mobile CPUs | Detect serial/scheduling behavior independently of GPU tier. |
| GPU vendor/type | Intel Iris Xe or similar iGPU; AMD integrated; AMD discrete; NVIDIA discrete; at least one strong discrete GPU | Driver synchronization/backends and shared-memory pressure, without assuming slowdowns only occur on iGPUs. |
| Hybrid systems | Actual rendering adapter verified | Distinguish selection mistakes if observed; do not assume them from adapter inventory alone. |
| Display/pacing | 60/59.94 Hz and available high refresh (120/144+); VSync off/on; window/fullscreen | Detect cadence quantization, swap blocking, and limiter interaction. |
| Sustained conditions | AC, representative battery mode, warm sustained session, restore after background/minimize | Identify power/thermal/scheduler sensitivity on otherwise fast machines. User operates desktop transitions. |
| Content/config | Both games, stock/custom costumes, cold/warm assets, 1x/2x/4x, normal packs and large supported packs | Separate resolution, CPU geometry/material, upload, and lifetime issues. |

The available Ryzen and Intel/Iris Xe laptops are useful first cross-vendor checks, not a statistically representative population. Add a strong discrete-GPU system and relevant affected hardware when available. Do not turn a two-laptop pass into “99% of users fixed.” Define repeatable engineering gates and report actual coverage.

## Acceptance protocol for the implementation phase

1. **Identify builds and phases.** Compare the public/reported baseline and candidate with the same scene, save/fixture, asset set, selected costume, and settings. Preserve hashes. Record cold startup separately from warm gameplay. Do not compare different scenes or profiled vs unprofiled builds as a speedup.
2. **Test mechanisms first.** P01 load/late-edge tests; P03 stream wrap/fences; P04 partial writes/MMIO/provenance; P05/P06 cache invalidation. Then run existing relevant regression suites. Synthetic tests support diagnosis, not visual acceptance.
3. **Repeat native scenarios.** At least three clean 2–3 minute runs per key scenario/settings on each available hardware class; no concurrent game or build. Both games: title/movie play+skip, tutorial/dialogue, stationary view, swinging/traversal, dense combat, scene transition/retry, and costume switch. SM1: sewer lizards and normal-input Chase building sequence/cadence restoration. SM2: rooftop/tutorial plus a populated later combat area. Current stationary fixtures do not cover this list.
4. **Measure actual distributions.** Store per-frame/update timing with phase labels. Proposed initial VSync-off gate for supported settings: ordinary gameplay approximately 29.5–30 updates/s, presentation capped at 30, p99 presentation interval at most 40 ms, no unexplained >100 ms gameplay stalls, and no sustained <29 updates/s for more than two seconds. These are proposed gates, **not results achieved here**. Keep exact limiter tests separate from finite-window counter rounding. Define explicit exceptions for loading and the authored 20-update Chase script. Under VSync, document expected display-step quantization instead of silently failing/waiving the off-mode threshold. Check IRQ drift and simulated seconds over minutes.
5. **Require headroom.** A capped average of 30 is insufficient. Measure CPU work and GPU duration separately, their critical-path waits, p95/p99 spikes, and response to controlled load. Do not add overlapping CPU/GPU averages and label the sum frame time. Confirm clock cadence survives work that fits the intended budget.
6. **Check the content.** The user operates normal gameplay and desktop transitions. Use a small number of targeted native captures only when needed and authorized; inspect all captured frames sequentially. Verify backgrounds, media, transparency, motion, transitions, and expected gameplay behavior. Verify speech/music continuity and synchronization with a separate short native audio capture/listening check. Document every unverified flow. No desktop capture/control tools.
7. **Soak.** At least 30–60 minutes of normal changing gameplay per game on representative classes, with repeated scene/costume transitions. Track memory high-water and plateau/reclamation, frame tails, audio behavior, and crashes. Stationary or synthetic forced-flush soaks alone do not establish normal behavior.
8. **Compare and deliver.** Summarize before/after distributions and functional evidence per game/hardware/config, list exceptions, and keep issues open where reproduction/acceptance remains missing. Do not claim statistical 99% confidence without a defined population and evidence supporting it.

## Ordered work packages for the next implementation task

This is the original assessment checklist, retained as historical scope. For
implemented fixes, subsequent measurements, and remaining acceptance work, use
the [repair handoff](performance-repairs-2026-09-17.md) and root TODO.md.

- [ ] **A — Measurement and clock:** P02 minimal recorder + source/build fingerprint; extend P01's source-linked load probe, then design/validate the clock fix in both native ports. No other optimization is needed to reproduce the headroom defect. Preserve the prior timing/audio invariants.
- [ ] **B — Bounded GPU uploads:** reconcile P03's absent implementation; implement/test lifetime/offset/fallback behavior and normal soaks. Keep timing measurements from A so driver wait improvements are attributable.
- [ ] **C — Main-thread CPU:** P04 RAM/provenance bookkeeping, P05 material fast paths, P06 active-suit cache, each separately measured and checked in both games. Use P09 profiles to guide emitter specialization after these shared costs.
- [ ] **D — Polling and diagnostics:** P07 movie wait sites in both games and P08 residual logging/formatting; preserve IRQ/CD/pad progress and movie/audio semantics. Essential progress must no longer depend on optional call logging before disabling it.
- [ ] **E — Remaining measured tails:** P10 GPU copies/barriers, P11 uploads/cache/memory, P12 audio scheduling, P13 loading/JIT/GC/decode. Promote an item only when target hardware/scene evidence supports it. Do not perform a broad threading or rendering rewrite without a demonstrated need.
- [ ] **F — Cross-hardware acceptance:** automatic inventory/log export; user-run Intel/AMD/integrated/discrete scenarios including strong machines; repeated clean timing/content tests and sustained soaks. Update TODO/issue status from this evidence, not the original local 30 FPS observation.

## Evidence and reproduction index

All new raw evidence and analysis helpers are under ignored `proof_render/performance-audit-2026-09-17/`; this report carries the conclusions even if ignored binaries/traces are not shared. Preserve/export that directory before deleting local proof artifacts. Reports reference function names and source lines as of the assessed working tree; line numbers may shift after edits.

| Artifact | Purpose |
| --- | --- |
| `source-snapshot.json`, `source-verification.json` | HEAD and hashes of the 33 already modified tracked files; final verification found zero changes to them. |
| `hardware.json`, `github-issues.json`, `issue-2-spidey.log` | Host inventory and refreshed report evidence. |
| `ClockLoadProbe/`, `clock-load.log` | Actual unchanged FrameClock linked into an isolated load sweep. |
| `audit_run.py` | Isolated native runner/configs and 0.5-second process/thread CPU/memory samples; optional scoped trace; no images. |
| `run-summary.json`, `summarize.py` | Clean/profiler run summaries and sampled-stack attribution. |
| Each run's `environment.json`, `result.json`, `spidey.log`, `console.log` | Executable hash, environment, exit/timeout, raw samples, phase/rate logs. |
| `sm1-4x-profile/`, `sm2-4x-profile/` | Valid steady `.nettrace`/Speedscope profiles, `thread-time-summary.json`, `runtime-events.json`. |
| `sm2-startup-profile/`, `startup_summary.py` | Valid startup profile, exact movie-poll stack attribution, contention/GC evidence. |
| `TraceAudit/` | Analysis-only CLR event extraction tool using installed TraceEvent assemblies; not linked to a game. |
| `sm1-4x-native-aspect/` | Additional unprofiled SM1 4:3 default-aspect coverage; see final result and logs. |

Example commands from the repository root (Python with psutil, .NET 10, and the stated existing candidate binaries required):

```powershell
dotnet run --project proof_render/performance-audit-2026-09-17/ClockLoadProbe -c Release
python proof_render/performance-audit-2026-09-17/audit_run.py sm1 new-sm1-4x --scale 4 --wide 0
python proof_render/performance-audit-2026-09-17/audit_run.py sm2 new-sm2-4x --scale 4 --wide 1
python proof_render/performance-audit-2026-09-17/audit_run.py sm1 new-sm1-profile --trace
python proof_render/performance-audit-2026-09-17/audit_run.py sm2 new-sm2-startup --trace --trace-start 12 --trace-duration 18 --frames 2300
python proof_render/performance-audit-2026-09-17/summarize.py
```

Run game commands sequentially. Use unique output names; the runner reuses an already copied executable in an existing folder. It clears inherited SPIDEY_/RECOMP_/DOTNET_/COMPlus_ variables for isolation, disables image capture, uses process-local input, and has a 155-second outer guard plus native tick exit. This is an audit harness, not a future user-facing benchmark package. The startup run's 12–30 second window must not be summarized using the steady 60–90 second window.

Known limitations: one host, short mostly stationary gameplay, no foreground presentation/display timing, no GPU timer measurements, no active custom-suit profile, no GL 3.3 run, no new graphics/audio inspection, no long normal gameplay soak, and no new end-to-end ordinary-input Chase completion. Those are explicit implementation/acceptance tasks, not assumed passes.
