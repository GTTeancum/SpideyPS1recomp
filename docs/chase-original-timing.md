# Chase Venom: update rate on the original disc

Measured November 2, 2026. Question: does the retail game run Chase Venom (`l5a1`) at
30 updates/s with only the through-building sequence at 20, as the port's
`Timing.cs` cadence switch assumes?

## How the retail game paces itself

The main loop (`8002C1E0`..`8002C29C` in `main`) runs one update, then calls the
vblank wait `8005E748` twice with `A0=1`. That routine waits for the vblank counter at
`GP+0xC74` to advance by `A0` from its value at the call. An update that finishes
inside one vblank therefore takes two (30/s); one that overruns takes three (20/s) or
more. Nothing in the game reads the counter apart from that wait and the IRQ handler
that increments it, so every update advances the simulation by a fixed amount and an
overrun slows the game down rather than skipping time. There is no authored 20 Hz
mode: the rate is whatever the console's load allows.

The port runs updates in far less than a vblank, so it always gets two, except where
`Timing.WaitVBlanks` adds a third during the authored building script.

## Measurement

PCSX-Redux nightly 30168.20261102.2 (interpreter, OpenBIOS), original
`Spider-Man (USA).bin`, run with `tools/redux/run.sh chase_timing.lua` (no window;
output in `private/work/redux/chase`). The script:

- presses Start and Cross through the menus at the port's usual offsets;
- swaps the level descriptor `l1a1_t` to `l5a1_t` at `80018800`, as `SPIDEY_LEVEL`
  does;
- reproduces `SPIDEY_CHASE_FOLLOW`/`SPIDEY_CHASE_REGION_PULSES`: the player is moved to
  Venom's position from 20 vblanks earlier, and the five region commands are issued
  by calling `8005BA58` from the main loop when Venom waits at a taunt stop;
- logs the vblank count of every update (one row per change of the counter at
  `800B4F38`).

Screenshots confirm the run is the real chase: "Chase Venom to his hideout", the taunt
stops, the building sequence, then "You lost Venom" once the follower is released.

## Result

Updates by the number of vblanks they took (1,852 updates). Stretches are in vblanks
from level start; the building sequence starts when the authored player script does
(+2606) and ends where the screenshots show the HUD back with Spider-Man outside.

| Stretch | 2 (30/s) | 3 (20/s) | 4 (15/s) | 5+ |
| --- | --- | --- | --- | --- |
| 0-1799, opening rooftops | 134 | 441 | 49 | 2 |
| 1800-2605, last stops before the building | 0 | 24 | 177 | 5 |
| 2606-4649, building sequence | 216 | 480 | 26 | 13 |
| 4650-5454, outside afterwards until "You lost Venom" | 52 | 223 | 7 | 1 |

Inside the building sequence the rate is not constant either: 3100-3499 is mostly two
vblanks (151 of 184 updates), while 3500-4649 is almost all three.

Region pulses land at level +733, +1219, +2006, +2371 and +2590 vblanks. The port's
fixture puts the same pulses 478, 778, 362 and 214 vblanks apart; the disc puts them
486, 787, 365 and 219 apart.

The retail chase runs at about 20 updates/s for most of its length and about 15/s
over the last stops before the building. The port runs all of it at 30/s except the
building script, where it holds a steady 20/s. Outside that sequence the port's chase
plays up to twice as fast in real time; inside it, the disc briefly returns to 30/s
where the port does not. 60 FPS interpolation (#21) does not change update timing
either way.

## Limits

- PCSX-Redux approximates CPU timing and does not model memory wait states or GPU
  drawing time closely; it generally runs games no slower than hardware. Not checked
  against Mednafen or a console.
- The follower teleports the player along Venom's route instead of ordinary
  traversal, the same compromise as the port's fixture.
- The port has not been changed.
