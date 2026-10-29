# Issue #5: sewer-entry animation buffer corruption

## Evidence and cause

The [issue #5 crash log](https://github.com/GTTeancum/SpideyPS1recomp/issues/5#issuecomment-5682904515)
loads `L5A3`, then spawns two `lizman` actors at log frame 4280. At frame 4284,
`func_8007311C` calls `func_800804D8` with the unmapped address `0x0D6F0201`.
The same register dump contains `FP=0xF2820894`.

The bundled `lizman.psx` and `lizman2.psx` each have 19 skeleton objects and 56
meshes. Older conversion output marks 37 of those meshes as terminal parts.
The native parser, `func_80074C98`, counts the terminal markers into the model
resource's 16-bit field at offset 52. Their raw animation banks contain 19-part
samples; all 40 clips independently agree with that layout.

`func_800742A8` passes the cached count to `func_80074310`, which writes one
24-byte transform per part into static storage beginning at `0x800A58E4`.
The collision grid begins at `0x800A5BB4`, only 720 bytes later. The erroneous
count 37 therefore writes 888 bytes, exceeding this storage by 168 bytes.

The first clip begins at animation-bank offset `0x144`. Its bytes at offsets
`0x41C` and `0x420` are `0xF2820894` and `0x0D6F0201`; the overflow places them at
collision-grid offsets 8 and 12. Executing the released native counting and
animation routines in isolation reproduced both exact reported values. This
identifies an actual writer of the corrupted data, beyond a matching byte search.

## Bounded correction

`Sm1AnimationBounds.AfterModelLoad` runs after `func_80068BB0`, the complete
model loader. That routine returns the model-resource entry in `V0`, after both
the terminal count and animation pointer have been installed. The hook changes
only resource offset 52 and preserves all guest registers and projection tags.

Correction requires all of the following:

- An exact live allocation owned by the expanded-RAM override loader.
- The native v4 header, encoded as two 16-bit fields `4, 2`, and 1–30 objects.
- A cached part count greater than the object count.
- A tag `0x2A` animation bank reached through that model's metadata chain, with
  every read bounded by the tracked allocation and chunk length.
- Every clip, including the final clip, proving the same part count:
  `clipBytes = ceil(frames / (flags + 1)) * objectCount * 24`.

Already-correct counts return early. Compressed, padded, malformed, or ambiguous
banks retain their native behavior. The correction runs once during loading,
without adding an animation-frame scan. The existing ZIP assets are preserved;
a rebuilt executable repairs their loaded count. The converter separately
rejects future animated output whose terminal-marker count differs from its
skeleton object count.

## Asset-free regression

Run from the repository root with .NET 10:

```sh
dotnet run --project tools/RecompOne/tests/Sm1AnimationRegression -c Release
```

The regression calls the actual hook on synthetic post-loader resource records
and raw banks. It covers the packed v4 header, non-power-of-two sample intervals,
the exact 30-part storage boundary, unknown layouts, malformed pointers and
lengths, allocation ownership and release, and preservation of guest state.
A checked `IMemory` implementation rejects every read outside the model/resource
and every write except the repaired 16-bit count. No game assets or graphics
context are required. All 33 checks passed on 2026-10-29. The test is part of the
shared asset-free gate.

## Optional released-code reproduction

This probe is separate from CI because it uses a public release assembly and
bundled model bytes. Neither extracted DLLs nor decompiled game code belong in
the repository. The probe uses only three released functions; it does not start
the game or need retail disc data.

1. Download and unpack
   [Spider-Man v1.1](https://github.com/GTTeancum/SpideyPS1recomp/releases/tag/sm1-v1.1)
   (`Spider-Man-1.1-Windows-x64.zip`).
2. Install ILSpy CLI 11.2.0.10050 into a temporary tools directory and extract the
   managed assemblies from the single-file executable:

```sh
dotnet tool install ilspycmd --version 11.2.0.10050 --tool-path /tmp/spidey-ilspy
/tmp/spidey-ilspy/ilspycmd -d -o /tmp/spidey-release-assemblies /path/to/SpiderMan.exe
```

3. Run the probe from the repository root, substituting the extracted DLL path:

```sh
DOTNET_ReadyToRun=0 dotnet run \
  --project spiderman/tools/animation_release_probe -c Release \
  -p:ReleasedSpiderManAssembly=/tmp/spidey-release-assemblies/SpiderMan.dll \
  -- spiderman/port/bundled/runtime-assets.zip lizman2.psx

DOTNET_ReadyToRun=0 dotnet run \
  --project spiderman/tools/animation_release_probe -c Release --no-build \
  -- spiderman/port/bundled/runtime-assets.zip lizman.psx
```

On Windows, set `DOTNET_ReadyToRun=0` using the shell's environment syntax before
running the equivalent commands. Disabling ReadyToRun allows the Windows release's
managed IL to execute in the Linux test host as well.

The probe preserves the actual model header, object records, metadata, and
animation bank, but supplies zero-polygon meshes carrying the original terminal
markers to isolate counting from texture initialization. It executes
`func_80074C98_Impl`, then the native `func_800742A8`/`func_80074310` decoder. It
supplies the documented post-loader `V0` contract to the actual repair hook and
runs the released decoder again. Both `lizman.psx` and `lizman2.psx` produced this
result on 2026-10-29, using .NET SDK 10.0.403 on Linux:

```text
Released func_80074C98_Impl computed animation count: 37
Before hook: count=37, bytes=888, grid+8=0xF2820894, grid+12=0x0D6F0201
After actual AfterModelLoad hook: count=19, bytes=456, grid+8=0xA5A5A5A5, grid+12=0xA5A5A5A5
```

SHA-256 values of the inspected inputs:

| Input | SHA-256 |
| --- | --- |
| v1.1 Windows ZIP | `a7ed8752b40c0e36a88cca64162e71939344239f6bc7e4be6d3136f3f3fa96b7` |
| `SpiderMan.exe` | `eb3ed7a9594c3f78f38163d655f5c7a2aa5f8385b07d72f9cebe1e8f6a0cd4c2` |
| extracted `SpiderMan.dll` | `3e0a779f9ad0c8d17d2c3ab25ee77f49f80ab6b1716677aeafc6343373f1a69b` |
| bundled `lizman.psx` | `dbdbefa4a50790cb0a30e0dfffd49cfdc5e730f67909581ac29e4396be573f0b` |
| bundled `lizman2.psx` | `85f05f3d1121d84252d66270a75306c6b4d0ba69a22f23aa008e963398268b76` |

## Verification boundary and next gameplay check

The isolated native reproduction establishes the count-driven overwrite and its
exact correspondence to the issue log. The synthetic suite checks the runtime
hook's safety and selection rules. Neither proves a complete `LoadPsx` session,
L5A3 completion, visual animation quality, or full gameplay compatibility.
Those require a rebuilt game with user-supplied retail inputs and real gameplay.

The next check is to enter L5A3 normally, verify a single load-time
`[dc-model] corrected animation parts ... 37 -> 19` report for `lizman2`, then play
through the two-lizard spawn and continue through the sewer level. Keep issue #5
open until that gameplay result is recorded.

For additional tracing, `SPIDEY_GUARD_AFTER_FRAME` is measured in host presents,
whereas the log's `f` field counts vblanks. At the default two vblanks per present,
log frame 4284 is approximately present 2142; a late-write guard cutoff of 2000
presents targets this window. The separate `MemGuard.HitValue` correction makes
value watches honor that cutoff too.
