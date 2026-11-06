# To-do

Synced with GitHub issues on November 2, 2026 ([all issues](https://github.com/GTTeancum/SpideyPS1recomp/issues)).
Local status notes describe this repository only.

## Open GitHub issues

### Bugs

- [ ] [#18: SteamOS/Wine/Proton: reported startup crash after ROM import](https://github.com/GTTeancum/SpideyPS1recomp/issues/18) — needs more info from the reporter (game, build, logs/screenshot)
- [ ] [#20: Reported rainbow horizontal bands on world textures while custom player renders correctly](https://github.com/GTTeancum/SpideyPS1recomp/issues/20) — needs more info from the reporter (game, build, logs/screenshot)

### Enhancements

- [ ] [#21: 60 FPS interpolated presentation on Windows and Linux](https://github.com/GTTeancum/SpideyPS1recomp/issues/21) — implemented and pushed; stays open until a live Chase Venom playthrough confirms Venom can still be followed. Notes: [issue-21-60fps](docs/issues/issue-21-60fps.md).
- [ ] [#22: Original Xbox port (NXDK)](https://github.com/GTTeancum/SpideyPS1recomp/issues/22) — both games as one compilation; RecompOne emits C, no 60 FPS, 480i/480p/720p, QoL and extra suits kept. Started November 3, 2026: bring-up done (nxdk build boots in XEMU, isolation check). Plan and status: [xbox/README.md](xbox/README.md). Rough budget: 50–60 working hours for both games.
  - [ ] Review: texture projection warping still visible on real Xbox hardware (level 1, 720p widescreen). Find which surfaces still warp, and whether they draw affine (no GTE depth recovered) or perspective with a wrong depth.

Community report details: [community-reports-2026-10-29](docs/issues/community-reports-2026-10-29.md).

## Local work (not GitHub issues)

- [ ] Default Spider-Man model shows a seam on the head: reported November 2, 2026 during the same Chase Venom run, visible from behind.

Historical evidence: [Timing and audio repairs](docs/timing-and-audio-repairs.md), [repair handoff](docs/performance-repairs-2026-09-17.md), [P1 rendering evidence](docs/rendering-p1-2026-09-27.md). Additional SM2 work: [Spider-Man 2 to-do](spiderman2/TO_DO.md).
