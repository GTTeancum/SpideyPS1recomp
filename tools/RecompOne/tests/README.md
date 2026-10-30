# Regression checks

From a clean source checkout, install .NET 10 and Python 3, then run:

```sh
python -m pip install Pillow
python tools/run_regressions.py
```

The runner builds the shared runtime and recompiler, then runs the synthetic
console projects, build configuration checks, and converter fixtures below. It exits nonzero if a build or suite fails,
continues through independent suite failures, and retains each process log plus
`results.json` under `artifacts/regressions/`. A shared build failure marks the
.NET suites as skipped, never passed; independent configuration checks and Python fixtures still run.
GitHub Actions runs these checks on Windows and Linux and retains the logs as
workflow artifacts. Linux additionally runs the native OpenGL and SDL checks
described below.

Use `--dotnet /path/to/dotnet` for a local SDK, or repeat `--suite NAME` to select
individual suites. Every invocation still builds the shared tools.
CI sets `SPIDEY_GUARD_AFTER_FRAME=2000` so corruption-watch checks exercise a
positive configured start frame as well as the exact boundary.

| Suite | Evidence supplied |
|---|---|
| `BundledAssetsRegression` | Synthetic bundle upgrade, corruption repair, interrupted extraction, ownership preservation, traversal and case collision rejection |
| `CustomSuitModelRegression` | Synthetic actor parsing, hierarchy and face bounds, contiguous native texture records, padded 4-bit/8-bit rows, and rejected malformed data |
| `DiscInstallRegression` | Synthetic disc identity and sector layout checks, plus the actual installer worker recovering from disc-output and bundled-output failures for both games; retry availability, installed bytes, cleared errors and corrected invalid selections are checked without opening the installer window |
| `InputConfigRegression` | Actual settings loading and input consumers: null recovery, correct defaults for partial player-two profiles, preserved custom/unbound controls, unchanged file contents on load, and save/reload |
| `InputManagerRegression` | Actual host polling with unavailable devices and a keyboard-state double: stale analog release, digital input preservation, script merging and replay precedence/end |
| `InteriorHookRegression` | Generated C# execution of a synthetic MIPS local-call fixture with and without an interior hook |
| `MemoryDiagnosticsRegression` | Lazy diagnostic allocations, heatmap disable/re-enable, guest read/write aliases, partial freezes, bulk writes, preserved projection-depth invalidation, and frame-gated corruption watches |
| `NativeLibraryRegression` | On Linux, initialize the actual native-library resolver and load packaged GLFW to query its version without opening a display; Linux-specific behavior is skipped on other operating systems |
| `Sm1AnimationRegression` | Actual post-loader hook exercised with synthetic native actor and animation-bank layouts, including the legacy extra-terminal-mesh overflow case |
| `Sm2LevelSwitchRegression` | Actual target-level descriptor hook with synthetic bounded storage: stage-loader caller guards, native name selection, register preservation, subsequent transitions, Retry, and archive fallback |
| `VideoSetupRegression` | Video option bounds, cycling, Apply, cancel, and persisted state conversion |
| `VramTrackerRegression` | Actual GP0 fill, VRAM copy and image-load commands with no renderer: the texture-cache generation and GPU-dirty state follow right, bottom and corner wrapping, an exact-edge fill does not wrap, a full-width copy at nonzero X covers every column, and unrelated blocks keep their generation |
| `GamePortConfigurationRegression` | MSBuild evaluation of both actual game projects: host defaults, explicit cross-target RIDs, Windows-only native payload selection, separate publish directories, and an explicit output override; no restore or compilation |
| `Sm1ActorAnimationLayoutConverterRegression` | Synthetic actor headers exercise the converter's native body-part count invariant before an incompatible model can be emitted |

Run just the configuration checks without a shared build using
`python tools/RecompOne/tests/test_game_port_configuration.py` (optional
`--dotnet /path/to/dotnet`). They exercise the actual SDK host and simulated
Linux/Windows SDK RIDs. Simulation checks project settings, not execution of a
different operating system.

The default checks do not open a window, load a retail executable, or complete a level.
Passing on Linux demonstrates that the shared tools and these tests work there;
it does not verify a native Linux game port.

## Native renderer and controller checks

With a working display, OpenGL 4.5 and SDL2, add `--include-native` to run two
additional asset-free suites. On headless Linux with Mesa and Xvfb:

```sh
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe xvfb-run -a python tools/run_regressions.py --include-native
```

| Suite | Evidence supplied |
|---|---|
| `GpuTransferRegression` | Actual GP0 commands and GL3.3/GL4.5 backend classes at 1x/4x scale: upload/readback and fills compared with CPU VRAM, alignment/empty-size controls, one-axis/two-axis wrapping, full extents, dirty display synchronization and full display clears including widescreen margins |
| `InputManagerSdlRegression` | Runs `InputManagerRegression --sdl-virtual`: the 13 ordinary input checks plus six checks that attach an SDL virtual controller, supply real axis/button state, process removal and verify release |

Select either with `--suite NAME`. The graphics suite creates hidden windows;
the SDL suite creates a virtual device without requiring a physical controller.
Missing required native support fails the requested suite rather than counting
it as passed. The default input suite explicitly reports that its optional SDL
part was not requested.

Some drivers return a newer context than the requested minimum. Linux CI also
verifies the GL3.3 backend with Mesa limited to an actual 3.3 context, after the
normal native gate has built the test executable:

```sh
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330 xvfb-run -a dotnet tools/RecompOne/tests/GpuTransferRegression/bin/Release/net10.0/GpuTransferRegression.dll gl33
```

This separate CI step retains `GpuTransferGl33Context.log` beside the normal
runner results. A failure in either step fails the workflow. The GL3.3 checks
overlap the normal graphics suite; they verify the lower context capability,
not additional independent scenarios. Native checks do not establish gameplay,
physical-controller behavior or performance on a hardware GPU.

## Checks requiring private inputs

`RenderingRegression`, `Sm2RenderingRegression`, `Sm1FramePacketRegression`,
`SuitModRegression`, and `Sm2SuitModRegression` reference a complete game project.
The game projects require the generated C# under each game's `generated/`
directory, which is deliberately excluded from Git. The suit regressions also
read extracted retail menu/character data. These checks cannot run from source
alone and are intentionally absent from the asset-free gate.

Follow the repository's build-from-source instructions with your own supported
USA disc to regenerate the game, then run the appropriate project. Retail discs,
extracted files, save files, and diagnostic RAM snapshots remain private.

## Gameplay verification

Full gameplay remains a separate gate: build and launch each actual game,
capture level entry and progression, then verify objectives, level transitions,
bosses, death/retry, save/load, and completion. Existing screenshot scripts and
historical level-boot reports are useful evidence for their stated routes only.
They do not prove a campaign playthrough or current release stability.
