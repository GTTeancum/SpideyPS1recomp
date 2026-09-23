# Paged Transport Integration

Status: metadata/core, native actor writing, managed loading, independent decoding
and drawing hooks are implemented. Both large suits passed Windows game checks
and are installed as batch 27. Windows and Linux core binaries are updated, with
previous binaries backed up. Linux was cross-built and statically inspected, not
executed; this does not claim Linux game acceptance.

RTG2 version 2 remains byte-layout compatible. Version 3 retains the first 280
header bytes and adds four uint32 fields: page count, driver-map offset, and two
reserved zero fields. It accepts 19..64 pages and at most 8192 weighted vertices.
Each page retains the native 256-vertex bound. Pages 0..17 use drivers 0..17;
remaining pages use driver 0. Alternate hand pages 6/11 exactly duplicate 5/10.
All source triangles occur once outside those intentional aliases.

The validator bounds every page/map and requires ordered non-overlapping major
sections. The core still receives exactly 18 native animation matrices. Source
bones, weights, bind matrices and skinning arithmetic do not change.

## Remaining Integration

- Dynamic native mesh pointers/names now retain the exact extra-page LOD chain.
- Actors keep 18 native terminal meshes/animation drivers. Root mesh 0 chains through
  extra pages 18 onward; roots 1..17 remain terminal.
- Independent actor decoding and managed loader validation now require
  version 3 and the exact page topology for actors with more than 18 meshes.
- Drawing hooks now skin all resident pages, bypass ordinary LOD selection only
  for the active paged context, and schedule extra pages under native driver 0.
  A 366-check process-local harness verifies ordering, termination and isolation;
  this is not actual paged native rendering acceptance.
- Configured instruction hooks regenerate from the current recompiler source.
  Generation fails if a required instruction hook is absent or duplicated. The
  config generator retains prior pose, web and native movie hooks as well.
- Both staged actors pass 55 structural/full-animation checks, 115 web frames
  each, and managed loading/evaluation, including six malformed-chain cases each.
  All 231 installed legacy actors still pass managed loading with the new DLL.
- The first Damon run stalled because packed animation used the full mesh count
  (29) against the original 18-driver stream/HIER. The scoped 80010678 hook now
  supplies the preserved driver count only for the active paged custom actor.
  Unpacked animation and unrelated actors retain their original path.
- Both candidates now complete native sewer runs, with all extra pages scheduled,
  live pose/web traces and two individually reviewed images each. The original
  failed run remains evidence. A same-build Anti-Venom control also completed.
- The two suits are installed with preserved prior catalogue files. Linux
  freestanding cross-build has no external dependencies/unresolved symbols and
  passes ELF/export/provenance checks; Linux execution remains unverified.
  Fist closure and other catalogue-wide visual issues remain separate.

`test_paged_rig.py` exercises the real Damon Ryder and Other sources over all
4196 frames. It checks complete triangle assignment, page round trips, bind
reconstruction, preserved source metadata, and malformed-page rejection. It
also compares five specified poses for all 231 installed version-2 suits against
the explicitly supplied `--reference-library` baseline. Identical library hashes
are rejected so the comparison cannot silently compare the new core to itself.
This evidence is not native rendering or visual acceptance.

Linux cross-build: `python tools/retarget/build_native.py --target linux-x64 --freestanding-linux`
requires LLVM clang++, ld.lld and llvm-readobj. It fails on unresolved symbols,
unexpected exports or dependencies. Provenance marks `runtimeExecuted: false`
on non-Linux hosts; ordinary Linux builds still load the library to read its ABI.
