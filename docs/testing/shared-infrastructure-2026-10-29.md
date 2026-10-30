# Shared renderer and input infrastructure verification

Date: **2026-10-29 (Eastern Time)**.

## Outcome and scope

Published source: [f579d39](https://github.com/GTTeancum/SpideyPS1recomp/commit/f579d39318d5079d072a30f728ebc99432cbfe48).
Three demonstrated defects were repaired in the runtime shared by SM1 and SM2:
wrapped VRAM uploads/readbacks, stale analog input after controller removal or
replay completion, and null/partial input settings. These are common runtime
paths rather than encounter-specific patches.

The [rainbow world-texture report, #20](https://github.com/GTTeancum/SpideyPS1recomp/issues/20)
was created separately as **unconfirmed; screenshot pending**. Its deleted
original issue, exact game/build/model, GPU/driver and reproduction are not
available. No connection between that report and these repairs is established.

## Paired before/after results

The baseline runtime was built from `8ce5bf7`. The fixed runtime was built from
the candidate files subsequently published as `f579d39`; all 12 published blobs
were matched against the local files. Tests ran in the cloud with .NET SDK
10.0.403. Graphics tests used Xvfb and Mesa 25.2.8 llvmpipe software OpenGL.

| Production path exercised | Baseline | Fixed result |
|---|---|---|
| Actual GP0 commands, both GL backend classes, 1x and 4x scale | 20/76 passed; 56 failed, including invalid wrapped GL uploads | **76/76 passed**, no GL errors in the asserted transfers |
| Actual settings loader and input consumers | 2/10 cases passed; 8 failed. Three minimal valid JSON cases threw `NullReferenceException` | **10/10 passed** |
| Actual input polling, including SDL virtual-controller attach/tilt/button/detach | 14/19 passed; 5 failed, including stale axes after observed removal | **19/19 passed** |
| Additional actual OpenGL 3.3 context, GL3.3 backend, 1x and 4x | Not run as a separate baseline | **38/38 passed**, exit 0 |

The ordinary input suite's **13/13** checks are included in the 19-check native
run; they are not 32 distinct checks. The strict GL3.3 run repeats the graphics
scenarios with a lower context capability; it is not 38 new scenarios. In the
normal two-backend run, Mesa supplied actual OpenGL 4.5 contexts even when the
minimum request was 3.3. Explicit backend selection still exercised both
implementations; the separate limited-context run verifies actual 3.3 support.

The complete local gate passed **16 steps**: shared build plus 15 suites,
including the two optional native suites. The standalone strict-context run
also passed. The initial sandbox display-startup failure occurred before any
transfer checks, was retried with authorized cloud display access, and is not
included in the baseline transfer results.

[Retained result lines, source identities and local gate results](shared-infrastructure-2026-10-29-results.json)
provide the before/after evidence without private paths or game data.

[GitHub Actions run 40977096625](https://github.com/GTTeancum/SpideyPS1recomp/actions/runs/40977096625)
passed for the published source on **Windows and Ubuntu**. Both ran the shared
build and portable synthetic suites; Ubuntu also passed real OpenGL, SDL
virtual-device and strict GL3.3-context steps. Windows GPU/gameplay was not run.

## What changed

### VRAM edge transfers

The CPU GP0 implementation wraps image coordinates at the native 1024-by-512
VRAM edges. Previously `GlCore.WriteVram` and `ReadVram` forwarded an unsplit
rectangle to the GL backend. An upload beginning at `(1022, 511)` with dimensions
`4 x 2` crosses both edges; the unsplit GL texture update is outside the texture
and fails with `GL_INVALID_VALUE`. Readback and display-cache synchronization
also failed the wrapped cases.

The shared code now splits these transfers at the VRAM edges. Horizontal
splits preserve the original source/destination row stride with a pooled packing
buffer; vertical-only splits use contiguous rows directly. Ordinary in-bounds
transfers retain the one-call path. Each region synchronizes or writes back the
relevant display target, and temporary buffers return to the pool in `finally`.
Span bounds are checked before native transfers.

The test uses the existing CPU VRAM implementation as its oracle, actual GP0
commands, real GL readback and GL error checks. Controls include odd dimensions,
exact edges, horizontal/vertical/both-edge wraps, full VRAM extents from a
nonzero origin and dirty display targets. Fill/copy semantics, tracking and mask
handling were not changed by this repair.

### Controller lifecycle

Previously player-one stick values were updated only when a controller existed.
With no SDL input available, neither player's axes were reset. A removed tilted
controller or completed analog replay could therefore leave movement active.

Both players' axes now start neutral on each gamepad poll. Available devices
replace those defaults, then replay/script overrides still apply in their
existing order. Tests preserve keyboard buttons, independent player-two input,
script merging, exclusive script behavior and active replay precedence. The
native test creates and removes an SDL virtual device through production event
handling; it does not simulate the device by merely changing the connected flag.

### Input settings

JSON permits explicit null values despite non-nullable C# declarations. Null
profiles or binding members previously reached input consumers and caused
exceptions. Also, replacing a partially specified player-two profile invoked
player-one constructor defaults for omitted controls.

Input profiles now populate their own initialized defaults and repair explicit
nulls before use. Player two's omitted controls stay unbound. Empty strings and
arrays remain deliberate unbindings; explicit custom arrays replace defaults
without appending. Other valid settings survive. Loading does not rewrite the
user's settings file; save/reload preserves the effective mappings.

## Reproduction commands

From the fixed checkout, with .NET 10, Python, Pillow, SDL2, Mesa/OpenGL and Xvfb
installed on Linux:

```sh
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe xvfb-run -a python tools/run_regressions.py --include-native
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330 xvfb-run -a dotnet tools/RecompOne/tests/GpuTransferRegression/bin/Release/net10.0/GpuTransferRegression.dll gl33
```

The new test sources were first built against the baseline runtime to establish
the failures, then exercised against the repaired runtime. Running only the
fixed checkout demonstrates the after state. The default runner, without
`--include-native`, still supplies the portable synthetic gate for Windows and
Linux. [Test documentation](../../tools/RecompOne/tests/README.md) describes the
native prerequisites and individual suite selection.

## Limits and next infrastructure work

No retail executable, private game assets or physical controller were used in
this batch. It does not establish gameplay acceptance, Windows GPU behavior,
hardware GPU performance, or a fix for #20. Issues #10 and #11 remain open; #5
stays closed as resolved for the next release. No release package was published.

Next, reproduce the shared installer's stale error after a failed extraction
followed by a successful retry, then repair its lifecycle if confirmed. The
current observation is source-only. Review remaining VRAM fill/copy boundary
semantics as a separate bounded task with equivalent actual-GL evidence. Long
encounter navigation is no longer the immediate work priority.
