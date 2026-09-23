# Paged Transport Integration

Status: metadata/core implemented and tested offline; actor generation remains
gated until the native drawing loop is integrated and verified. Shipping DLLs
and installed suits have not been replaced by this work.

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

- Emit dynamic native mesh pointers/names and the exact extra-page LOD chain.
- Keep 18 native terminal meshes/animation drivers. Root mesh 0 chains through
  extra pages 18 onward; roots 1..17 remain terminal.
- Extend independent actor decoding and managed loader validation, requiring
  version 3 and the exact page topology for actors with more than 18 meshes.
- Skin all resident pages before drawing. Bypass ordinary LOD selection only for
  the validated custom paged actor; draw each extra page using native driver 0.
- Add configured instruction hooks and regenerate native code reproducibly.
- Test native loading, drawing/page counts, material coverage, full regressions
  and actual gameplay before removing the converter guard or installing suits.

`test_paged_rig.py` exercises the real Damon Ryder and Other sources over all
4196 frames. It checks complete triangle assignment, page round trips, bind
reconstruction, preserved source metadata, and malformed-page rejection. It
also compares five specified poses for all 231 installed version-2 suits against
the shipping core. This evidence is not native rendering or visual acceptance.
