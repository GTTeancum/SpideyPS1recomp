# Maintainer handoff · November 24, 2026

Resume on `master`. The latest game code is in [55c84f5](https://github.com/GTTeancum/SpideyPS1recomp/commit/55c84f5ffaa1b9512ce08d9a0f2cdf04ccaf49f6). The November fixes are available in source and in the two staged Windows installs. The latest downloadable game packages are [SM1 1.1](https://github.com/GTTeancum/SpideyPS1recomp/releases/tag/sm1-v1.1) and [SM2 1.1](https://github.com/GTTeancum/SpideyPS1recomp/releases/tag/sm2-v1.1). Both packages predate those fixes.

## Pick up on the existing PC

Open `C:\Programming\GitHub\SpideyPS1recomp` as the workspace. Launch each game directly from its executable for player testing. These SHA-256 hashes identify the exact binaries tested.

| Game | Executable from the repository root | SHA-256 |
| --- | --- | --- |
| SM1 | `Spider-Man/SpiderMan.exe` | `fd8a9db9636eb2a9d6e238ce68455e03c47fa03b1ac0391d7538d06f0a0af851` |
| SM2 | `Spider-Man 2/SpiderMan2.exe` | `47862ebc891e668916f16be48e4bd1995ba4af481f47851fdd246edf8d3e1308` |

**All three SM2 mask revisions are staged.** Earth-62412, Spidey New Design, and UK each received the committed actor, diffuse texture, and conversion report. The staging audit matched **452/452 SM2** and **439/439 SM1** suit files to source. The active catalogues contain 84 external suits in SM2 and 83 in SM1, including the same 70 reviewed SMU suits in both. This was a file-hash verification. No new costume-viewer run accompanied the copy. The machine-local audit is `.local/reviewed-suits/20261124T165630193933Z/report.json`. The [restaging tool](../tools/restage_reviewed_suits.py) copies reviewed models and preserves the intended Spider-Ham proportions.

**Keep these files if moving machines.** GitHub has the source, reviewed suits, and the evidence linked below. The staged installs and their contents live on this PC. Retain each complete game folder to carry over retail data, mods, settings, and saves. SM1 uses both `saves/carda.sav` and `saves/carda.sav.continue.json` to preserve progress and the most recently saved slot. SM2 has its own `saves/carda.sav`. The original disc archives, private N64 reference ROM, and `.local/` evidence and scripts also live in the checkout. On this machine, `.local/environment.ps1` sets repository-local tool caches and temporary paths. Keep all project work under the repository.

## What was verified

| Area | Evidence and scope |
| --- | --- |
| SM1 Doc Ock and Venom chases | All 19 rail tiles joined in 88 widescreen Doc Ock frames, 87 in 4:3, and 62 widescreen Venom frames. The previous gap at 853×480 was about 80 pixels. The renderer now identifies the entire meter from each level's own draw routine. [Before and after](screenshots/monster-ock-chase-bar.png) |
| Shared GPU coordinates | All six coordinate regression cases pass in both games. Large solid geometry and small translucent particles crossing the coordinate boundary have separate checks. Oversized translucent effects still hit the original span limit. Both regression suites were rerun on November 24. SM1 passed 97 checks and SM2 passed 78, with zero failures. [Shared test source](../tools/RecompOne/tests/RenderingRegression/Program.cs) |
| SM1 bank and SM2 opening | The player confirmed the SM1 bank and SM2 opening camera sweep after the rendering repairs. The packaged SM2 executable also ran the opening street fight under hidden, muted capture. [Bank investigation](bank-rendering.md) · [Street-fight frames](screenshots/sm2-street-proof.jpg) |
| Other November changes | The player confirmed right-stick rotation and loading a level after changing costume. Further notes cover [save selection](continue-save-repair.md), [camera orbit](right-stick-orbit.md), [N64 shadow behavior in both games](n64-shadows.md), and [menu and costume repairs](menu-orbit-bigtime.md). |

## Rebuild and retest

1. On a fresh checkout, use the default `master` branch with full Git history. The suit audit reads prior repair commits. Development requires .NET 10, Python 3 with NumPy and Pillow, and the corresponding USA retail disc. SM1 uses `SLUS-00875`. SM2 uses `SLUS-01378`, Rev 1. Follow the per-game extraction and code-generation instructions before running game-specific tests. [SM1 build guide](../spiderman/README.md#building) · [SM2 build guide](../spiderman2/README.md#building)
2. With generated sources present, run the commands below from the repository root. On the existing PC, dot-source `.\.local\environment.ps1` first. The publish commands place new Windows binaries under each game's `port/dist/` directory. Use the matching game's generated project and keep stage-specific settings and saves when updating a playable install.
3. For a prepared pair of staged installs, `python tools/restage_reviewed_suits.py` audits without installing. Add `--apply` to copy the reviewed catalogue. To repeat visual tests, use a hidden, muted process with private settings, copies of saves, and the game's native capture facility. Check the actual images. The local chase runs, measurements, and capture scripts are in `.local/ock-hud/`.

```powershell
dotnet run --project tools/RecompOne/tests/RenderingRegression -c Release
dotnet run --project tools/RecompOne/tests/Sm2RenderingRegression -c Release
dotnet publish spiderman/port/SpiderMan.csproj -c Release -r win-x64
dotnet publish spiderman2/port/SpiderMan2.csproj -c Release -r win-x64
```

## Remaining coverage

GitHub showed zero open issues and zero open pull requests on November 24. The playtests above cover specific routes. Full campaign completion and long-session stability remain unverified. The SM2 notes also record earlier model-initialization failures in `e3m1`, `e3m2`, `e4m1`, and `e5m2`, plus outstanding SM2 memory-card coverage. These were outside the November 24 test runs. [Project TODO](../TODO.md) · [SM1 coverage](../spiderman/TO_DO.md) · [SM2 coverage](../spiderman2/TO_DO.md)
