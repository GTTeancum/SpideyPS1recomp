# Devil Spider Source Recovery

The original supplied FBX contained 304 vertices and 152 triangles belonging to
a web helper. The original BDAE also contains a 1819-vertex, 3058-triangle body
at descriptor offset 30840. Its descriptor size 0x150, vertex stride 92 and
primitive kind 9 were outside the importer's accepted cases. After skipping
the body, the importer's positional name mapping called the first helper the
body. This caused both the oversized repeat atlas and invisible-gameplay result.

`repair_devil_source.py` adds strictly scoped, hash-checked support in the
Blender process without editing the source toolkit. It validates all source
triangle references, imports the full original body and rig, and excludes the
two separately named gameplay web helpers from the costume export. The body
uses its actual DevilSpider material, not the helper's Iron material.

The registered derived FBX is retained here. The source BDAE, original FBXs and
textures remain unchanged. `SOURCE-REPAIRS.json` checks the identities of all
three source stages before the pipeline uses the repaired export. Repeated
export verifies exact semantic equality despite differing FBX timestamps/IDs.
The raw skin influence audit reports zero weight error.

The installed body passed full 4196-frame animation and 115-frame web tests,
finger policy, material preservation, managed loading and individually reviewed
native swing/crouch images. See `verification/repair-devil-body` and the task's
two retained output images. Open-hand appearance remains a separate issue.

An experimental loader-size increase allowed the faulty helper to load, but
its two frames were invisible. It was reverted completely. The final repair
uses the original 4096 texture-dimension and 64 MiB decoded-memory limits.
