# Reproduction attempts: #8, #12, #13, #14 (2026-11-01, SM1)

Builds: the current source (`master` after `35acba3`) and the unmodified
1.1 release package (`Spider-Man-1.1-Windows-x64.zip`, extracted to a private work
folder with the same imported game data). Runs were hidden and silent; every
result below was checked on captured frames, not counters.

## #14 Scorpion introduction skipped — not reproduced

The introduction is the pre-rendered video started by the L2A2 trigger script
(opcode 190, intro 22) before Scorpion and Jameson load.

- Current build, L2A2 entered from New Game with no input during the intro: the
  whole video plays (Scorpion threatens Jameson, Spider-Man arrives through the
  window, cut to the fight).
- 1.1 release, same route: the video plays.
- Holding Up, or holding Circle, from before the intro to its end: the video
  still plays in full. Only a fresh X press skips it, which is retail behaviour.
- Earlier "skips" during this investigation were caused by the test script's own
  fourth X press landing on the first frame of the video.

Not tested: arriving at L2A2 by finishing L2A1, and entering from the game's
own level select (cheat). Either could differ if the reporter took that route.

## #8 suit textures, #12 health icon/model, #13 enemy textures — not reproduced

- Costume viewer: 40 consecutive suit selections (stock, reskins, converted SMU
  models, the 8 MB-texture suits) each rendered cleanly.
- Gameplay with Captain Spider, 1967 TV Series and Amazing Spider-Man (L1A1),
  Amazing Spider-Man vs Scorpion (L2A2), Hobart Brown in L1A2, L3A1, L5A2
  (Venom fight) and L7A1 on the 1.1 release: player, HUD and enemies clean.
- Twelve Game Over/Retry cycles in L7A1 with Hobart Brown: no change after
  repeated reloads. The magenta creatures there are the level's symbiote
  invaders (pink by design, as on the level's comic cover), not a palette fault.
- HUD face icon checked for all 19 alternate stock costumes in L1A1. All intact.
  One oddity: the Captain Universe icon has two magenta pixels (RGB 214,8,230)
  on its top edge. They are identical on the 1.1 release and the current build,
  so they are not a regression; whether retail hardware shows them was not
  established. Imported costumes 10-19 and suit mods show the stock Spider-Man
  icon, which is the intended fallback, not corruption.

Related finding: the Spider-Armor swap (#10) copied saved texture pages through
unset pointers for converted and custom suits. When that did not crash, it wrote
into memory it did not own, which is one way texture corruption could have
appeared after an armor pickup. It is fixed in `a65960f`; whether it explains any
of these reports is not established.
