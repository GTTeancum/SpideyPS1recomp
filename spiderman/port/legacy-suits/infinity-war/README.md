# MCU Infinity War

Iron Spider — Battle Mode (Infinity War), from Spider-Man Unlimited.

Copy this entire folder into `mods/suits` beside either staged game's executable,
then select **Infinity War** from the costume menu. Requires custom `modelFile`
support. Standard Spider-Man powers; no gameplay abilities are added by the claws.

All four mechanical arms are static attachments to the torso. Body topology and
closed fists are retained. The native torso is limited to 256 vertices, so each
mechanical arm is reduced to 22 vertices; layered back decals and transparent
light cards are omitted. The full-resolution diffuse texture is retained.

Source: `ironspidernewwithtentackles.fbx` / `IronSpiderNewWithTentackles_D.png`.
Recipe: `docs/ports/unlimited-costume-mods.md` in OpenSpideyPS1.

The original diffuse RGB pixels are unchanged; its alpha is converted to opaque
for the native body material.
