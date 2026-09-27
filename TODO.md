# To-do

## GitHub issues

Local task status, updated September 27. GitHub issue state is unchanged.

Optimization scope: **both Spider-Man and Spider-Man 2**, across Intel/AMD CPUs and integrated/discrete GPUs, including powerful machines.

- [x] **P0 — Performance acceptance and remaining stalls across both games.** Closed by user direction on September 27. Historical implementation evidence and verification limits remain in the [repair handoff](docs/performance-repairs-2026-09-17.md) and [hardware test procedure](docs/performance-validation.md). Closure does not assert additional hardware or long-session testing.
- [ ] **P1 — Remaining Chase Venom rendering defects ([#3](https://github.com/GTTeancum/OpenSpideyPS1/issues/3)).** A player has now completed the chase normally (September 19); do not spend more time trying to prove completion with the position-following fixture. Fix the confirmed visual defects: Venom's disappearance fade is wrong, and the chase meter splits once it extends. The meter has a specific candidate cause: the widescreen HUD classifier rejects panels wider than one quarter of the 320-pixel draw area, so a long meter can have transformed and untransformed pieces. Establish the fade's exact primitive/blend path before changing it. Source matching traces part of the suspected spikes, including long strips, to unchanged original level meshes; do not remove authored scenery or assume actor corruption. Preserve the verified script-specific cadence and audio repairs; see the source-matching and ordinary-route evidence in the repair handoff.
- [x] **P1 — Character-rig retargeting and shoulders.** Closed by user direction on September 27 following the preserved-rig suit conversion work.
- [ ] **P1 — Retry and Pause menu rendering.** Diagnose and correct flickering and any other draw defects in the Retry and Pause menus. Verify the actual menu backgrounds, text, transitions, and input-state changes after the fix.
- [ ] **P2 — [#4: Add SFD Dreamcast FMV support](https://github.com/GTTeancum/OpenSpideyPS1/issues/4).** Use optional Dreamcast SFD videos when present, retaining PS1 videos as the fallback. Deferred until performance work is addressed.

Historical implementation evidence and verification limits: [Timing and audio repairs](docs/timing-and-audio-repairs.md). Additional SM2 work: [Spider-Man 2 to-do](spiderman2/TO_DO.md).

Active goal (September 27): fix Chase Venom's fade and splitting meter, and
Retry/Pause menu rendering. Verify native visual output and menu transitions;
preserve accepted timing/audio and suit work. Commit completed fixes without pushing.

Goal progress: the chase meter now uses a shared transform for its rail, caps
and markers, verified in native widescreen captures with a 4:3 comparison.
The earlier 320-pixel/large-panel hypothesis above is superseded by the measured
512-pixel segmented-rail diagnosis in [P1 rendering evidence](docs/rendering-p1-2026-09-27.md).
Venom's fade and menu defects remain open; candidate builds are not final staging.
