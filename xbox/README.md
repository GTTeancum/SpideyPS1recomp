# OpenSpidey — original Xbox port (nxdk)

Both games, Spider-Man and Spider-Man 2: Enter Electro, as one Xbox compilation
([issue #22](https://github.com/GTTeancum/SpideyPS1recomp/issues/22)). Status: bring-up.
The toolchain, disc image and XEMU test loop work; no game code runs yet.

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
| `src/` | Xbox C sources (currently the bring-up entry point) |
| `tools/build.sh` | nxdk build wrapper (MSYS2 bash, installed LLVM) |
| `tools/run_xemu.py` | hidden, silent XEMU boot with native screenshots |
| `tools/xemu_native_screenshot.py` | triggers XEMU's own screenshot (from the OpenJKDF2ogx tools) |
| `build/` | build output, ignored by git (`build/release/default.xbe`) |

## Build

From the repository root (PowerShell or Git Bash), with nxdk at `C:\nxdk`, LLVM at
`C:\Program Files\LLVM` and MSYS2 at `C:\msys64`:

```bash
MSYSTEM=MINGW64 C:/msys64/usr/bin/bash.exe --noprofile --norc xbox/tools/build.sh -j8
```

## Test in XEMU

```bash
python -I xbox/tools/run_xemu.py --shots 10,15
```

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
2. **C output from RecompOne** — a C backend in the shared recompiler, beside the
   current C# output, for both games' main code and overlays. Generic tool work,
   kept with the shared tools.
3. **C runtime** (here) — CPU context, RAM and scratchpad, overlay dispatch,
   vblank/interrupt timing, and the PlayStation library calls the games use
   (graphics, CD, sound, pad).
4. **Xbox adapters** (here) — drawing through pbkit/NV2A, sound through AC97,
   controllers, reading game data from the disc.
5. **Patches** — the gameplay and quality-of-life patches and the suit loader in C.
6. **Video modes and the compilation launcher**, then Spider-Man 2.

Game data is never part of the repository or a published image; the player's own
discs supply it.
