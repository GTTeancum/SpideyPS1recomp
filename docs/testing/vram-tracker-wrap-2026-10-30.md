# Wrapped VRAM writes and texture-cache invalidation

Date: 2026-10-30 (America/Indianapolis).

**Outcome:** the source finding recorded on 2026-10-29 was reproduced with a
short CPU-only check through the actual GP0 command path, repaired in the shared
runtime, and re-verified with the identical compiled test program. Both games
use this path for texture replacement and the texture page cache.

| Verification | Original runtime | Repaired runtime |
|---|---:|---:|
| New `VramTrackerRegression`, first build (10 cases) | 46/60; exit 1 | 60/60; exit 0 |
| Final `VramTrackerRegression` (adds exact-edge and full-width cases) | 48/64; exit 1 | 64/64; exit 0 |
| Full local gate (Windows 11, .NET SDK 10.0.302) | Not rerun as a before baseline | Shared build plus all 16 suites passed |
| `GpuTransferRegression` on a real Windows GPU (AMD Radeon 780M, GL 3.3 and 4.5 core) | Not rerun as a before baseline | 128/128; exit 0 |

Each pair ran one compiled test assembly (final SHA-256
`7021ce5802f5bac126a2d145907b8905e65acd27f547896626b9c01ce8ff5791`) against the
original runtime (`76980be7…150c7`) and the repaired runtime
(`fc99d625…99268`); only the runtime DLL changed. Logs are under the ignored
`artifacts/regressions/vram-tracker/`. No retail data, window or game was used
by the tracker suite.

## The defect

GP0 fills, VRAM-to-VRAM copies and CPU image loads wrap their destination at
1024x512: a write that runs off the right or bottom edge continues at the left
or top. `Gpu` already writes the wrapped pixels correctly. `VramTracker`, which
records a generation and a GPU-dirty flag for every 16x16 block, clamped the
original rectangle to the edge instead. The wrapped blocks therefore kept their
old generation and any GPU-dirty flag.

`TextureResolver` keys its replacement-texture memo and its texture-page cache
on those generations and skips replacement while a block is GPU-dirty. After a
wrapped write, a cached replacement could stay bound over changed pixels, and a
block the CPU had overwritten could stay excluded from replacement.

## Executed reproduction

`tools/RecompOne/tests/VramTrackerRegression` creates a `Gpu` with HLE
inactive, marks the probed and unwrapped destinations as GPU-written, sends one
real GP0 command, and asks `VramTracker` the questions `TextureResolver` asks.
Every case checks that the command changed the probed VRAM pixel, that both the
unwrapped and probed destinations received a new generation and lost the
GPU-dirty mark, and that an unrelated block kept its generation.

Cases: in-bounds fill, copy and upload controls; right, bottom and corner fill
wraps; right and bottom copy wraps; right and corner upload wraps. Two further
checks confirm that a fill ending exactly at the right/bottom edge does not
reach the left/top, and that a full-width (width 0 = 1024) copy at X 17 clears
every column.

On the original runtime, all controls passed and the pixel checks passed; the
14 failures were exactly the generation and GPU-dirty checks for the seven
wrapped destinations. The final build adds the two full-width failures (the
clamped rectangle never reached columns 0–16).

## Repair

`VramTracker.MarkCpuWrite`, `MarkGpuWrite`, `Generation` and `IsGpuDirty` now
split each axis into at most two inclusive block spans modulo 1024/512, never
visiting a block twice. In-bounds rectangles produce the same single span as
before, so their generation hash and dirty answers are unchanged. Upload records
(`NoteUpload`/`TryFindUpload`) keep the clamped bounds: a wrapped rectangle can
never satisfy `Upload.Contains`, so recording it in wrapped blocks would only
evict usable records.

Renderer callers pass rectangles inside VRAM (draw clip and display targets),
so their marking is unchanged.

## Limits

- This is a synthetic tracker check, not a rendered frame. It does not
  establish the cause of issue #20 (rainbow bands), which still needs its
  screenshot and reproduction details.
- No game was booted and no gameplay was performed.
- The SDL virtual-controller step of `--include-native` failed on Windows
  before reaching its checks: `BundledNativeRuntime.Initialize` could not load
  `vcruntime140.dll` beside the test assembly. That suite has only run on
  Linux CI; the failure is independent of this change and was not repaired.
