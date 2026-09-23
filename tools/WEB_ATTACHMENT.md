# Spider-Ham swing-web repair — cumulative build 03

## Result

The large height gap is real at the native-code level. Repair02 animates the
preserved imported rig, but the native swing controller still obtains its web
endpoint from the original adult 18-part skeleton. In the controlled capture of
original swing clip **275, frame 25**, that endpoint is **134.23877 world units
above Spider-Ham's animated wrist**. Across clips 275 and 280, the measured gap is
**133.66650–145.27197 world units** for Spider-Ham. 2099's smaller discrepancy is
4.93286–9.33496 world units. These are world units, not pixels or metres.

Build03 makes the rendered swing web follow the current preserved-rig wrist.
The corrected final line coordinate equals that wrist's world-Q12 coordinate in
all **115 frames per suit**. No constant height offset or suit-specific height is
used. The native integer mesh transport remains within 0.829 model units per
component (less than 0.052 world units); zero endpoint-to-bone error does not mean
zero pixel error against every polygon or perfectly shaped finger contact.

## Actual fix and source trace

- Original `func_80073E18` resolves spidey's socket 0 to native part 10 and socket 1
  to part 5, then applies `func_8007FA00` and the actor/world coordinate conversion.
- The player update's branch at **80046444** uses socket 1 for animation 0x118
  (280), otherwise socket 0. It subtracts eight units along the actor's forward
  vector, then updates the swing controller with `func_80022D68`.
- Player `+F84` points to the swing controller; controller `+178` points to its
  line object. Its points and controller `+F8` also participate in native motion.
  Editing these would risk changing release/swing behaviour, so they are untouched.
- New **instruction hook 8002CF80** runs in `ptr_8002CEE4`, immediately before its
  `func_80080988` projection call. It edits only the six world-Q12 scratch words
  at **1F800000–1F800017**. It never overwrites the original rope points, player,
  collision data, controller or environmental anchor.
- `SuitWebAttachment.cs` limits the hook to the active RTG2 player suit and the
  actual swing line (plus its native secondary/doubled-point embellishment).
  Stock actors, NPC/effect lines and the separate zip-web controller are bypassed.
- The endpoint comes from the same calibrated FK as the weighted mesh. Current
  full native poses are read from the normal native cache, with clip/frame checks
  for the original 0x2C bank; stale or unsupported caches are skipped, not guessed.
- Actor rotation, mirror, position and translation use the native model-to-world
  conversion, with double intermediates to retain distant-world precision.
- Each original line vertex receives a fraction of the endpoint correction,
  rising from zero at the environmental anchor to one at the wrist. This retains
  curvature and pins the tip. Original inputs are re-read for **every segment**;
  using the previous modified scratch value would double the warp and cause seams.

The C# bindings and C++ DLL share **ABI 0x00020002**. Do not mix the new DLL with
an older executable/runtime. Both config registration and the generated main.cs
are patched; the prior code-generator hook support preserves the new hook when
regenerating the project.

## What was actually run

**58 regression checks passed.** The current Linux core was exercised against the
previous compiled Repair02 core on all **300 clips / 4,196 frames for each suit**.
All full-rig bone matrices and weighted vertex outputs remained **bit-identical**
(8,392 character-frames). Fists, shoulders, rigs and skin weights are not changed.
The two native `.psx` assets are byte-identical to Repair02.

A compiled offline oracle mechanically translates the original socket helper,
GTE socket transform, native line builder and native line iteration from the
supplied generated source. The recorded inputs cover both real swing clips:
**230 character-frames / 3,680 corrected native line segments**. Tests verify the
original native socket against independent pose math, exact corrected endpoints,
unchanged anchors, segment continuity, unchanged actor/point bytes, and no
CPU/GTE register changes by the small C++ hook adapter. The managed selection and
cache guards are statically checked, not falsely described as executed C# tests.

Additional tests cover 60 actor-transform cases per suit, hand aliases, curved
and doubled-count lines up to 4,096 segments, integer overflow, malformed rigs,
nonfinite wrists, absent/ambiguous mappings, and atomic rejection. ASan/UBSan ran
**10,000 mutated/truncated rigs** plus **40,000 new-export stress calls**, with no
sanitizer findings. Structurally valid mutations are not necessarily equivalent
rigs. Windows x64 and Linux x64 native libraries compiled; the DLL has no imports.

## Capture boundary — important

**The PNG/GIF are offline native-asset/code diagnostics, NOT screenshots from the
running game.** They show actual converted geometry, source textures, native
signed-16 mesh transport, original decoded swing animation and the original
native web-line coordinates before/after correction. White strokes display those
line coordinates. The final native projection call is a coordinate recorder in
the oracle; full GPU projection, depth/occlusion and gameplay are not tested.

The test uses a declared, controlled environment anchor, identity actor transform
at the origin and +Z forward. It is **not a saved gameplay swing state**. The GIF
samples every second clip275 frame with a 100ms display delay, not measured game
timing. The large vertical mismatch is obtained from real native pose/socket
math, not created by the chosen anchor or an artificial vertical offset.

The full .NET 10 SDK/runtime is still unavailable in this environment; another
retrieval attempt failed. No complete game/recompiler build, live managed hook,
or live gameplay capture has been obtained. The code fix is implemented and
native-core validated, but it is not a claimed fully game-tested release. No new
game EXE is supplied. The PowerShell installer has been inspected and its
manifest logic validated with a Python reproduction, not executed in PowerShell.

This repair concerns **swing-web visual attachment only**. It does not change
movement/collision/camera height, add IK, implement grab/zip/shoot-web corrections,
or verify all part hide masks and SM2 paths. Both hands remain articulated fists;
placing the web precisely between individual fingers is not the acceptance target.

## Installation

Extract the complete package. Run `INSTALL-REPAIR.cmd` and select the project root
(the folder containing `spiderman`, `tools`, and `dreamcast`). It checks all payload
and destination hashes before writing and backs up changed files. It accepts the
uploaded base project or Repair02; unrecognized local edits are refused rather
than overwritten. **This is cumulative; Repair02 is not a prerequisite.**

Then run `BUILD-RETARGET-REPAIR.cmd` from the project root with the .NET 10 SDK.
This rebuilds the game and copies the matching native DLL and the two sample
suits to its output. Do not test the new source/DLL with an old executable.
`SPIDEY_RETARGET_TRACE=1` enables a first-use `[web-retarget] live swing render`
message; catalogue registration is not evidence that this hook executed.

## Reproducing the native verification

The package's `evidence` directory contains the exact decoded animation bank,
extracted RTG2 blobs, recorded coordinates, prior comparison core, build/source
hashes and executed-test logs. Source scripts are installed by the overlay.
From the project root, replace PACKAGE below with the extracted package path:

```text
python tools/retarget/build_native.py
python tools/retarget/build_web_oracle.py --project . --out web-oracle
web-oracle/web-oracle PACKAGE/evidence/spiderham.rtg PACKAGE/evidence/native-animation.bin web-oracle/spiderham-swing.bin
web-oracle/web-oracle PACKAGE/evidence/2099.rtg PACKAGE/evidence/native-animation.bin web-oracle/2099-swing.bin
python tools/retarget/test_web_attachment.py --project . --bank PACKAGE/evidence/native-animation.bin --captures web-oracle --previous-core PACKAGE/evidence/previous-core/libOpenSpideyRetarget.so --out web-tests
python tools/retarget/make_web_proof.py --project . --bank PACKAGE/evidence/native-animation.bin --capture web-oracle/spiderham-swing.bin --out web-proof --browser --gif
```

The oracle/regression commands above are the Linux proof workflow, not the Windows
game build. Python tools use NumPy and Pillow. Browser capture additionally uses
Playwright and Chromium; omit `--browser` for rendered PNG panels without browser
capture. No font files are redistributed. Full sanitizer commands are included
in `evidence/REPRODUCE-SANITIZERS.txt`.
