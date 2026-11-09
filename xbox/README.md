# OpenSpidey — original Xbox port (nxdk)

Both games, Spider-Man and Spider-Man 2: Enter Electro, as one Xbox compilation
([issue #22](https://github.com/GTTeancum/SpideyPS1recomp/issues/22)). Status: Spider-Man
plays on the console (levels, saving, movies, suits; see below). Spider-Man 2 reaches
its title screen and attract demo in XEMU (2026-11-09), as on the PC; its patches are
mostly still to port.

Each game is its own XBE, built from the same runtime (`make GAME=spiderman` or
`GAME=spiderman2`): either game's code is about 10 MB, more than the console has spare
for both, and the two games define hooks of the same names. They share the title ID
and so the memory cards in `E:\UDATA`, as one PlayStation's cards serve both games. A
small launcher XBE choosing between them is still to come.

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
| `src/main.c` | entry point: video mode, game thread (256 KB stack), status screen |
| `src/game.h`, `src/game_sm1.c`, `src/game_sm2.c` | the game an XBE runs: its generated program and its patches' set-up |
| `src/patches_common.c` | patches both games share (`GpuPatches`, `PadPatches`, the Bag-Man shell depth, the armour test) |
| `src/wide.c`, `src/world.c`, `src/shadow.c` | widescreen, precise world subdivision and the player's silhouette shadow, both games (each game gives its addresses) |
| `src/movies_sm2.c` | SM2's movies: the runtime's PS1 player called from inside the game's own (`patch_generated_sm2.py`) |
| `src/patches_sm2.c` | Spider-Man 2 patches ported from `spiderman2/patches` (keep in step) |
| `src/level_sm2.c` | SM2's direct level start (`level=e3m1`, the PC's `LevelSwitch.cs`) |
| `src/costume_sm2.c` | SM2's costumes and models: dedicated actors, Dreamcast assets, suit powers, frame packets, the costume viewer's mod rows |
| `src/retarget_sm2.c` | SM2's suit retargeting and swing web (the PC's `SuitRetargeting.cs`, `SuitWebAttachment.cs`) |
| `src/wad_overrides.c` | WAD entries served from the disc's prepared files into the expanded-RAM arena (the PC's `LooseWadOverrides`), both games |
| `src/suits.c` | mod suits: the catalogue, choosing one (textures, rig, the kept choice) and material binding (the PC's `SuitMods`), both games |
| `src/guest_layout.h` | the recomp's reserved blocks in the 8 MB guest RAM (arena, texture registry, costume viewer, scratch names) |
| `runtime/files.c` | file reads and writes through the kernel's calls (nxdk's `fopen` leaks a kernel object per call) |
| `src/rig.c` | the suit rig engine (the PC's `NativeRetargetRig` around its retarget core), both games |
| `tools/patch_generated_sm2.py` | SM2's generated-source transforms (vblank countdowns, pad-read loop, movies, costume viewer) |
| `runtime/recomp.h` | the interface the generated code is written against |
| `runtime/runtime.h` | runtime services: log and status, overlays, BIOS, boot |
| `runtime/*.c` | RAM, hardware registers and DMA, dispatch, BIOS, boot, clock and interrupts, CD library, software GPU and graphics library, GTE registers, status |
| `src/patches_sm1.c` | Spider-Man 1 patches ported from `spiderman/patches` (keep in step) |
| `src/movies_sm1.c` | SM1's movie hook: the PS1 `.STR` (the Dreamcast `.SFD` path is commented out for now) |
| `runtime/vtag.c` | vertex tags in RAM: the GTE's precise positions and depths beside the words that carry them |
| `runtime/spu.c` | the sound chip: 24 voices, envelopes, CD audio input, mixed at 44.1 kHz each vblank (port of the PC's Spu.cs/XaAudio.cs) |
| `runtime/audio.c` | AC97 output at 48 kHz, shared by the sound chip and movies |
| `runtime/xa.c` | CD-XA ADPCM decoding (streamed music, PS1 movie sound) |
| `src/input.c` | the Xbox controller as PlayStation pad 1 (SDL2, the PC's default bindings) |
| `runtime/card.c` | the PlayStation memory card on the hard disk (port of the PC's MemoryCard.cs and BIOS card calls) |
| `runtime/title.h` | the title ID and name: the one place they are defined |
| `tools/patch_xbe.py` | writes the title ID, name and dashboard pictures into the built XBE (run by `build.sh`) |
| `tools/make_xbox_images.py` | dashboard images and launcher thumbnail from the game icon (run by `build.sh`) |
| `runtime/movie.c` | movie playback: AC97 sound, the movie clock, skip, the two players' loops |
| `runtime/str.c` | PS1 `.STR` decoder: MDEC video (bitstream v1/v2) and XA ADPCM sound |
| `runtime/sfd_core.cpp` | the PC's Dreamcast SFD decoder (`tools/movies/native`); commented out for now (see Movies) |
| `tools/make_disc_map.py` | the test disc's sector map from an install's `recompone-disc.json` |
| `generated/<game>/` | RecompOne's C output for each game, ignored by git (made from the player's disc) |
| `tools/gen_hook_defaults.py` | default bodies for hooks not implemented yet (`build/hooks_default_<game>.c`) |
| `tools/gen_gte_ops.py` | the GTE command words the generated code issues (`runtime/gte_ops.h`) |
| `runtime/memfast.s` | word-at-a-time `memcpy`/`memset` in place of nxdk's byte loops |
| `tools/compile_generated.sh` | compiles one game's generated C with nxdk (no link) |
| `tools/build.sh` | nxdk build wrapper (MSYS2 bash, installed LLVM) |
| `tools/run_xemu.py` | hidden, silent XEMU boot with native screenshots and the status text |
| `tools/xemu_native_screenshot.py` | triggers XEMU's own screenshot (from the OpenJKDF2ogx tools) |
| `build/` | build output, ignored by git (`release/default.xbe`, `OpenSpidey.map`) |

## Build

With nxdk at `C:\nxdk`, LLVM at `C:\Program Files\LLVM` and MSYS2 at `C:\msys64`.
From `spiderman/` (and `spiderman2/`), generate the game code (see
[docs/recompiler-c-output.md](../docs/recompiler-c-output.md)):

```bash
dotnet ../tools/RecompOne/RecompOne.Recompiler/bin/Release/net10.0/recompone.dll config/spiderman.json --c ../xbox/generated/spiderman
dotnet ../tools/RecompOne/RecompOne.Recompiler/bin/Release/net10.0/recompone.dll config/spiderman2.json --c ../xbox/generated/spiderman2
```

Then from the repository root, for Spider-Man (`build/release/default.xbe`):

```bash
python -I xbox/tools/patch_generated_sm1.py
python -I xbox/tools/gen_hook_defaults.py spiderman
python -I xbox/tools/gen_gte_ops.py
MSYSTEM=MINGW64 C:/msys64/usr/bin/bash.exe --noprofile --norc xbox/tools/build.sh -j8
```

and for Spider-Man 2 (`build/sm2/default.xbe`):

```bash
python -I xbox/tools/patch_generated_sm2.py
python -I xbox/tools/gen_hook_defaults.py spiderman2
python -I xbox/tools/gen_gte_ops.py
GAME=spiderman2 MSYSTEM=MINGW64 C:/msys64/usr/bin/bash.exe --noprofile --norc xbox/tools/build.sh -j8
```

Each build writes its link map beside its XBE (`build/release/OpenSpidey.map`,
`build/sm2/OpenSpidey.map`): `run_xemu.py` reads the one beside the XBE it runs, and
`xbdm.py` Spider-Man's unless `OPENSPIDEY_MAP` names another. Switching game relinks.

`patch_generated_sm1.py` is the C counterpart of the PC's generated-source transform
`spiderman/tools/patch_texture_registry.py` (texture registry relocated, player models
register every texture), and makes the game's four vblank-countdown loops call the
clock (the PC catches them by counting memory reads); run it after every generation.
`gen_gte_ops.py` lists the GTE command words for the per-command GTE routines; a word
missing from the list still works, through the general routine.

The dependency files nxdk writes name Windows paths make does not match, so the
Makefile rewrites each as a `.mk` with make's own paths and includes those: a change to
a header rebuilds what includes it (checked: `texpack.h` rebuilds its four users,
`recomp.h` the generated code too). Switching `RT_PROFILE` or `RT_NO_VTAGS` changes no
file, so delete the `.obj` files under `src`, `runtime`, `generated` and `build` before
building after one.

## Assets and settings

The disc carries the player's game files plus what the Windows-side preparation makes
from a PC install (the future installer's job):

```bash
python -I xbox/tools/prep_assets.py "<PC install>/assets/builtin" private/work/xbox-disc/assets
python -I xbox/tools/prep_suits.py "<PC install>/mods/suits" private/work/xbox-disc/mods
```

- `assets/builtin`: the Dreamcast character actors, served in place of the game's own
  (`src/wad_overrides.c`, the PC's `LooseWadOverrides`; which actor answers is the
  game's costume code, `src/assets_sm1.c` and `src/costume_sm2.c`). Spider-Man 2's are
  its Dreamcast Spider-Man, the 19 costume texture libraries and the dedicated
  Bag-Man, Peter Parker, Quick Change and SM1 Spider-Man actors.
- `assets/packs/<pack>`: their art as DXT1 DDS, matched by the PC pack's fingerprint
  of each uploaded texture (`runtime/texpack.c`): `dreamcast-sm1-actors` and
  `dreamcast-sm2-special-costumes`.
- `mods/suits/<id>`: a suit's textures as DXT1 DDS, halved to 256 on the longest side
  but never below 256 on either side (suit art is often 1024-2048; see Memory), and for suits with their own
  model the actor and its RTG2 rig split apart (`src/suits.c`, `src/rig.c`, the games'
  `retarget_sm1.c`/`retarget_sm2.c`; the rig maths is the PC's
  `tools/retarget/native/retarget_core.cpp`, compiled in). `suit.txt` gives the power
  profile as SM1's index and by name, which SM2 looks up in its own list. The suit
  folders are the same for both games.

`D:\openspidey.ini` holds `level=<lNaM>` (Spider-Man 2: `level=<eNmM>`; start a new
game there, `src/level_sm1.c`, `src/level_sm2.c`),
`suit=<id>` (a test run's suit, not kept; players choose in the costume menu) and `dreamcast_movies=0` (PS1 movies only; no effect while Dreamcast movies are out). Include `openspidey.ini`,
`mods` and `assets` with `--include`.

## Sound

The sound chip (`runtime/spu.c`) is a port of the PC's: its registers and DMA channel
4 reach it through `runtime/memory.c`, and each vblank mixes 735 frames (44.1 kHz) into
the AC97 output (`runtime/audio.c`, resampled to 48 kHz). Streamed music and dialogue
are CD-XA: a read in XA mode (`runtime/cd.c`) plays the disc's ADPCM sectors at drive
speed instead of delivering them, the selected file and channel decoded into the chip's
CD input (the title music is `COMPILED.XA` channel 13). Movies take the AC97 while they
play. No reverb, as on the PC. `COMPILED.XA` must be on the disc.

## Saving

The game saves to PlayStation memory cards, kept on the hard disk as the same 128 KB
card files the PC keeps in its `saves` folder (`carda.sav`, `cardb.sav`; they move
between the two unchanged): `E:\UDATA\6F730001\4F70656E5370\carda.sav`. `runtime/card.c` answers the game's BIOS
card calls as the PC runtime does, writing each changed frame through to the disk.
`6F730001` is the title ID (`runtime/title.h`): nxdk's cxbe writes 0xFFFF0002 into every
program, so `tools/patch_xbe.py` replaces it after the link; the lower-case "os" cannot
match a retail publisher. The title's name on the Xbox is "Spider-Man X": `patch_xbe.py`
writes it into the certificate too (launchers list that; cxbe's `-TITLE` cannot take a
space), and `runtime/card.c` rewrites `TitleMeta.xbx` with it at every start. On a
debug console the monitor shows `E:\UDATA` as `V:`. The dashboard reads the title's name from `TitleMeta.xbx` and
its pictures from `TitleImage.xbx` and `SaveImage.xbx`, which `tools/make_xbox_images.py`
makes from `spiderman/port/OpenSpidey.png` (with `default.tbn` for launchers); copy the
three beside `default.xbe` on the console. Launchers and emulators take the icon from
inside the XBE, so `patch_xbe.py` also inserts the pictures and the name as the XDK's
image builder does (`$$XTINFO`, `$$XTIMAGE`, `$$XSIMAGE`; the method of Perfect Dark X's
`scripts/insert-xbe-dashboard-assets.py`). Checked in XEMU: save, load, and the save
surviving a console reset (`run_xemu.py --reset-at`). XEMU's hard disk image has no
dashboard, so how the dashboard shows the title is checked on the console.

## Movies

The game's own STR player needs the MDEC and XA streaming, which the Xbox runtime does
not have, so the runtime plays movies itself from the hook at 8002AD0C (the PC's
`DreamcastMoviePatches`): the PS1 movie from the disc's `CINEMAS` folder (raw
2336-byte sectors, as the install holds them). Spider-Man 2 has no such hook on the PC
(its build runs the game's own player on the PC's MDEC): `patch_generated_sm2.py`
makes the game's player (func_80030350) call `src/movies_sm2.c` once it has noted the
movie as seen, which plays the `CINEMAS` or `HEADLINE` STR the movie number names and
returns as the player would (1 played, 0 skipped). Checked in XEMU (2026-11-09): the
Activision and Vicarious Visions logos, 10.3 s and 15.9 s long, took 10.4 s and
16.0 s, and the title followed. Dreamcast movies are out on the Xbox
for now: the `.SFD` path (from `D:\mods\movies\dreamcast`, the 23 the PC maps) is
commented out in `runtime/sfd_core.cpp`, `runtime/movie.c` and `src/movies_sm1.c`,
and an Xbox install carries no `mods\movies`. Sound is resampled to the AC97's 48 kHz and is the clock; Start or Cross skips
after the first second. A movie that cannot play is skipped, as before. Checked in
XEMU (2026-11-06) on the boot logos: PS1 and Dreamcast pictures, sound matching
ffmpeg's decode of the same files, skip, and the title screen after. On the console
(2026-11-09) the PS1 movies play in time (the status log gives each one's length, the
time it took and the latest picture): the two boot logos 14.0 s and 14.7 s long took
14.1 s and 14.8 s, the level 1 intro 185.9 s took 186.0 s, no picture over 0.21 s
late, and the game carried on into the level with its sound. Every movie had hung at
its end on the console until `runtime/audio.c` learned that the console's sound chip
does not stop after the last buffer (see there); XEMU (2026-11-09) agrees, the logos
ending and the title following. Why the Dreamcast movies are out: their picture is
right but they play at about 40% speed on the console (the 14 s Activision logo took
about 35 s, the game's ticks dropped meanwhile), and while one plays it holds about
3.2 MB (the decoder's workspace and four 320x240 pictures). The SFD decoder would need
to be made faster before they come back.

`compile_generated.sh spiderman` compiles the generated code alone, as a quick check.

## Memory

At 720p the console's 64 MB goes, before the game loads a level: about 22.6 MB to
the XBE (10 MB code, the 8 MB PlayStation RAM window, other buffers), 3.6 MB to
`XVideoSetMode`'s frame buffer, about 26 MB to pbkit (three frame buffers, the
depth/stencil buffer, the two display-buffer render targets, the 4 MB push buffer)
and the game thread's stack. The status text reports the rest: `memory low` (the
lowest free memory since boot), free memory at the start-up stages, contiguous memory
by use (textures, palettes, texture pack, suits, movie), the asset arena's peak, the
largest frame in the push buffer and the game thread's stack depth.

Measured on the console in every level (38 acts, `private/work/hw/sweep.py`), the
lowest free memory went from 1.1 MB to 5.7 MB (2026-11-09), and stays above 5.1 MB
with the heaviest suit (Iron Spider with tentacles: own model and rig). What changed:
- The game thread's stack is 256 KB, not 4 MB: the deepest it reached anywhere
  (levels, a Dreamcast movie, a save) was 4 KB. The kernel commits a whole stack.
- The blit staging image is a display area's size (640x512), not all of VRAM.
- Palettes share 4 KB pages, four to a page (the kernel hands out whole pages).
- Texture-pack art is released when over 1 MB loaded and not drawn for 60 frames,
  so a long game does not keep every level's characters (checked with a 64 KB budget:
  about 85 releases and reloads a level, the characters' art right).
- Suit textures are 256 at most (see Assets and settings).

Kept as they are: the push buffer (the largest frame measured was 1.1 MB, and the CPU
may run two frames ahead), `XVideoSetMode`'s frame buffer (nxdk's debug text, its USB
stack and its assert all draw into it, and `XVideoSetFB` cannot point it at pbkit's
memory, so freeing it could let a stray message write freed memory), and the 8 MB RAM
window (the asset arena peaked at 2.9 MB with a suit, too close to a 6 MB window's
room). Frame rate standing still is 30 fps (the game's own rate) in levels 1 and 3.

## Test in XEMU

```bash
G=private/clean-1.1/Spider-Man/game
python -I xbox/tools/make_disc_map.py $G private/work/xbox-disc/disc.map
python -I xbox/tools/run_xemu.py --shots 20,40 --include $G/SLUS_008.75 --include $G/SYSTEM.CNF --include $G/CD.HED --include $G/CD.WAD --include private/work/xbox-disc/disc.map
```

`--include` puts the player's extracted game files and the sector map on the test disc. The runtime
keeps its full status in `g_status_text`; at every screenshot the script copies it
out through the XEMU monitor (address from the link map beside the XBE) to
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
The subdivision and edge rules are `src/world.c` (the PC's WorldSubpixel), both games.
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
(undrawn pixels show). Widescreen (`src/wide.c`, a port of `spiderman/patches/Wide.cs` and
`WorldFrustum.cs`, both games: each gives its gameplay loop's buffer swap) is on for
gameplay when the display is 16:9; menus stay 4:3 and are pillarboxed. A fix to the
widescreen rules belongs in both copies.

Reading VRAM back (StoreImage, the source of a VRAM-to-VRAM copy) first brings what a
display buffer's render target holds for that rectangle into `gpu_vram`
(`hw_read_back`): drawn frames live in the render targets, not in `gpu_vram`. SM2's
Game Over reads the last frame back a row at a time to dim it behind its menu, which
came out black before. SM1 reads back three times in a minute of level 1.

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
6. **Video modes and the compilation launcher**, then Spider-Man 2. Spider-Man 2
   bring-up, 2026-11-09: its own XBE; shared GPU and pad patches; its overlay rules
   (region 0x80200000-0x80280000), vblank wait (gp+0xCD0) and menu packet order;
   its six vblank-countdown loops (0x800C2430) and pad-read loop given the clock.
   Result in XEMU: the Marvel logo, the title screen and the attract demo, matching
   the PC (whose demo also has a black background). Gameplay and mods, 2026-11-09:
   direct level start (`level=e1m1`) into the street fight; the Dreamcast Spider-Man
   and costume assets served from the prepared files; mod suits of every kind,
   checked in XEMU in E1M1 -- a reskin of SM1's model (1967 TV Series, on the
   dedicated SM1 actor), an SMU suit with its own model and rig (Miles Morales,
   animated by SM2's own animations, the rig matching the PC's core, 1.3 ms a pose),
   and own-model suits on the Symbiote and Quick Change bases (the symbiote profile
   changes the HUD icon). Spider-Man 1's suits and Dreamcast actors, now on the shared
   code, rechecked in level 1. Parity with Spider-Man 1, 2026-11-09 (XEMU): movies
   (`src/movies_sm2.c`), widescreen, precise world subdivision and the silhouette
   shadow (now shared, `wide.c`, `world.c`, `shadow.c`), the Bag-Man shell depth and
   the armour test (shared), saving and loading through the memory card menu ("SAVE
   COMPLETED", "LOAD SUCCESSFUL"), and the Game Over menu over the dimmed last frame
   (VRAM read-back, above). Every mission (24) started directly and played in XEMU
   without a stop, the lowest free memory 7.2 MB at 720p. Costume menu, 2026-11-09
   (XEMU): both games' COSTUME VIEWER lists the mod suits after the stock costumes
   (SM1's twenty, with the ten imported SM2 Dreamcast costumes; SM2's nineteen), each
   with its name, powers and comments, the PC's CostumeViewer port and its
   generated-code transforms. A suit chosen there is worn in gameplay and kept across a
   restart (suit-sm1.txt / suit-sm2.txt in the save folder). Each change frees the
   previous suit's textures, rig and model: 112 changes in SM2 and 108 in SM1 left
   free memory where it started. Found on the way: nxdk's fopen leaks a kernel mutant
   per call (repeated reads and writes now use runtime/files.c), and a fill drawn while
   a push-buffer block was open could corrupt it after a buffer wrap (XEMU: "Reserved
   pb command"). Next: a console run, and the launcher.

Game data is never part of the repository or a published image; the player's own
discs supply it.
