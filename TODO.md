# To-do

## GitHub issues
Current work, actual verification results, and the next blocker are recorded in
[CHECKPOINT.md](CHECKPOINT.md). The completed items below describe earlier batches;
they do not establish full campaign verification or resolution of newer reports.

- [x] Add native **Video Setup** under **OPTIONS** in both SM1 and SM2, with **4:3 and 16:9 windowed output resolutions** starting at **640×480**, plus **Fullscreen On/Off**. Preserves each game's menu look and controls. Verified Apply, cancel, restart persistence, fullscreen transitions, exact windowed output areas, and temporary menu allocation cleanup. [Implementation and tests](docs/native-video-setup.md).

Local task status, updated September 29. GitHub issue state is unchanged.

- [ ] **P2 — [#4: Add SFD Dreamcast FMV support](https://github.com/GTTeancum/OpenSpideyPS1/issues/4).** Use optional Dreamcast SFD videos when present, retaining PS1 videos as the fallback. Deferred until performance work is addressed.

Historical implementation evidence and verification limits: [Timing and audio repairs](docs/timing-and-audio-repairs.md), [repair handoff](docs/performance-repairs-2026-09-17.md), and [P1 rendering evidence](docs/rendering-p1-2026-09-27.md). Additional SM2 work: [Spider-Man 2 to-do](spiderman2/TO_DO.md).

User-test folders: `proof_render/p1-user-test-2026-09-27/Spider-Man` and
`proof_render/p1-user-test-2026-09-27/Spider-Man 2`. Both include local disc data
and published mods. No new performance, audio or rig acceptance is claimed.
No push or GitHub issue-state update was requested or performed.
