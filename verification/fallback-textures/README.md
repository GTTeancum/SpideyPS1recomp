# Exact Native Fallback Audit

The initial batch 22 suite failed three texture heuristics: Batty Brant,
Devil Spider and Dusk. The original result is retained as
`batch22-initial-failed.json`. That heuristic compared hidden RGB under alpha,
used a different resize order, required asymmetric images, and decoded native
palette colors incorrectly. It checked only the first material.

The replacement checks every slot against source-derived RGB5 colors, native
STP/transparent semantics and exact row order. Native palette expansion matches
`TextureTile.Expand`; four unit tests cover palette behavior, symmetric images,
flipped rows, swapped slots and changed alpha. No converter or runtime behavior
was changed for these test corrections. The full batch 22 rerun passed 256
checks, including all 4196 animation frames for each of its ten suits.

The broader installed-210 audit did not pass: Gwenom and Venom from the initial
import have two authored material slots but one installed slot. Both source slots
use the same image; the second is assigned to 3 and 280 triangles respectively.
208 other suits matched every fallback pixel and material count. These failures
are tracked in SMU-OPEN-ISSUES.json and must be repaired, not waived.

This is offline texture evidence, not gameplay or original shader equivalence.

After installation of the reviewed Gwenom/Venom repairs, the exact audit passes
all 231 installed suits and 261 material slots (`installed-231-repaired.json`).
The complete managed catalogue audit still fails one manifest, Devil Spider,
whose 1024x6144 repeat atlas exceeds the current dimension cap. All 231 native
actor containers and the other 230 manifests pass. These are separate gates;
an exact texture match does not prove the host can load that texture.

The later original-body recovery resolves Devil Spider without increasing any
loader limit. `installed-231-body-repaired.json` passes all 231 suits and 261
material slots; `verification/repair-devil-body/catalogue.txt` reports all 231
actor containers and manifests passing. The earlier failures remain historical
evidence. See `verification/source-repairs/devilspider` for the upstream cause.
