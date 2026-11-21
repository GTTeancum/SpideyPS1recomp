# SM1 scene-depth regression

Run from the repository with temporary and .NET cache paths directed into its
ignored `.local` directory:

```powershell
dotnet run --project tools/RecompOne/tests/SceneDepthRegression -c Release -- <absolute-repository-path>
```

Requires the privately extracted `spiderman/extracted/SLUS_008.75`. It calls the
four original actor transform routines and checks that near/far precision modes
produce comparable host depth without changing native SZ or screen position.

The rendering checks use an invisible OpenGL context, no audio, and no host input.
They verify opaque wall/actor visibility in both draw orders, actors in front,
HUD and transparency behavior, texture cut-outs, frame/replay depth clearing,
and a same-command-list depth-off/depth-on comparison. These controlled checks
supplement actual bank captures; they do not establish that a reported bank
pose has been reproduced.

Do not run a restoring test build concurrently with `dotnet publish` of SM1:
both use the port project's restore metadata, with different runtime targets.
