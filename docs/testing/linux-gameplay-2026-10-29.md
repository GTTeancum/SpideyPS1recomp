# Native Linux gameplay verification — 2026-10-29

## Result and scope

Targeted SM1 `L5A3` testing reached the first Lizard-man encounter, real combat,
incoming and outgoing damage, Game Over, and ordinary Retry. This advances
verification beyond Linux startup and level-entry screenshots. **No enemy kill,
completed sewer level, or normal campaign entry is accepted by these runs.**

The tested game/runtime source is
[`774b3c4eceee1b30d5579387bec14c3facd41ec5`](https://github.com/GTTeancum/SpideyPS1recomp/commit/774b3c4eceee1b30d5579387bec14c3facd41ec5),
including the sewer animation repair from `307562e`. The working branch advanced
through documentation commits without changing these tested assemblies.

| Tested assembly | SHA-256 |
|---|---|
| `SpiderMan.dll` | `52c489ff4c9cd841111015c0dbfbee6add4cc188ec33ae7161aed8c28ac1a985` |
| `RecompOne.Runtime.dll` | `7180eea275c904880103ade15c252d7f2948ae41d25d1eb98b5cf30a73cc744b` |

The actual USA `SLUS-00875` retail input was supplied privately. Execution used
Ubuntu 24.04 x64, .NET 10.0.403, Xvfb, and Mesa llvmpipe OpenGL 4.5. The apphost
ran from an isolated copied runtime so its settings and cards could be compared
against the originals after testing. Both original-state hash comparisons passed.

The level was selected with `SPIDEY_LEVEL=l5a3`; this also permits the existing
model-residency rescue path. After the menu/intro input, traversal and combat used
ordinary mapped keyboard/controller actions. No invulnerability, artificial
trigger, guest-memory write, or alternate actor alias was used. Saved snapshots
were inspected read-only. This setup does not prove entry from the preceding
campaign level.

Audio used the OpenAL null device, so audible audio is unverified. Software
rendering supports functional observations, not a hardware performance claim.
This is native Linux evidence; Windows gameplay and SteamOS/Wine/Proton behavior
remain separate acceptance work.

## Observed runs

| Run | Duration and termination | Accepted observations |
|---|---|---|
| `sm1-l5a3-live-01` | 1,091.32 seconds; requested wrapper stop; child `-15` | Movement, jumping, wall climbing, pause, and Restart Level; 207 captures. Did not reach the Lizard-man encounter. |
| `sm1-l5a3-live-02` | 1,800.25 seconds; outer timeout; child `-15` | First encounter, combat/damage, death, and Retry; 207 captures. Retry traversal later blocked by the controller's stale progress source. |

Both child exits are the wrapper's SIGTERM. Neither is recorded as a spontaneous
game crash or a normal game exit. All game/test processes were stopped after the
bounded runs. The second run's final completed snapshot summary was vblank
19888; its old wrapper reported only 10768 because its progress reader was stale.

### Second-run evidence

Counts below are the game's logged/saved vblank counts, not elapsed seconds.
Capture filenames use `frame_<count>.png` despite that distinction.

| Count | Evidence | What it establishes |
|---|---|---|
| 1234 | `[dc-model]` correction for `lizman2.psx`, slot 8, 37 to 19 | The repair ran after the real loader installed the model. |
| 1728–2976 | HUD/player captures, changing player positions, cave traversal | Ordinary control and movement beyond the introduction. |
| 4608 | Two `SpawnActor("lizman", slot=0)` log entries | The nearby encounter was activated. |
| 4616 | Two further spawn entries; distinct saved actor positions | Distant pipe guards, not evidence of duplicate nearby spawns. |
| 4696, 5248, 5272 | Typed linked actors and changed positions in saved snapshots | Four constructed, persistent, updating Lizard-man instances. |
| 5184 | Visually inspected capture | A Lizard-man beside Spider-Man during traversal/combat. |
| 5760, 6240 | Visually inspected close-combat and grab captures | Visible enemy attack/grab animation and player interaction. |
| 6040 | Saved combat snapshot compared with 4696 | One nearby enemy HP 120 to 100; player HP 100 to 12. |
| 6720 | Visually inspected Game Over capture | The player died; no fight victory is claimed. |
| 7340–7352 | Ordinary Cross input on Retry | Retry was selected through the normal menu. |
| 8640 | Visually inspected player/HUD capture after reload | Live play returned after Retry; subsequent saved state has player HP 100. |

Six punch requests were delivered during combat; the snapshots establish only
20 total outgoing damage to one nearby enemy, not six successful hits. Three
later queue commands were mistakenly labelled as web-dome attempts but sent
Down+Triangle (web yank). They do not establish web-dome use. A prepared combat
helper never ran because the later waypoint route did not complete.

## Animation and collision evidence

The actor-list head is `0x800B5234`; list links are at actor `+0x1C`, type at
`+0x34`, and vtable at `+0x3C`. Lizard-men were identified by type `0x013D` and
vtable `0x80235124`. Actual health is signed 16-bit actor `+0xDE`, confirmed
against the native damage/replenishment routines. Actor `+0x21A` is a geometric
height/offset field, not health.

At 4696 and 6040, the two nearby actors were `0x8010DEF8` and `0x8010E284`; the
distant guards were `0x8010F780` and `0x8010F3F4`. The former nearby actor took
20 damage. The other three remained at 120 HP. All four remained linked.

Model slot 8 at `0x800A0B04` retained its cached animation count of **19** in the
post-spawn and combat samples. All eight sampled words at collision-grid base
`0x800A5BB4` remained:

```text
00000001 FF8D3000 FD7BA000 084B5014
0639C014 006FE801 00000000 00140014
```

The reported corruption pair `F2820894 / 0D6F0201` was absent at grid offsets
8/12. This is sampled evidence through the tested encounter, not continuous
validation of every collision operation or the entire level. No fatal,
unmapped-address, or missing-overlay marker was found in the logs inspected
through combat; later log silence limits any broader log-based assertion.

The [issue #5 evidence note](../issues/issue-5-sewer-crash.md) connects these
gameplay observations with the earlier isolated decoder reproduction and the
bounded load-time correction. Issue #5 remains open.

## Controller blocker and bounded repair

During the retry, both game logs stopped at count 10760 while screenshots and
completed snapshots kept advancing. The loaded controller used only those logs,
so its observed count stopped at 10768. A movement hold reached its wall-clock
safety deadline; later commands waiting for higher counts never started. This
demonstrates a controller progress-source defect, not a stopped game.

The private controller now supplements log counts with completed current-run
PNGs, stable complete numeric snapshots, and hash-verified atomic snapshot
summaries. It also recovers log offsets after truncation/rotation and keeps its
counter monotonic. Fifteen focused checks passed, including incomplete files,
malformed names, foreign paths/symlinks, summary hash/frame mismatches, and the
actual stopped-log case: the old reader returned 10760, while the repaired
reader reached 19432 from saved artifacts.

The repair did not update the already loaded wrapper and has not yet controlled
a fresh gameplay run. The cause of log cessation remains unestablished. No game
source or X11 event timestamp was changed for this repair.
The observed logging failure and remaining investigation are tracked in
[issue #19](https://github.com/GTTeancum/SpideyPS1recomp/issues/19); the private
controller fallback is a mitigation, not a resolution of that issue.

## Retained evidence and next action

The private evidence package retains the two runs' logs, 414 captures, command
and controller journals, controller recordings, snapshot summaries, independent
spawn/combat audits, run results, and the exact used helper versions. A separate
copy of the repaired helper and its check results is included for resumption.
Raw RAM snapshots, runtime copies, extracted game data, cards, and private asset
links are excluded from public commits.

Next, use a fresh bounded run with the repaired controller to finish the first
pair, traverse the pipe section with normal jump/swing actions, handle the far
encounter, and verify the `L5A4_T` transition. Static route/trigger inspection
identified that destination; runtime activation remains unverified. Then verify
normal campaign entry. Keep kills, objectives, level transitions, death/retry,
save/load, audio, and full campaign completion as separate acceptance results.

The game READMEs were corrected to explain the default 30 Hz presentation rate,
two vblanks per present, callback-based script holds, and visual acceptance.
Only documentation changed in the public repository during this batch. No game
rebuild or full regression rerun is claimed. The earlier source-specific
[Windows/Ubuntu ten-suite gate](https://github.com/GTTeancum/SpideyPS1recomp/actions/runs/40886198566)
remains the existing shared-code evidence.
