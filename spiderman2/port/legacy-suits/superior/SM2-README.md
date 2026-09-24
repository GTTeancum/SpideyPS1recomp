# Superior in Spider-Man 2

This migrated suit retains model `spiderman` and its original PNGs.
Its current powers donor is `2099`.
Copy this folder into `mods/suits` beside `SpiderMan2.exe`, restart, and select it
under SPECIAL → COSTUMES.



`model` chooses the body and its matching texture layout. `abilities.profile`
chooses one original SM2 costume as the **powers donor**. Changing the powers
profile keeps your painted appearance, model, and texture files.

For example, this suit can use Prodigy's double jump, enhanced strength and
enhanced web swinging while keeping the selected body and texture layout:

```json
"model": "spiderman",
"abilities": {
  "profile": "prodigy"
}
```

Replace those fields in `suit.json`; the snippet is not a complete manifest.
Choose one profile at a time. Profiles inherit the original game's behavior;
you cannot combine donors, invent powers, or select an enemy's moveset with JSON.
The original donor costume does not need to be unlocked to use it for a reskin.

## Donor powersets

Use the exact identifier in the left column. All 19 native SM2 profiles are
available, including the SM2-specific double-jump and enhanced-swing donors.

| `abilities.profile` | Costume powers |
| --- | --- |
| `spiderman` | Standard Spider-Man abilities |
| `spider-phoenix` | Invulnerability, enhanced strength, enhanced web swing |
| `prodigy` | Double jump, enhanced strength, enhanced web swing |
| `dusk` | Stealth |
| `insulated` | Enhanced strength and electrical resistance |
| `alex-ross-red` | Double jump |
| `alex-ross-white` | Enhanced web swing |
| `venom-earth-x` | Enhanced strength and unlimited webbing |
| `negative-zone` | No additional costume powers |
| `symbiote` | Unlimited webbing |
| `2099` | Enhanced strength |
| `captain-universe` | Invulnerability, enhanced strength, unlimited webbing |
| `unlimited` | Stealth mode |
| `bagman` | No Spidey belt |
| `scarlet` | No additional costume powers |
| `ben-reilly` | No additional costume powers |
| `quick-change` | No Spidey belt |
| `peter-parker` | No Spidey belt |
| `battle-damaged` | No additional costume powers |

For double jump alone, choose `alex-ross-red`. For enhanced swinging alone,
choose `alex-ross-white`. For unlimited webbing alone, choose `symbiote`.
`unlimited` means the Unlimited costume's stealth profile, **not unlimited webbing**.
Insulated's menu lists enhanced strength; its native costume identity also enables
the game's electrical-resistance behavior.

**GAME POWERS** uses the original costume wording automatically. Keep `name` to
18 characters. Comments share the panel with the donor's power list; if a donor
with more powers makes the mod fail validation, shorten `comments`.
