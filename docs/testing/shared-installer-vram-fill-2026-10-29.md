# Shared installer retries and VRAM fills

Date: 2026-10-29 (America/Indianapolis).

**Outcome:** two shared-runtime defects were reproduced with short synthetic
checks, repaired, and published in
[`dcd7ce0`](https://github.com/GTTeancum/SpideyPS1recomp/commit/dcd7ce01d4f9cfbc1c7617ce31862a1a01d8b7bb).
Both games use these common runtime paths. The work follows the maintainer's
priority of infrastructure repairs rather than isolated encounter navigation.

| Verification | Before | After |
|---|---:|---:|
| Installer validation, sectors and actual retry worker | 47/51; exit 1 | 51/51; exit 0 |
| Expanded GPU transfer/fill suite | 100/128; exit 1 | 128/128; exit 0 |
| New fill checks within that suite | 24/52 | 52/52 |
| Actual OpenGL 3.3 context, repeated subset | Not used as a paired baseline | 64/64; exit 0 |
| Full local gate | Not rerun as a before baseline | Shared build plus all 15 suites passed |

[Windows and Ubuntu CI](https://github.com/GTTeancum/SpideyPS1recomp/actions/runs/40988010704)
completed successfully on the published code commit. Windows ran the shared
build and default synthetic checks. Ubuntu additionally ran the native OpenGL
and SDL suites and the separate actual GL3.3-context step. This does not verify
Windows GPU behavior or either game's campaign.

The [machine-readable evidence](shared-installer-vram-fill-2026-10-29-results.json)
retains assertion lines, context strings, source/binary hashes, raw-log hashes,
all 16 local gate steps, and the CI receipt. No retail data is included.

## 1. Successful installer retries retained an earlier failure

`FirstRunDiscInstaller.Session.Start` cleared the error displayed in the UI but
left its separate public `Error` property unchanged. The worker set `Error`
after an extraction failure. A subsequent successful worker set `Result` and
`Finished` without clearing that previous error. The outer `EnsureInstalled`
method checks `Error` after completion, so a successfully recovered installation
could still be reported as failed.

The production repair resets `Error = null` beside the existing UI-error reset,
after the selected disc passes validation and before starting the next worker.
A new worker failure continues to populate the error normally.

### Executed reproduction

The new `InstallerRetryChecks` enters the actual private session's `Start`
method through reflection and waits for its real worker. It uses a 49,152-byte
synthetic ISO and an assembly double that supplies a tiny ZIP stream. The real
`DiscFs`, `LooseDiscImporter` and `BundledAssets` implementations execute.
Neither the synthetic boot-file contents nor any retail executable is run.

For each supported game profile, the test performs two recovery sequences:

1. Put a file at the intended disc-output directory. Confirm the worker fails,
   leaves that file intact, and exposes retry. Remove the blocking file and
   retry the same session with the same image.
2. Put a file at the bundled-assets destination. Confirm disc extraction commits
   but the bundled stage fails. Remove that blocker and retry the same session.
   The importer resumes with the already committed disc files.

Before the repair, all four retries produced the expected disc and bundled
bytes and published their successful installation paths, while retaining the
old public error. Those four stale-error assertions failed. After the repair,
all four pass. Additional checks cover the commit marker, retry availability,
failure reporting, and correcting an invalid image selection.

There are 17 existing disc/sector checks and 34 new worker/fixture checks in the
51-check suite. The paired executions took approximately 0.318 and 0.457 seconds;
these are run durations, not performance benchmarks. The outer ImGui window and
`EnsureInstalled` event-pumping loop were not exercised.

## 2. VRAM fills clipped at the GPU texture edge

The GP0 fill implementation already wraps CPU VRAM coordinates at 1024 by 512.
`GlCore.FillRect` previously sent the corresponding unsplit rectangle to its
OpenGL backend. The clear operation clipped at the texture boundary and left
the wrapped-origin pixels unchanged. Unlike the earlier upload defect, these
fill failures produced **wrong pixels without an OpenGL error**.

The shared repair normalizes the origin, treats empty rectangles as no-ops,
and divides a wrapped constant fill into at most four in-bounds regions. It uses
stack storage, with no heap allocation for the region list. Ordinary fills
still issue one backend clear. Repeating a constant fill beyond a complete
VRAM axis has no further effect, so each dimension is capped at that axis size.

Cached display targets also receive the affected regions. Partial fills retain
neighboring dirty pixels. A target covered by the union of wrapped regions uses
the existing full-clear path, preserving widescreen-margin clearing and the
model-depth reset behavior. No backend-specific implementation was added.

### Executed reproduction

The expanded `GpuTransferRegression` sends actual GP0 fill commands, compares
full GL readback against the existing CPU VRAM implementation, and checks GL
errors. It explicitly instantiates both GL33 and GL45 backend classes at 1x and
4x scale. New cases cover:

- Command X alignment and width rounding, exact-edge and ordinary controls.
- Horizontal, vertical and simultaneous edge wrapping.
- Full-width fills at nonzero X and maximum-height fills near the bottom edge.
- Empty dimensions and the fill command's existing mask behavior.
- Wrapped updates to a dirty display target without losing neighboring draws.
- Full-width wrapped clears whose combined regions cover the display, including
  its host-only widescreen margins, checked through the presented image pixels.

The 76 previous initialization/upload/readback checks passed before and after.
The new 52 fill checks changed from 24 passing to all 52 passing, producing
100/128 before and 128/128 after for the expanded suite. These comparisons
demonstrate agreement with the project's CPU implementation, not a new physical
PlayStation hardware conformance study.

Both normal runs received Mesa 4.5 core contexts, although they used different
backend classes. A separate Mesa-limited run confirmed an actual 3.3 core
context and passed its 64-check subset. Those 64 checks overlap the normal
suite; they are not 64 additional independent scenarios.

## Evidence identity and execution

The baseline source was
[`32887fb`](https://github.com/GTTeancum/SpideyPS1recomp/commit/32887fb30f79b4fd54cf4ddadcb430c419575698).
Each targeted before/after pair used unchanged test source/project files and
the **same compiled test DLL**. Only the runtime DLL was replaced with the
combined repair build. Both production patches were independently reviewed.
The integrated gate subsequently rebuilt and ran the projects normally.

- .NET SDK: 10.0.403, restored from Microsoft's release metadata with its SHA512
  verified before extraction.
- Renderer: Mesa 25.2.8, llvmpipe (LLVM 20.1.2), cloud Linux with Xvfb.
- Baseline shared build: six existing warnings, zero errors. The incremental
  repair build reported five existing runtime warnings and zero errors; the
  recompiler was already up to date. No warning cleanup is claimed.
- Local full gate: all 16 steps passed, consisting of one shared build and 15
  suites. `git diff --check` passed.
- Published source: all six changed file blobs matched the verified local
  content before synchronizing the checkout.

An initial graphics launch failed before assertions because `/usr/bin/time`
was absent; the retained paired runs used a Python timer. An initial restricted
build returned failure without compiler diagnostics; the identical build in
the authorized normal cloud execution environment succeeded. These setup
attempts are not counted as reproduced application defects.

Evidence review also found the first saved strict-context log incomplete:
58 assertions were retained without the completion summary. It was preserved
but excluded from the final 64-check evidence. A separate repeat captured the
completed child process, wrote and synced the log, and verified its 64 PASS
lines, zero failures, actual 3.3 context, final summary and exit 0. That complete
repeat took 1.997 seconds and is the strict-context record in the JSON evidence.

## Commands for the current source

With .NET 10 and Python 3 available:

```sh
python tools/run_regressions.py --suite DiscInstallRegression
```

For the native GPU suite, install the dependencies documented in the
[test README](../../tools/RecompOne/tests/README.md), then on headless Linux:

```sh
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe xvfb-run -a python tools/run_regressions.py --suite GpuTransferRegression
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330 xvfb-run -a dotnet tools/RecompOne/tests/GpuTransferRegression/bin/Release/net10.0/GpuTransferRegression.dll gl33
```

The full local gate used `tools/run_regressions.py --include-native` under Xvfb,
with `SPIDEY_GUARD_AFTER_FRAME=2000`. `--dotnet` selected the pinned local SDK.
No game assets were needed or downloaded for this batch.

## Remaining scope and next action

[Issue #20](https://github.com/GTTeancum/SpideyPS1recomp/issues/20) remains open,
unconfirmed and awaiting a screenshot/details. These independent VRAM repairs
do not establish its cause or resolve the rainbow-texture report. No retail
gameplay, hardware GPU, Windows GPU or installer-window interaction was tested.
No release package was published.

**Next concrete target:** reproduce wrapped texture-cache invalidation through
short CPU-only GP0 tests. `VramTracker.Bounds` currently clamps rectangles;
wrapped fill/upload/copy commands pass the original rectangle, potentially
leaving the wrapped-origin block's generation and dirty state unchanged.
`TextureResolver` uses those generations for cache reuse. This is a source
finding, not yet an executed reproduction or a completed fix.

Other deferred source findings are the installer's existing-manifest fast path
when loose data is invalid, a possible worker-task assignment race, and VRAM
copy wrapping/overlap/mask semantics. They need their own bounded reproductions.
All test processes from this batch have exited. Resume from the repository
[checkpoint](../../CHECKPOINT.md); preserve #5 closed and #10/#11 open.
