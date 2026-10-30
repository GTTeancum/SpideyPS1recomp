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
GitHub Actions runs the same command on Windows
and Linux and retains the logs as workflow artifacts.

Use `--dotnet /path/to/dotnet` for a local SDK, or repeat `--suite NAME` to select
individual suites. Every invocation still builds the shared tools.
CI sets `SPIDEY_GUARD_AFTER_FRAME=2000` so corruption-watch checks exercise a
positive configured start frame as well as the exact boundary.

| Suite | Evidence supplied |
|---|---|
| `BundledAssetsRegression` | Synthetic bundle upgrade, corruption repair, interrupted extraction, ownership preservation, traversal and case collision rejection |
| `CustomSuitModelRegression` | Synthetic actor parsing, hierarchy and face bounds, contiguous native texture records, padded 4-bit/8-bit rows, and rejected malformed data |
| `DiscInstallRegression` | Synthetic disc identity validation and sector layout reading; no retail disc supplied |
| `InteriorHookRegression` | Generated C# execution of a synthetic MIPS local-call fixture with and without an interior hook |
| `MemoryDiagnosticsRegression` | Lazy diagnostic allocations, heatmap disable/re-enable, guest read/write aliases, partial freezes, bulk writes, preserved projection-depth invalidation, and frame-gated corruption watches |
| `NativeLibraryRegression` | On Linux, initialize the actual native-library resolver and load packaged GLFW to query its version without opening a display; Linux-specific behavior is skipped on other operating systems |
| `Sm1AnimationRegression` | Actual post-loader hook exercised with synthetic native actor and animation-bank layouts, including the legacy extra-terminal-mesh overflow case |
| `Sm2LevelSwitchRegression` | Actual target-level descriptor hook with synthetic bounded storage: stage-loader caller guards, native name selection, register preservation, subsequent transitions, Retry, and archive fallback |
| `VideoSetupRegression` | Video option bounds, cycling, Apply, cancel, and persisted state conversion |
| `GamePortConfigurationRegression` | MSBuild evaluation of both actual game projects: host defaults, explicit cross-target RIDs, Windows-only native payload selection, separate publish directories, and an explicit output override; no restore or compilation |
| `Sm1ActorAnimationLayoutConverterRegression` | Synthetic actor headers exercise the converter's native body-part count invariant before an incompatible model can be emitted |

Run just the configuration checks without a shared build using
`python tools/RecompOne/tests/test_game_port_configuration.py` (optional
`--dotnet /path/to/dotnet`). They exercise the actual SDK host and simulated
Linux/Windows SDK RIDs. Simulation checks project settings, not execution of a
different operating system.

These checks do not open a window, load a retail executable, or complete a level.
Passing on Linux demonstrates that the shared tools and these tests work there;
it does not verify a native Linux game port.

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
