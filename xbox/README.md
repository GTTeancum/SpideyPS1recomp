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
| `src/movies_sm1.c` | SM1's movie hook: Dreamcast `.SFD` where installed, else the PS1 `.STR` |
| `runtime/vtag.c` | vertex tags in RAM: the GTE's precise positions and depths beside the words that carry them |
| `runtime/spu.c` | the sound chip: 24 voices, envelopes, CD audio input, mixed at 44.1 kHz each vblank (port of the PC's Spu.cs/XaAudio.cs) |
| `runtime/audio.c` | AC97 output at 48 kHz, shared by the sound chip and movies |
| `runtime/xa.c` | CD-XA ADPCM decoding (streamed music, PS1 movie sound) |
| `src/input.c` | the Xbox controller as PlayStation pad 1 (SDL2, the PC's default bindings) |
| `runtime/movie.c` | movie playback: AC97 sound, the movie clock, skip, the two players' loops |
| `runtime/str.c` | PS1 `.STR` decoder: MDEC video (bitstream v1/v2) and XA ADPCM sound |
| `runtime/sfd_core.cpp` | the PC's Dreamcast SFD decoder (`tools/movies/native`), compiled in |
| `tools/make_disc_map.py` | the test disc's sector map from an install's `recompone-disc.json` |
| `generated/<game>/` | RecompOne's C output for each game, ignored by git (made from the player's disc) |
| `tools/gen_hook_defaults.py` | default bodies for hooks not implemented yet (`build/hooks_default.c`) |
| `tools/gen_gte_ops.py` | the GTE command words the generated code issues (`runtime/gte_ops.h`) |
| `runtime/memfast.s` | word-at-a-time `memcpy`/`memset` in place of nxdk's byte loops |
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
python -I xbox/tools/patch_generated_sm1.py
python -I xbox/tools/gen_hook_defaults.py spiderman
python -I xbox/tools/gen_gte_ops.py
MSYSTEM=MINGW64 C:/msys64/usr/bin/bash.exe --noprofile --norc xbox/tools/build.sh -j8
```

`patch_generated_sm1.py` is the C counterpart of the PC's generated-source transform
`spiderman/tools/patch_texture_registry.py` (texture registry relocated, player models
register every texture), and makes the game's four vblank-countdown loops call the
clock (the PC catches them by counting memory reads); run it after every generation.
`gen_gte_ops.py` lists the GTE command words for the per-command GTE routines; a word
missing from the list still works, through the general routine.

The dependency files nxdk writes name Windows paths make does not match, so a change
to a header does not rebuild what includes it: delete the `.obj` files under `src`,
`runtime`, `generated` and `build` before building after one (or after switching
`RT_PROFILE`).

## Assets and settings

The disc carries the player's game files plus what the Windows-side preparation makes
from a PC install (the future installer's job):

```bash
python -I xbox/tools/prep_assets.py "<PC install>/assets/builtin" private/work/xbox-disc/assets
python -I xbox/tools/prep_suits.py "<PC install>/mods/suits" private/work/xbox-disc/mods
```

- `assets/builtin`: the Dreamcast character actors, served in place of the game's own
  (`src/assets_sm1.c`, a port of `LooseWadOverrides` and `Costume`).
- `assets/packs/dreamcast-sm1-actors`: their art as DXT1 DDS, matched by the PC pack's
  fingerprint of each uploaded texture (`runtime/texpack.c`).
- `mods/suits/<id>`: a suit's textures as DXT1 DDS capped at 512 (suit art is often
  1024-2048 and the Xbox has about 10 MB spare at 720p), and for suits with their own
  model the actor and its RTG2 rig split apart (`src/suits_sm1.c`, `src/retarget_sm1.c`;
  the rig maths is the PC's `tools/retarget/native/retarget_core.cpp`, compiled in).

`D:\openspidey.ini` holds `level=<lNaM>` (start a new game there, `src/level_sm1.c`),
`suit=<id>` and `dreamcast_movies=0` (PS1 movies only). Include `openspidey.ini`,
`mods` and `assets` with `--include`.

## Sound

The sound chip (`runtime/spu.c`) is a port of the PC's: its registers and DMA channel
4 reach it through `runtime/memory.c`, and each vblank mixes 735 frames (44.1 kHz) into
the AC97 output (`runtime/audio.c`, resampled to 48 kHz). Streamed music and dialogue
are CD-XA: a read in XA mode (`runtime/cd.c`) plays the disc's ADPCM sectors at drive
speed instead of delivering them, the selected file and channel decoded into the chip's
CD input (the title music is `COMPILED.XA` channel 13). Movies take the AC97 while they
play. No reverb, as on the PC. `COMPILED.XA` must be on the disc.

## Movies

The game's own STR player needs the MDEC and XA streaming, which the Xbox runtime does
not have, so the runtime plays movies itself from the hook at 8002AD0C (the PC's
`DreamcastMoviePatches`): the Dreamcast `.SFD` from `D:\mods\movies\dreamcast` when
present (the 23 the PC maps, copied as the PC's installer copies them), else the PS1
movie from the disc's `CINEMAS` folder (raw 2336-byte sectors, as the install holds
them). Sound is resampled to the AC97's 48 kHz and is the clock; Start or Cross skips
after the first second. A movie that cannot play is skipped, as before. Checked in
XEMU (2026-11-06) on the boot logos: PS1 and Dreamcast pictures, sound matching
ffmpeg's decode of the same files, skip, and the title screen after. Not yet on a
console; the Dreamcast decoder's 64-bit IDCT is the first thing to time there.

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

`--hd` selects XEMU's HD cable and marks the EEPROM copy for 480p, 720p and a
widescreen TV, so the build runs at 1280x720 in 16:9. `--registers` also saves the
CPU registers and stack at each shot (a halted CPU with interrupts off is an nxdk
assert or a crash). `--ram` saves the PlayStation's RAM.

## Test on a development console

`tools/xbdm.py` talks to a debug console's monitor (TCP 731) directly, no SDK:

```bash
python -I xbox/tools/xbdm.py 10.0.0.231 put private/work/xbox-install 'E:\OpenSpidey'
python -I xbox/tools/xbdm.py 10.0.0.231 run 'E:\OpenSpidey\default.xbe'
python -I xbox/tools/xbdm.py 10.0.0.231 status
python -I xbox/tools/xbdm.py 10.0.0.231 frame shot.png
```

`put` sends only files whose size changed (and always the XBE). `status` reads
`g_status_text` from console memory; `frame` pauses the console and copies the last
display buffer shown (the console's own screenshot cannot find a picture drawn
without D3D). The install folder holds the XBE, the player's game files, the sector
map, `openspidey.ini`, `assets`, `mods` and, for unattended runs, `input.txt`.

`tools/xbprofile.py HOST [SAMPLES]` samples where the game thread is: it suspends the
thread and takes the position from the processor's interrupt frame on its stack
(code addresses lower on a parked thread's stack are stale). Time a thread spends
asleep in the clock's wait can show under the last function interrupted before it.
The status text counts ticks dropped (the game more than four ticks behind) and the
longest gap between ticks. Reading console memory costs console time: poll a few
bytes a few times a second, not the whole status text.

## Video

The output follows the console's settings: 720p when allowed, else 480. Rendering
is on the NV2A (`runtime/hwgpu.c`): each PlayStation display buffer is a render
target at the output size; 4- and 8-bit textures are I8 images with the chip's
palette lookup. Each SXY the GTE produces keeps its exact, unrounded position and
camera depth as a vertex tag (`runtime/vtag.c`, the PC's GteScreen provenance), which
the generated code carries through registers and RAM into the packet words (only in
the functions that use the GTE: tag work in every load and store halved the frame rate
in level 3 and its code ran the console out of memory); a
polygon whose vertices all carry one is drawn at the precise positions, perspective
correct. Without them, whole-pixel corners at 720p left slivers between split faces
(far buildings), and a depth looked up from the screen position could be another
vertex's and bend the texture; untagged textured polygons now draw affine instead.
The SM1 subdivision and edge rules are `src/world_sm1.c` (the PC's WorldSubpixel).
Textured polygons take texels as the PC's shader does (floor where the texture runs
forward across the screen, ceil where it runs backward). Seams on buildings came from
corners that lost their exact position and fell back to whole pixels beside
neighbours drawn exactly. Tags are kept as the PC keeps them: a subdivision grid point
or nudged edge point is certified for its word (`VTag.native`, the PC's NativeScreen)
rather than judged by distance; a corner whose word no longer matches keeps its depth
and loses only its position (the rest of the polygon stays exact); world geometry is
"every corner tagged" (the PC's Wide.FromGte); and a coordinate written as two halves
(SH, the object list) takes the tag back once the word is whole (both runtimes'
StoreU16). The PC's after-drawing crack fill is ported (`crack_fill=1`, off by
default): with the above, the hole test shows it is not needed. `geometry_debug=1` draws world materials in flat colours (cracks
show as thin lines of another colour); `geometry_debug=2` starts each frame magenta
(undrawn pixels show). Widescreen (`src/wide_sm1.c`, a port of `spiderman/patches/Wide.cs` and
`WorldFrustum.cs`) is on for gameplay when the display is 16:9; menus stay 4:3
and are pillarboxed. A fix to the widescreen rules belongs in both copies.

nxdk's maths library has unimplemented functions that assert and halt the console
(`lround`, `lrint`, `cbrt`, `log2` and others in `lib/pdclib/platform/xbox/functions/math`);
do not call them.

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
   (GPU reset/display mask, fixed-address overlays, movie skip; movies now play, see
   Movies). Result: the title
   screen. Next: the GTE (first commands appear after the title), MDEC (DMA 0),
   pads, sound, movies, the Venom-chase pacing and the other SM1 hooks.
4. **Xbox adapters** — drawing through pbkit/NV2A (`DrawOTag`, `PutDispEnv`,
   `PutDrawEnv`), sound through AC97, controllers.
5. **Patches** — the 114 SM1 hooks (all defaults now) and the suit loader in C.
6. **Video modes and the compilation launcher**, then Spider-Man 2.

Game data is never part of the repository or a published image; the player's own
discs supply it.
