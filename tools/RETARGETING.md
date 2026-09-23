# Preserved-rig native retargeting — cumulative repair build 03

Build03 adds a render-only swing-web wrist correction. See `WEB_ATTACHMENT.md`
for its measured results and offline/live verification boundary. The rig
architecture and Repair02 measurements below are retained; the assets are unchanged.

The previous `retarget_fbx_rig.py --target/--profile/--model` length-scaling patch
is retired. It wrote the wrong native fields, did not convert meshes and did not
preserve a playable source rig. `tools/test_retarget_fbx_rig.py` now runs the real
native/core regression suite; it no longer reports a misleading header-only PASS.

## Architecture

1. Read the FBX's authoritative bind-pose and skin-cluster matrices. Retain every
   source bone/control node, parent link and positive skin influence. Preserve the
   exact original matrices, model properties, cluster indices/weights and input
   SHA-256 in the embedded native metadata as well.
2. Calibrate a **separate virtual rest pose**. Segment rotations align with native
   animation axes; target parent-to-child distances are retained. The source bind
   rig is not overwritten by a native template. Clavicles remain children of the
   chest instead of inheriting upper-arm positions/rotations incorrectly.
3. Apply the game's global bone rotations to this calibrated target rig. Use target
   local translations for FK. Scale animation-local pelvis translation by the
   target/native leg-length ratio. Do not scale the actor's world movement.
4. Articulate the preserved finger bones into fists. Determine flexion axes from
   the palm geometry and original bone frames; explicitly oppose the thumb across
   the curled fingers rather than treating it like another finger. The core also
   exposes an open-hand control pose for comparison.
5. Perform full variable-influence linear-blend skinning in the shared native core.
   Mesh packets are storage/render transport, **not new rigid skin assignments**.
   Undo the exact current Q12 native part matrix before writing vertices, then let
   the existing native renderer apply it once. Update normals and packet bounds.

## What "native format, original rig intact" means here

The output is a native v4 `.psx` actor: ordinary geometry packets, native faces and
UVs, palettes/textures, unchanged original 18-object flags/origins and unchanged
original animation/HIER data. A bounded **RTG2 metadata extension** inside that
same asset contains the full imported rig and weights. There is no FBX loaded at
runtime, no Blender process, and no separate replacement-mesh renderer.

The recomp's `SuitModel` loader retains RTG2 host-side and removes its block from
the bytes sent to the original guest loader, relocating the texture pointers.
Thus the guest never has to understand a new metadata tag. This is **recomp-native
output**, not a claim that an unmodified retail PS1 executable supports weighted
FBX rigs. The source FBX and donor files remain byte-identical.

Meshes may be split at source normal discontinuities or native packet capacity
boundaries. Vertices created for these splits retain their source influences.
No triangle is decimated or discarded. Native alternate-hand packets 5/6 and
10/11 share identical vertex mappings and wrist origins. Both use the fist pose.

## Integration points

- `spiderman/patches/SuitRetargeting.cs`: SM1 instruction hook at `80077418`, after
  native pose evaluation/cached-pose selection and before lighting/drawing.
- `spiderman/config/spiderman.json` plus generated `main.cs`: registered hook.
- `RecompOne.Recompiler`: instruction-hook emission, so regeneration retains it.
- `RecompOne.Runtime/Assets/Suits/NativeRetargetRig.cs`: bounded shared-core binding.
- `RecompOne.Runtime/Assets/Suits/SuitModel.cs`: RTG2 validation, retention and guest
  stream stripping. Existing non-RTG2 suits bypass the new skinning path.
- `dreamcast/tools/build_unlimited_actor.py`: defaults to the preserved-rig path.
  `--legacy-template` is an explicitly warned opt-in to the old destructive fitter.
  The old Blender template-transfer addon remains a legacy tool, **not** this path.

The hook resolves the active resident `spidey` model slot by name, validates all
packets before writes, and does not assume title-screen and level slots match.
A missing/wrong native DLL causes the rigged suit to be rejected, not silently
loaded with the wrong rig. `SPIDEY_RETARGET_TRACE=1` enables a first-use live hook
message. A catalogue registration message alone is not proof of live retargeting.

## Repair02 measurements (historical; build03 regressions in WEB_ATTACHMENT.md)

- Spider-Ham: **63 rig nodes, 2,044 triangles, 2,149 source influences**.
- Spider-Man 2099: **66 rig nodes, 2,918 triangles, 2,693 source influences**.
- Counts include original non-deforming/control/armature nodes, not only weighted
  deform bones. Ham has 1,278 control points / 1,280 normal-split runtime vertices;
  2099 has 1,684 of each. Normal splits explain Ham's 2,155 runtime influences.
- Both use the same FBX-to-native unit scale, 19.161587330459145. Ham is not stretched
  to adult height. Animation-local root ratios are approximately 0.3663 and 0.9028.
- **54 regression checks passed**, including 300 clips / 4,196 original frames for
  each character (8,392 character-frames total). Across those poses, maximum
  relative link-length drift is under 0.079%, consistent with Q12 rotations.
- Native signed-16 packet round-trip error stayed below 0.853 native units per
  component; full source-bind reconstruction error stayed below 0.001 native unit.
- Linux C++ core and Windows x64 DLL compiled. The Windows DLL has no CRT imports.
- ASan/UBSan: 10,000 mutated/truncated rig blobs, no sanitizer findings. Structurally
  valid mutations are not claimed to be semantically identical rigs.

## Honest verification boundary

**The complete .NET game/recompiler build and a live game process were NOT run in
this environment.** The compatible .NET SDK/runtime was unavailable and network
retrieval attempts failed. The C# integration is implemented and statically
reviewed, not claimed compiled or live-validated. No replacement game EXE is in
this repair package. The included DLL is the retargeting component, not the game.

Proof PNGs/GIF use the actual converted `.psx` geometry, preserved rig, runtime C++
core, source textures, native signed-16 quantization, and the original game's
animation routines in a compiled oracle. The oracle translates the supplied
packed-animation decoder, native matrix routine and pose evaluator with a small
memory/GTE harness; it is **not the whole game or an independent emulator**. The
Chromium screenshot captures that offline native-asset viewer and is labeled so.
All 300 clips were numerically exercised; only the included representative poses
were visually inspected. Do not interpret this as every gameplay state looking
perfect or as an in-game screenshot.

Not implemented/verified here: foot/contact IK, gameplay collision/camera resizing,
non-swing web/grab contact-point correction for the shorter rig, every part-specific hide
mask, or full SM2 integration. Oversized anatomical parts spill into other visible
native packets (137 Ham triangles, 6 2099 triangles); skinning remains weighted,
but native part-specific visibility effects require live verification. Unmodified
stock actors bypass retargeting, but a full stock-game regression was not run.

The importer currently targets the supplied **Clown001-named SMU rig family**:
one triangulated skinned mesh, compatible bind/cluster matrices, one opaque atlas,
and uniform non-reflected bone scale. It rejects unsupported scenes instead of
removing weights, auto-decimating, or guessing a bone mapping. This is not yet a
universal arbitrary-FBX importer. Do not use the original game's older executable
with the new RTG2 suits and assume they are animated correctly.

## Use / build

Apply the repair overlay with the package installer; it verifies expected source
hashes and backs up existing files before changing anything. Then run
`BUILD-RETARGET-REPAIR.cmd` from the project root on Windows with .NET 10 SDK.
The two sample suits are copied beside the newly built executable under
`mods/suits/smu-spiderham` and `mods/suits/smu-2099`; select them through the existing
suit catalogue. This command is supplied, **not a claim it ran here**.

No Python/Blender dependency is required to play a converted suit. Offline
conversion requires Python, NumPy and Pillow; rebuilding the small native core
requires clang++ and lld-link. Prebuilt Windows/Linux core libraries are included.

```text
python tools/retarget/convert.py --fbx "SMU-Costumes/costumes/spiderham/spiderham.fbx" --reference "SMU-Costumes/costumes/2099/2099.fbx" --donor spiderman/extracted/wad/spidey.psx --texture "SMU-Costumes/textures-png/SpiderHam_D.png" --out new-spiderham-suit --id my-spiderham --name "Spider-Ham"
```

The output must be new/empty; source input files are never overwritten. For the
existing entry point, use `dreamcast/tools/build_unlimited_actor.py --source ...
--reference ... --texture ... --donor ... --output ... --id ... --name ...`.

## Reproduce verification

```text
python tools/retarget/build_native.py
python tools/retarget/build_animation_oracle.py --project . --out oracle
oracle/animation-oracle spiderman/extracted/SLUS_008.75 spiderman/extracted/wad/spidey.psx native-animation.bin
python tools/retarget/test_retarget.py --project . --samples SMU-Costumes --suits spiderman/port/mods/suits --animation-bank native-animation.bin --out retarget-test-results
python tools/retarget/make_proof.py --suits spiderman/port/mods/suits --animation-bank native-animation.bin --out retarget-proof --capture
```

The oracle build is currently a Linux/g++ proof utility. Browser capture optionally
requires Playwright and Chromium; ordinary PNG/GIF rendering does not. No fonts
are redistributed. `native/test_sanitized.cpp` is the standalone sanitizer harness.
See the package evidence for actual commands, hashes and measured results.
