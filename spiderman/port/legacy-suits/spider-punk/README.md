# Spider-Punk

Spider-Punk from Spider-Man Unlimited, with the original mohawk, shoulder spikes,
outfit texture and closed fists. Standard Spider-Man powers.

Copy this entire folder to `mods/suits` beside either game's executable and select
**Spider-Punk** in the costume menu. Requires the custom `modelFile` suit loader.

The head surface is lightly reduced to meet the native per-part vertex limit;
the body topology and separate spikes are retained. Original diffuse RGB pixels
are preserved; alpha is made opaque so the pants and arms render correctly. Native actor: 2,942 source triangles, largest
part 247 vertices. Both native hand variants use fists.

Source: `punk.fbx` / `SpidermanPunk_D.png`.
Recipe: `docs/ports/unlimited-costume-mods.md` in OpenSpideyPS1.

Waist revision: the coat's lower band and inner fold now follow one native joint
consistently. This replaces alternating Spine1/Spine2 assignments around the hem.
