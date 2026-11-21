# N64 shadow regression

From the repository root in PowerShell:

```powershell
. .\.local\environment.ps1
python tools/trace_n64_shadows.py
dotnet run --project tools/RecompOne/tests/ShadowRegression -c Release -- .
```

The optional ROM replay requires the private USA `.z64` in the root and Unicorn
installed under `.local/n64-shadow/python`. Its output is independent of the C#
implementation. Compare `.local/n64-shadow/matrix-oracle.json` with the checked-in
numeric fixture. The regression itself uses the fixture and needs no ROM.

The GL tests create an invisible 32×32 context; no audio or host input is used.
They verify actual pixels for coplanar shadow coverage, wall occlusion, overlap,
depth-write suppression, HUD layering and stencil reset. Gameplay content still
requires inspection of the game's native captures; these tests alone are not
visual acceptance for a level.
