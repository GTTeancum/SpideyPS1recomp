# SM1 scene-depth regression

Run from the repository with temporary and .NET cache paths directed into its
ignored `.local` directory:

```powershell
dotnet run --project tools/RecompOne/tests/SceneDepthRegression -c Release -- <absolute-repository-path>
```

Requires the privately extracted `spiderman/extracted/SLUS_008.75`. It calls the
four original actor transform routines and checks that near/far precision modes
preserve their original depth scales and agree on screen position.

The CPU clip checks carry fractional source vertices and native camera coordinates
through intersections, check the corresponding UV weights, and verify every
triangle and quad grid vertex in 4:3 and widescreen.

Both the SM1 suit-only depth path and the scene-depth path used by SM2 are exercised.
The rendering checks use an invisible OpenGL context, no audio, and no host input.
They read back native draw order in both submission orders, HUD, blending and
texture cut-outs. A receding textured triangle checks perspective interpolation.
Replacement suit faces check self-occlusion across an intervening scenery draw.
The same commands are replayed with and without shadow receiver depth. See the
bank and rooftop captures in [the rendering notes](../../../../docs/bank-rendering.md).

Do not run a restoring test build concurrently with `dotnet publish` of SM1:
both use the port project's restore metadata, with different runtime targets.
