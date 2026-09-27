# To-do

## GitHub issues

Local task status, updated September 27. GitHub issue state is unchanged.

Optimization scope: **both Spider-Man and Spider-Man 2**, across Intel/AMD CPUs and integrated/discrete GPUs, including powerful machines.

- [x] **P0 — Performance acceptance and remaining stalls across both games.** Closed by user direction on September 27. Historical implementation evidence and verification limits remain in the [repair handoff](docs/performance-repairs-2026-09-17.md) and [hardware test procedure](docs/performance-validation.md). Closure does not assert additional hardware or long-session testing.
- [x] **P1 — Chase Venom fade and splitting meter ([#3](https://github.com/GTTeancum/OpenSpideyPS1/issues/3)).** Fixed native blend eligibility for replacement textures and the common widescreen transform for the segmented rail, caps and markers. Native fade/disappearance and continuous-meter captures inspected; staged September 27. Authored scenery and prior timing/audio fixes retained. Chase completion was already established by the user and was not re-proved. See [P1 rendering evidence](docs/rendering-p1-2026-09-27.md) for verification scope.
- [x] **P1 — Character-rig retargeting and shoulders.** Closed by user direction on September 27 following the preserved-rig suit conversion work.
- [x] **P1 — Retry and Pause menu rendering.** Repaired cyclic native Pause packet tails in both games. Native regressions pass; inspected backgrounds, text, selection/resume, bounded consecutive Pause frames and Retry reloads. Both local user-test stages verified; limits and exact paths are in the [P1 rendering evidence](docs/rendering-p1-2026-09-27.md).
- [ ] **P2 — [#4: Add SFD Dreamcast FMV support](https://github.com/GTTeancum/OpenSpideyPS1/issues/4).** Use optional Dreamcast SFD videos when present, retaining PS1 videos as the fallback. Deferred until performance work is addressed.

Historical implementation evidence and verification limits: [Timing and audio repairs](docs/timing-and-audio-repairs.md). Additional SM2 work: [Spider-Man 2 to-do](spiderman2/TO_DO.md).

Rendering goal completed September 27. Fixes and targeted native visual checks
are recorded in [P1 rendering evidence](docs/rendering-p1-2026-09-27.md).
User-test folders: `proof_render/p1-user-test-2026-09-27/Spider-Man` and
`proof_render/p1-user-test-2026-09-27/Spider-Man 2`. Both include local disc data
and published mods. No new performance, audio or rig acceptance is claimed.
No push or GitHub issue-state update was requested or performed.
