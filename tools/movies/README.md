# Original Dreamcast SFD playback — Repair06

Repair06 replaces Repair05's erroneous decoded `.osmv` cache path with an
**in-process streaming decoder of the original `.SFD` files**. The movie is not
transcoded, remuxed, rewritten or predecoded into another file. The runtime opens
one read-only source handle and decodes MPEG-1 video and ADX audio as needed.

This is integrated into the **SM1 USA / SLUS-00875** game source. The supplied
Linux x64 ELF and Windows x64 DLL are the decoder component, **not a game EXE**.
The full .NET games and managed integration tests have not been compiled or run
in this environment. Native decoder, audio-queue and graphics fixtures do not
establish a successful in-game movie transition. SM2 has no Dreamcast movie hook.

## Runtime files and deferred installer work

After rebuilding the matching game, original movie names are resolved under:

```
<game executable directory>/mods/movies/dreamcast/*.SFD
```

`SPIDEY_MOVIE_DIR` optionally selects another directory. `SPIDEY_DREAMCAST_MOVIES=0`
forces original PS1 playback. Names are case-insensitive, but duplicate/ambiguous
case variants are rejected. The source handle remains open for both streams;
there is no audio/video time-of-check/reopen-by-path race. Files are never opened
for writing. `.osmv` files are ignored.

**Optional Dreamcast-disc detection/extraction in the installer is deferred to
Codex, as requested.** This update does not add a media-installation workflow or
repackage the movies. The pre-existing disc extraction utility is unchanged.
The old `INSTALL-DREAMCAST-MOVIES` cache wrappers and cache authoring/installation
Python commands now stop with a retirement notice instead of creating/installing
the wrong format. They do not delete any existing user files or movie caches.

Normal playback requires neither Python, FFmpeg, Blender, an external player nor
installed codecs. The matching `OpenSpideySfd.dll` (Windows) or
`libOpenSpideySfd.so` (Linux) is deployed by the game build. MIT notices are copied
under the output's `licenses` directory. Do not mix an old game executable with
this source change and assume the new decoder is in use.

## Playback and fallback

The native instruction hook remains at `8002AD0C`: original allocation, STR ring,
display, input and watched-state setup happen first. The host player opens the
SFD and decodes its first picture before suspending the original audio path.
Missing files, unsupported headers, missing/mismatched decoder libraries, or an
invalid first frame leave the original PS1 movie path active. Failure after
replacement playback begins returns `Failed` and invokes original STR starter
`8002B1FC` before resuming PS1 decoding. Successful playback or a deliberate skip
branches to original cleanup `8002AF8C`. This is the actual game hook, not a
viewer-only change; live execution of that managed hook remains unverified.

Video frames are decoded in display order. Native reference pictures are retained
for I/P/B prediction; at most one current RGBA display frame is held by the managed
player. Audio is decoded to four 2,048-frame OpenAL buffers. The consumed audio
sample position, not queued duration, drives rational frame selection. When late,
intermediate pictures are decoded/dropped with bounded catch-up work between host
event pumps. Discontinuous original SFD DTS values do not reorder content or
supply a broken clock. Original sample rates and 30 or 30000/1001 frame rates are
retained. The final ADX block is trimmed to the header's declared sample count.

Without an audio device, an explicit monotonic silent clock is used; ADX is still
decoded and validated. An audio underrun or a stalled device triggers PS1 fallback.
A fresh Start/Cross press after one second skips; an entry button held down must
first be released. PresentFrame continues host events, input, interrupts and reset.
The original SPU/STR suspension, obsolete XA clearing and cleanup behavior from
Repair05 are retained. Native PS1 timed rumble cues are not replayed by the host
player; original cleanup still stops the motors.

## Supported input and bounded implementation

The implementation targets the supplied Dreamcast **Sofdec MPEG-1 + unencrypted
ADX v3** files, not every format ever named SFD. It validates the Sofdec signature,
program/PES structures, stream identity, MPEG sequence/picture/slice coverage,
VLC/motion bounds, ADX header/predictor/terminator and declared lengths.
MPEG-2/Sofdec2, encrypted audio and unsupported multi-stream layouts are rejected.

Native state is a fixed **1,967,360-byte** caller-owned workspace, with 32 KiB
read windows and no heap allocation, operating-system imports or subprocess.
Separate video/audio demux cursors read the same held source file; a full decode
can read the source approximately twice, not retain the whole movie in RAM.
Additional bounded managed RGBA/YUV arrays and normal audio/GPU buffers exist;
the native workspace figure is not the entire game's memory usage.
Limits include even dimensions through 720x576, 512 MiB source length, 108,000
pictures and ADX rates of 8–48 kHz. Unsupported files fail instead of allocating
from untrusted movie dimensions. No cryptographic integrity check is implied by
format validation; external modification of an open file can still cause failure.

23 retail STR basenames have verified SFD equivalents: 21 story movies, ATVILOGO
and TTSLOGO. `LOGO.STR` is deliberately not aliased to an unrelated logo. LEGAL,
NEVERSOFT, SOFDEC and TREYARCH are decoded in tests but remain unmapped extras.
No SFD, PS1 movie, save, model rig, skin weight or animation asset is rewritten.
Encoded pixel aspect metadata is retained; real Dreamcast presentation is not
claimed verified against hardware.

## Executed validation and reproduction

All 27 supplied original SFDs decoded to **37,391 video pictures**. Declared-length
ADX PCM matched FFmpeg 7.1.5 sample-for-sample for all movies. Video comparison is
not bit-exact: IDCT/prediction differences are recorded, including one localized
full-pel B-motion reference discrepancy in L2M2 frame 2. See `native/SOURCES.md`
and the evidence JSON; do not interpret audio equality as pixel-perfect hardware
emulation. All source hashes remained unchanged.

Executed tests include 12,616 native unit assertions, 10,000 structured/truncated
SFD mutation cases under ASan/UBSan, 18 Python tests (two explicitly static
managed-source checks), and two old-cache-tool retirement tests. Actual supplied
OpenAL Soft executed complete 44.1 kHz and 22.05 kHz movie queues and intentional
underrun/SPU-resume cases. Actual GLFW/OpenGL on Xvfb verified four decoded frame
texture uploads/readbacks. These are component fixtures, not C# game captures or
physical-speaker/lip-sync validation.

```sh
python3 tools/movies/build_native.py --verify
python3 tools/movies/test_movies.py
python3 tools/movies/test_install_cache.py
python3 tools/movies/compare_reference.py --movies /path/original-SFDs --out sfd-reference
python3 tools/movies/test_openal_movie.py --library /path/libopenal.so --movies /path/original-SFDs --out audio-results.json
xvfb-run -a python3 tools/movies/test_linux_movie_display.py --glfw /path/libglfw.so.3 --movie /path/original-SFDs/L1M1.SFD --out display-proof
```

The comparison command uses FFmpeg **as a development reference only** and writes
measurements, not converted movie assets. Python developer fixtures require NumPy;
image capture additionally uses Pillow. `make_test_fixture.py` creates the shipped
synthetic 32x32 test-pattern SFD using FFmpeg at development time and authored ADX;
that small test fixture contains no extracted game footage. Native rebuilding uses
clang++ and lld-link; both prebuilt targets and exact source hashes are supplied.

Supplied but **not executed here**, and required by the game build helpers:

```sh
dotnet run --project tools/RecompOne/tests/MovieOverrideRegression -c Release
dotnet run --project tools/RecompOne/tests/MovieInstructionHookRegression -c Release
```

The first links the actual managed decoder/resolver/player with host-device
fixtures and the native DLL/ELF. Passing it will not replace live movie-to-gameplay
validation. The second executes both generated instruction-hook branches. Codex
must build the complete games and verify real playback, A/V sync, skip/reset,
missing/damaged SFD fallback and transition to gameplay before marking #4 complete.
