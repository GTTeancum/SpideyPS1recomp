# Issue #10: Spider-Armor pickup crash with custom models

**Status: open; reported crash not reproduced on 2026-10-29.** Seven bounded
Linux attempts across two batches did not establish an ordinary armor pickup. Static SM1 analysis
identified an unsafe texture-remapping assumption, but it has not been connected
to the reported failure by an observed crash. No game-code correction was made.

The [community intake, R01](community-reports-2026-10-29.md) does not identify
which game, executable, custom actor, or pickup the reporter used. The findings
below concern the supported SM1 `SLUS_008.75` input; they do not establish an SM2
cause or a regression boundary.

## Actual test setup

The first five attempts used source commit
[`892df80`](https://github.com/GTTeancum/SpideyPS1recomp/commit/892df80c0be87e4a17e99d8bbbcc8536bc373225)
and identical SM1 assemblies. The
`SpiderMan.dll` SHA-256 was
`a8286267cff3b39b19a52af32a7d94489f783b2f76b84a16c858cd024d9fee28`;
`RecompOne.Runtime.dll` was
`b33bbace50b2e31effa113bf0fc3426374b81a090665b64bc7a72880db18d409`.

Execution used an isolated cloud Xvfb display, Mesa llvmpipe software rendering,
render scale 1, and OpenAL's null driver. Audible audio and hardware performance
were not checked. Existing `SPIDEY_LEVEL` configuration selected L8A3 or L8A1;
these were targeted entries, not campaign progression. Menu entry used the
existing input script; subsequent input consisted of ordinary mapped controller
buttons delivered to the owned game window. Saved snapshots were read-only.
The controller did not write guest state or synthesize pickups. No isolated
armor-execution probe was used.

The first three attempts used bundled assets, including the converted Dreamcast
default Spider-Man. The final two isolated that same converted player through
the existing asset-directory configuration, with retail NPCs and an empty host
texture-pack directory. This was a configuration comparison, not a source fix,
retail-player control, or test of the reporter's custom model.

## Five attempts and their limits

Run IDs share the `issue10-` prefix. Counts are observed vblanks, not elapsed
seconds or host-rendered frames. Results were checked against `run-result.json`,
retained command events, logs, private pilot audits where available, and saved
captures/snapshots.

| Run suffix | Duration; final count | Result |
| --- | --- | --- |
| `l8a3-before-01` | 179.65 s; 8408 | Player HP reached zero by 3040 while idle, before the first manual movement at 3592. Armor remained inactive in all 14 valid player samples. Requested wrapper stop: `queue_stop`, child exit `-15`. |
| `l8a3-before-02` | 480.11 s; 19064 | Ordinary navigation reached armor height. Closest sampled position at 13720 was about 256 level units from the pickup; HP 20, armor inactive, armor HP 0. Fixed outer timeout: `runner_timeout`, exit `-15`. Proximity did not establish collection. |
| `l8a1-before-01` | 300.05 s; 6640 | Introduction and premature pauses consumed most of the attempt. Last sample remained near the start, about 4472 units from the target; HP 100, armor inactive. Outer timeout interrupted the final movement: exit `-15`. |
| `l8a1-player-only-before-01` | 195.12 s; 8064 | Five ordinary Cross taps were delivered; no directional gameplay movement was delivered. Last numeric sample at 7960 had HP 40 and armor inactive. Requested wrapper stop: exit `-15`. |
| `l8a1-player-only-before-02` | 279.34 s; 12000 | Movement/calibration inputs left the player near the starting area. All 81 valid samples through 11920 had HP 100, armor inactive, and armor HP 0. Final capture was paused. Configured `SPIDEY_EXIT=12000` produced `game_exit`, exit `0`. |

Exit `-15` above is the wrapper's SIGTERM after its stop/timeout, not a
spontaneous game crash. Inspected logs contained no unhandled-exception,
unmapped-dispatch, crash-dump, or fired-watchdog marker. None of these outcomes
tests the pickup transition or establishes that armor works.

An early observation incorrectly suggested an XSync/controller stall in the
first player-only attempt because the visible journal stopped at the third
Cross press. The final retained events show Cross presses three through five
released normally and the requested stop was handled. **That stall hypothesis
is retracted:** journal staleness/truncation did not establish a blocked input
helper. Four-vblank Start taps did not reliably produce a visible pause
response; future navigation should use longer taps and visibly confirm pause
state.

## Follow-up: pickup contact and recorded-route attempts

Read-only inspection of all 120 retained L8A3 snapshots from 1600 through 15880
found the armor actor linked and enabled: type 13, trigger record 59, radius 128,
and no pickup-disable flag. Its actual position was `(2,-3219.5938,-432)`.
The initial trigger chain is `0 -> 53 -> 59`.

The native pickup loop compares the player's position with the pickup using
`max(|delta|) + middle(|delta|)/2 + min(|delta|)/4`, truncated to level units.
Collection requires a result **strictly below 128**. At the previous closest
sample, 13720, that metric was **287**. None of the 120 samples satisfied it.
Armor has no additional collection-button requirement or wall-cling exclusion
in this acceptance block. At the same Y/Z, the player needed about 160 more
units along positive X to enter the contact volume. Apparent model proximity
therefore did not imply a failed pickup.

Two further ordinary-input attempts used
[`3cec7fe`](https://github.com/GTTeancum/SpideyPS1recomp/commit/3cec7fe5c22bed55e432ce3d356da5d643f66b80).
Their SM1 apphost, game DLL, and runtime hashes matched the earlier attempts;
no armor correction or game-source change was applied.

| Run suffix | Actual result |
| --- | --- |
| `l8a3-route-03` | A compressed input sequence changed the early jump trajectory. Frame 2040 showed a fall, frame 2400 showed Game Over, and a later scripted Cross caused Retry and a level reload at 2484. The later introduction was a restarted level, not a blocked initial introduction. Requested stop at 4948: 126.48 s, wrapper exit `-15`; 82 captures and 51 valid player samples, all with armor inactive. |
| `l8a3-route-04` | Configured a 6020-callback schedule preserving the old route's pause/idle timing; stopped before completing it. Early positions matched, but later climbing and damage diverged. Last sampled position at 7840 was `(-645.1687,-376.7280,-98.2161)`, HP 10, armor inactive. Requested early stop at 8032: 241.57 s, wrapper exit `-15`; 66 captures and 27 valid player samples. |

The input schedule matched the recorded button words; game-state equivalence
did not. The game uses wall-clock pacing, camera changes, and native collision
responses. These traces are not established pickup replays. Neither follow-up
reached the pickup, and neither wrapper stop was an observed crash.

## Static finding: retail armor mapping versus converted textures

[TextureRegistry.cs](../../spiderman/patches/TextureRegistry.cs) and
[patch_texture_registry.py](../../spiderman/tools/patch_texture_registry.py)
extend player-texture registration to converted/custom materials. Native armor
routine `func_80046EEC`, however, still assumes the retail mapping:

1. `func_80046AF8` searches 13 player-to-armor hash pairs at `0x80099658` and
   returns zero for an unknown hash.
2. None of the bundled converted default player's nine texture hashes matches
   those 13 keys. Retail player keys have corresponding armor mappings.
3. The following lookup, `func_80046980`, leaves its pixel/palette output pointers
   untouched when no match exists. Its caller then consumes those outputs.
4. Before lookup, the caller also saves texture/palette data into fixed
   8192-byte and 1024-byte buffers. A guard placed only after lookup would miss
   that earlier size assumption for unsupported pages.

This is a source/data compatibility defect candidate, **not a reproduced
explanation of issue #10**. No crash stack from a collected pickup exists.

Retail `sparmour.psx` is a texture-only bank; bundled `sparmor.psx` is a complete
18-object actor. Aliasing those filenames would violate the loading contract.
Full armor-actor swapping requires separate model/resource lifetime work.

## Ordinary route targets

Read-only TRG inspection found 12 armor records across 10 SM1 trigger files.
The two focused routes use type-5/subtype-13 pickups. Coordinates below are raw
level units; negative Y is upward. Distances are direct geometry, not verified
walking paths.

| Target | Initial spawn | Pickup | Route consideration |
| --- | --- | --- | --- |
| L8A1, Waterfront Warehouse, record 90 | `(-2437,-211,-78)` | `(1501,-715,2198)` | First-warehouse crates; about 4576 units away and 504 units up. Initial spawn activates it through records `0 -> 88 -> 90`. Best next route candidate. |
| L8A3, Stopping the Fog, record 59 | `(-23,-114,-5596)` | `(2,-3213,-432)` | Top of central generator; about 6023 units away and 3099 units up. Climbing/alignment prevented collection in this batch. |

The closer L8A1 record 279 belongs to the hidden Ark-box route associated with
What If mode; it must not be confused with the normal first-warehouse pickup.

## Candidate correction and next action

The smallest proposed fallback is a prehook on `func_80046EEC` that skips the
retail cosmetic remap for a live `spidey` allocation owned by the override
loader. It would keep the selected custom appearance and native armor
protection. Activation (`func_800489D4`) and damage absorption are outside this
visual routine.

The hook would need to preserve guest registers, maintain visual-state flag
GP+`0xA80`, preserve pending palette-preparation flag GP+`0xA84`, and remember
the matching bypassed disable per memory session even after a model change.
Retail actors should retain the native path. This proposal is **unimplemented
and unverified**; silver armor appearance would remain separate work.

Keep #10 open. Next obtain a verified short ordinary pickup route or supported save,
capture actual collection, and compare converted-player and retail-player
controls. Only after a failing transition is established should a bounded fix
be paired with before/after pickup, damage, depletion, restart, and transition
checks. This batch verified neither pickup nor armor mechanics, and supplies
no basis for closure or a release-fix claim. Do not repeat the unsuccessful
L8A3 recordings as though they were deterministic pickup routes.
