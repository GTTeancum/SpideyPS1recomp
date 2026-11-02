# Chase Venom: update rate on the original disc

Measured November 2, 2026. Question: does the retail game run Chase Venom (`l5a1`) at
30 updates/s with only the through-building sequence at 20, as the port's earlier
`Timing.cs` cadence switch assumed? It does not; the port now follows the disc's pace
in 5 FPS steps (see "Port pacing").

## How the retail game paces itself

The main loop (`8002C1E0`..`8002C29C` in `main`) runs one update, then calls the
vblank wait `8005E748` twice with `A0=1`. That routine waits for the vblank counter at
`GP+0xC74` to advance by `A0` from its value at the call. An update that finishes
inside one vblank therefore takes two (30/s); one that overruns takes three (20/s) or
more. Nothing in the game reads the counter apart from that wait and the IRQ handler
that increments it, so every update advances the simulation by a fixed amount and an
overrun slows the game down rather than skipping time. There is no authored 20 Hz
mode: the rate is whatever the console's load allows.

The port runs updates in far less than a vblank, so without help it always gets two.

## Measurement

PCSX-Redux nightly 30168.20261102.2 (interpreter, OpenBIOS, `-debugger` for
breakpoints), original `Spider-Man (USA).bin`, run with
`tools/redux/run.sh chase_timing.lua` (no window; output in
`private/work/redux/chase-full`). The script:

- presses Start and Cross through the menus at the port's usual offsets;
- swaps the level descriptor `l1a1_t` to `l5a1_t` at `80018800`, as `SPIDEY_LEVEL`
  does;
- reproduces `SPIDEY_CHASE_FOLLOW`, `SPIDEY_CHASE_REGION_PULSES` and
  `SPIDEY_CHASE_FULL_AUDIT`: the player is moved to Venom's position from 20 vblanks
  earlier, the region commands are issued by calling `8005BA58` from the main loop
  when Venom waits at a stop, the follower lets go while the authored player script
  runs, and at the last stop region group 272's links are run;
- logs every update (one row per change of the counter at `800B4F38`): its vblank
  time, player and Venom positions, Venom's next route point and the player-script
  flag.

Screenshots confirm the run is the real chase: "Chase Venom to his hideout", the
stops, the building sequence, the route on to the hideout and the ending script, after
which the next level loads.

## Result

| Stretch | Updates | Disc updates/s |
| --- | --- | --- |
| Intro script | 22 | 18.9 |
| Start to Venom's first stop | 221 | 20.0 |
| First to second stop | 163 | 20.1 |
| Second to third stop | 275 | 21.0 |
| Third to fourth stop | 92 | 15.1 |
| Fourth to fifth stop, building entry | 60 | 15.3 |
| Building script | 760 | 21.5 |
| On to the sixth stop | 372 | 20.7 |
| Sixth stop to the hideout | 311 | 24.8 |
| Ending script | 156 | 18.9 |

The rate also moves inside a stretch; within the building script, for example, a run
of about 180 updates holds two vblanks while most of the rest take three.

Stops fall at level +733, +1219, +2006, +2371, +2590 and +5800 vblanks; the building
script runs from +2606 to +4724 and the ending script starts at +6552.

## Port pacing

`spiderman/patches/Timing.cs` holds the chase to these rates, rounded to 5 FPS steps,
by lengthening the main loop's second wait:

| Stretch | Port |
| --- | --- |
| Intro script, start to the third stop | 20 |
| Third stop to the building, building script | 15 |
| On to the sixth stop | 20 |
| Sixth stop to the hideout | 25 (two and three vblanks alternating) |
| Ending script | 20 |

The stretch is found from the player-script flag (the first start is the intro, the
second the building, the third the ending) and Venom's next route point (third stop
35, building entry 54, sixth stop 227, last point before the hideout 241). Other
levels keep 30 updates/s. This replaces the earlier switch that held only the
building script at 20.

The building script is the exception to rounding. Its disc rate is 21.5, but at a
steady three vblanks per update Spider-Man's scripted run misses the cutscene region
(script `800E2B28`, fired from `8005A890`): the script ends about 590 vblanks in and
the sequence stalls. A steady two or four vblanks both reach it about 360 vblanks in,
as the disc's own mix does; four is the nearer step. 25 (two and three alternating)
fails like 20. With 60 FPS interpolation off the result is the same.

Full chase on the port with this pacing, same fixture (vblanks after the first stop;
disc in brackets): second stop +486 (+486), third +1275 (+1273), fourth +1635
(+1638), fifth +1851 (+1857), building script +1871 (+1873), route resumes +3967
(+3991), sixth stop +5194 (+5067), hideout +5952 (+5819). The level ends and l5a2
loads as on the disc.

With the disc's exact per-update cadence replayed (`SPIDEY_CADENCE_FILE`), the port
matches the disc to within a few vblanks over the whole chase, and player positions
match at every update through the building script.

## Limits

- PCSX-Redux approximates CPU timing and does not model memory wait states or GPU
  drawing time closely; it generally runs games no slower than hardware. Not checked
  against Mednafen or a console.
- The follower teleports the player along Venom's route instead of ordinary
  traversal, the same compromise as the port's fixture. Normal play has not been
  checked.
