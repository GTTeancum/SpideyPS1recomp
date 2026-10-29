# Issue #11: SM2 crash before Shocker

**Status: open — not reproduced in two bounded Linux checks on 2026-10-29.**
The Shocker arena and its preceding stage both loaded and accepted ordinary
controller input. The normal transition between those stages remains untested.
No game-code fix was made for this issue.

Tracking: [issue #11](https://github.com/GTTeancum/SpideyPS1recomp/issues/11),
[first arena-screen result](https://github.com/GTTeancum/SpideyPS1recomp/issues/11#issuecomment-6504247947),
and [community intake R02](community-reports-2026-10-29.md).
The intake does not identify the reporter's executable hash, selected actor,
last loaded archive, exception, or exact transition. Successful targeted entry
therefore does not establish that the reported failure is fixed.

## Exact stage mapping

Read-only inspection of the supported `SLUS_013.78` archive established the
following targets. Record numbers below are zero-based TRG record indices.

| Stage | Archive evidence |
| --- | --- |
| Shocker encounter: `E1M4` | `e1m4_t.trg` record 1 loads the `E1M4_L`, `E1M4_O`, and `E1M4_G` world models, `shocker.psx`, and the `shocker` overlay. Record 2 is a Shocker spawn of native type `0x14B`. |
| Preceding stage: `E1M3` | `e1m3_t.trg` exit-script record 113 names `E1M4_T`. |
| E1M4 restart | `e1m4_t.trg` record 246 names `E1M4_T`. |
| Following stage | `e1m4_t.trg` exit-script record 48 names `E2M1_T`. |

E1M3 contains 18 thug spawn records: 14 linked from the initial wave record and
four later records linked to a completion node. Its script text includes
"Just one more left.." and "Good Job! Cut Scene Time". This supports a combat
route to the exit, but the complete native gate conditions have not been
decoded or exercised. It does not prove that exactly 18 kills are mandatory.

The historical pre-render failures listed in
[SM2 open-work notes](../../spiderman2/TO_DO.md) concern `e3m1`, `e3m2`, `e4m1`,
and `e5m2`. Those prefixes are distinct from the mapped Shocker route; no shared
cause has been established.

## Shared test setup

Both actual runs used the Linux build of
[`892df80`](https://github.com/GTTeancum/SpideyPS1recomp/commit/892df80c0be87e4a17e99d8bbbcc8536bc373225)
and user-supplied supported retail inputs, in an isolated cloud Xvfb display.
Logs identify the bundled shared Dreamcast Spider-Man actor and retail costume
slot `00`; this was the project's bundled configuration. No custom actor,
cheat, or lenient-mode override was requested for these checks.

Presentation used Mesa llvmpipe, OpenGL 4.5, render scale 1, and the existing
16:9 gameplay setting. Audio used the OpenAL null driver, so audible playback
was not checked. All frame numbers in this report are observed vblank counts,
not host presentation counts or a real-time performance measurement.

The existing [level override](../../spiderman2/patches/LevelSwitch.cs) redirected
the first new-game prefix to `e1m4` or `e1m3`. Scripted Start/Cross presses
advanced the title and entry sequence. Subsequent movement, attack, and pause
inputs were normal controller buttons delivered to the owned game window.
The input helper did not write actor state. Saved RAM was inspected read-only.
Neither run began from a completed E1M3 campaign save.

Evidence audited for each run consisted of `run-result.json`, `env.json`,
`controller-events.jsonl`, `console.log`, `spidey.log`, and the captures named
below. The runtime files were independently hashed and matched the run metadata:

| Runtime file, identical in both runs | SHA-256 |
| --- | --- |
| `SpiderMan2` | `b9ccdde1ac0ba896db3baa813fb364cbddcde018973b6d7a19282fc7aea7353c` |
| `SpiderMan2.dll` | `807cdd3c94368545aafb3e83273f5b0e5caf9efb3eaddcb3827a0c594fece686` |
| `RecompOne.Runtime.dll` | `b33bbace50b2e31effa113bf0fc3426374b81a090665b64bc7a72880db18d409` |

## Run A: direct E1M4 entry

Run ID: `issue11-e1m4-before-02`. Started at 1:21:07 p.m. EDT (17:21:07 UTC) and
finished after 155.15 seconds. This was a current-build baseline, not an
unpatched-versus-patched comparison.

| Observation | Evidence |
| --- | --- |
| Correct target | The log redirected the first new-game prefix and loaded `e1m4_t.trg` at `f1018`. |
| Encounter resources loaded | At `f2224`, the log loaded the E1M4 world assets, `shocker.psx`, `shocker.bin`, and `shocker.rel`, with the Shocker overlay activated. |
| Visible encounter | Inspected `frame_03960.png`: warehouse, player, Shocker, player HUD, and boss health bar were rendered. |
| Ordinary movement | Right was pressed at `f4428` and released at `f4548`, without reported release failure. Inspected `frame_04560.png` shows changed player/camera positioning with the encounter still rendered. |
| Live Shocker actor | Read-only snapshots at `f4200` and `f4800` each contained a linked actor of native type `0x14B` with expected vtable `0x80256B08`. Its position and state fields changed between samples. No health field was interpreted. |
| End state | Inspected `frame_06000.png`: **GAME OVER**, with Retry/Quit choices. The configured `SPIDEY_EXIT=6000` ended the process with exit code `0`; metadata reports `game_exit`. Retry was not selected. |

Both logs contained no unhandled-exception, unmapped-dispatch, crash-dump, or
fired-watchdog marker. The watchdog was armed. Log output ended at `f5980`,
followed by the retained `f6000` capture and configured exit. This bounded tail
does not establish a recurrence of the separate long-run logging issue.

The earlier attempt `issue11-e1m4-before-01` could not access Xvfb and failed
before gameplay frames. It was an environment setup failure, not a reproduced
game crash, and is excluded from the two actual gameplay checks.

## Run B: direct E1M3 entry and input check

Run ID: `issue11-e1m3-before-01`. Started at 1:52:46 p.m. EDT (17:52:46 UTC) and
finished after 108.07 seconds at the operator's requested stopping point.

| Observation | Evidence |
| --- | --- |
| Correct target and world load | `e1m3_t.trg` loaded at `f1018`; at `f2224` the log loaded the E1M3 world assets, `henchman.psx`, and the `thug` overlay. |
| Visible gameplay | Inspected `frame_02640.png`: player and HUD rendered in the crate-filled stage. |
| Movement/jump input | Right + Cross were pressed at `f3600` and released at `f3720`. Inspected `frame_03720.png` shows a changed position and airborne pose beside a wall. |
| Attack input | Square was pressed at `f3720` and released at `f3840`. Inspected `frame_03840.png` shows the player and a nearby thug. Button delivery and continued rendering are established; a punch hit, outgoing damage, or enemy defeat is not. |
| Pause response | Start was pressed at `f3840` and released at `f3852`. Inspected `frame_04440.png` shows the **PAUSED** menu. |
| Requested stop | The controller queued a stop at `f4440`; metadata reports `queue_stop`, followed by wrapper `SIGTERM` and exit code `-15`. This was a deliberate test stop while paused, not an observed crash. |

All three button sequences reached their observed frame targets and reported no
release failure. Both logs continued through `f4440` and contained no
unhandled-exception, unmapped-dispatch, crash-dump, or fired-watchdog marker.
No E1M3 completion, enemy defeat, cutscene-to-Shocker transition, or E1M4 load
was reached in this run. The close camera position also limits what the
captures establish about combat and visual quality.

## Capture integrity

These are SHA-256 identifiers for the inspected original captures. The report
publishes derived observations and identifiers; retail data, saves, and raw
RAM snapshots are not included.

| Run | Capture | SHA-256 |
| --- | --- | --- |
| A | `frame_03960.png` | `445c65243e0b34fd3cec77275aef35d819851148500e7f6be8d259ffe18e5a9e` |
| A | `frame_04560.png` | `9bd0a6885ef0609a2f9eb0414120bb1d0239bb9ffd04b7f94022e8d8ff028842` |
| A | `frame_06000.png` | `594e21e096ee14d8042ee746b959697627b16ea0bc65ea729475770a6059eeb0` |
| B | `frame_02640.png` | `0b77e2fb2af5a2cd5a06e830776af912f9ac1df4290fd220eeb576571ae345a1` |
| B | `frame_03720.png` | `fe08bdc37bb4e26296ae0d44f0d577c837eaf32dcfb8c199f738a40081553c9b` |
| B | `frame_03840.png` | `dfeffdc84e8fd813b7f2708a9e7ea77e971402353536d99772481e34f232a168` |
| B | `frame_04440.png` | `9bc06f3f681f7cd6bf00321051a0b0dcbfaa06b4fdcd05a1d8094807ce25f454` |

## Remaining blocker and next action

**Keep #11 open.** Neither run reproduced the reported crash, and no causal
fix or paired before/after test exists. The untested boundary is ordinary
E1M3 completion through its exit/cutscene into E1M4, including any state carried
across that transition. E1M4 Retry, boss defeat, level completion, alternate
actors, the reporter's installation, and Windows behavior are also unverified.
The empty original-state hash sets in these runs do not provide a saved-game
preservation check.

The next focused test should start near E1M3's ordinary exit, using a supported
save or a documented normal-input route, and capture the E1M3-to-E1M4 boundary
without overriding the destination. If that fails, retain the final loaded
archive, exception/dispatch details, actor selection, exact input sequence,
and a short visible capture before proposing a bounded fix. If it succeeds,
check E1M4 Retry and compare with the reporter's exact build and configuration
when available. Avoid treating a longer unrelated navigation session as a
substitute for the missing transition reproduction.
