# OpenSpidey — original Xbox port (nxdk)

Both games, Spider-Man and Spider-Man 2: Enter Electro, as one Xbox compilation
([issue #22](https://github.com/GTTeancum/SpideyPS1recomp/issues/22)). Status: title screen.
Spider-Man 1's recompiled code boots in XEMU and draws its title screen ("PRESS
START") through the Xbox runtime and a software PlayStation GPU (2026-11-03).

## Rules

- **Toolchain: nxdk only.** Clang/LLD, nxdk libraries, `cxbe` and `extract-xiso`.
  No Microsoft Xbox SDK, XDK or leaked Microsoft source is used or referenced.
- **One-way dependency.** The Xbox port may use shared code from the rest of the
  repository. Nothing outside `xbox/` may refer to this folder: the Windows and
  Linux builds never compile, include, package or run Xbox code.
  `tools/check_xbox_isolation.py` enforces this and is part of
  `tools/run_regressions.py` (`XboxIsolationRegression`).
- **Shared first, bespoke only where needed.** Game data handling, recompiler
  configuration and patch definitions are shared. This folder holds only what the
  Xbox needs that the PC does not: the entry point, the C runtime the PC does not
  use, and the Xbox graphics, audio, input and storage adapters.
- **Gameplay fixes go in both places.** The PC runs the game on the C# runtime with
  C# patches; the Xbox runs it in C. A gameplay fix meant for all platforms must be
  made in both, and checked on each.
- **No 60 FPS** on Xbox (the game's own 30 updates/s). Video modes: 4:3 480i,
  16:9 480p, 16:9 720p. Other quality-of-life changes and the extra suits stay.

## Layout

| Path | Contents |
| --- | --- |
| `src/main.c` | entry point: video mode, game thread (4 MB stack), status screen |
| `runtime/recomp.h` | the interface the generated code is written against |
| `runtime/runtime.h` | runtime services: log and status, overlays, BIOS, boot |
| `runtime/*.c` | RAM, hardware registers and DMA, dispatch, BIOS, boot, clock and interrupts, CD library, software GPU and graphics library, GTE registers, status |
| `src/patches_sm1.c` | Spider-Man 1 patches ported from `spiderman/patches` (keep in step) |
| `tools/make_disc_map.py` | the test disc's sector map from an install's `recompone-disc.json` |
| `generated/<game>/` | RecompOne's C output for each game, ignored by git (made from the player's disc) |
| `tools/gen_hook_defaults.py` | default bodies for hooks not implemented yet (`build/hooks_default.c`) |
| `tools/compile_generated.sh` | compiles one game's generated C with nxdk (no link) |
| `tools/build.sh` | nxdk build wrapper (MSYS2 bash, installed LLVM) |
| `tools/run_xemu.py` | hidden, silent XEMU boot with native screenshots and the status text |
| `tools/xemu_native_screenshot.py` | triggers XEMU's own screenshot (from the OpenJKDF2ogx tools) |
| `build/` | build output, ignored by git (`release/default.xbe`, `OpenSpidey.map`) |

## Build

With nxdk at `C:\nxdk`, LLVM at `C:\Program Files\LLVM` and MSYS2 at `C:\msys64`.
From `spiderman/`, generate the game code (see
[docs/recompiler-c-output.md](../docs/recompiler-c-output.md)):

```bash
dotnet ../tools/RecompOne/RecompOne.Recompiler/bin/Release/net10.0/recompone.dll config/spiderman.json --c ../xbox/generated/spiderman
```

Then from the repository root:

```bash
python -I xbox/tools/gen_hook_defaults.py spiderman
MSYSTEM=MINGW64 C:/msys64/usr/bin/bash.exe --noprofile --norc xbox/tools/build.sh -j8
```

`compile_generated.sh spiderman` compiles the generated code alone, as a quick check.

## Test in XEMU

```bash
G=private/clean-1.1/Spider-Man/game
python -I xbox/tools/make_disc_map.py $G private/work/xbox-disc/disc.map
python -I xbox/tools/run_xemu.py --shots 20,40 --include $G/SLUS_008.75 --include $G/SYSTEM.CNF --include $G/CD.HED --include $G/CD.WAD --include private/work/xbox-disc/disc.map
```

`--include` puts the player's extracted game files and the sector map on the test disc. The runtime
keeps its full status in `g_status_text`; at every screenshot the script copies it
out through the XEMU monitor (address from `build/OpenSpidey.map`) to
`captures/status-<second>.txt`: progress, recent log lines and every missing
service (hook, BIOS call or hardware register) in first-use order with its count.
The screen shows a summary of the same.

Uses `private/work/xemu/xemu.exe` (a copy of the local XEMU) and reads the boot
ROM, BIOS, EEPROM and HDD image from `C:\Games\Emulators\Xemu` without changing
them: the EEPROM is copied and HDD writes are discarded (`-snapshot`). XEMU runs
with its window hidden and audio written to a file. The disc image is written to
`C:\Games\Emulators\Xemu\OpenSpidey.iso` and replaced on every run (one image
only): XEMU rejects disc images on a network share (`bdrv_open_driver:
request_alignment` assertion). Settings, the EEPROM copy, logs and captures go to
`private/work/xbox-run`.

## Plan

1. **Bring-up** — done 2026-11-03: nxdk build, XBE, disc image, hidden XEMU boot,
   native captures, isolation check.
2. **C output from RecompOne** — done 2026-11-03: `--c <dir>` writes C from the same
   analysis and configuration; the C# output is byte-for-byte unchanged. Both games
   generate (SM1 3,314 functions in 32 files, SM2 3,262 in 30) and compile with
   nxdk at -O2 -Wall with no errors or warnings.
3. **C runtime** — 2026-11-03. RAM, scratchpad, hardware registers (plain storage
   as on the PC) with GPU ports and DMA channels 2 and 6, overlay dispatch with the
   PC's region rules, BIOS memory/string/heap/event/setjmp calls and interrupt
   registration, a 60 Hz clock delivering vblank and DMA interrupts to the game's
   handlers, the idle-loop breaker, VSync and the game's counter wait, the CD
   library over the disc's sector map, a software GPU (port of the PC's reference
   rasteriser) with the graphics library, and the SM1 patches the title needs
   (GPU reset/display mask, fixed-address overlays, movie skip). Result: the title
   screen. Next: the GTE (first commands appear after the title), MDEC (DMA 0),
   pads, sound, movies, the Venom-chase pacing and the other SM1 hooks.
4. **Xbox adapters** — drawing through pbkit/NV2A (`DrawOTag`, `PutDispEnv`,
   `PutDrawEnv`), sound through AC97, controllers.
5. **Patches** — the 114 SM1 hooks (all defaults now) and the suit loader in C.
6. **Video modes and the compilation launcher**, then Spider-Man 2.

Game data is never part of the repository or a published image; the player's own
discs supply it.
