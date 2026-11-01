# To-do

Synced with GitHub issues on November 1, 2026 ([all issues](https://github.com/GTTeancum/SpideyPS1recomp/issues)).
Local status notes describe this repository only.

## Open GitHub issues

### Bugs

- [ ] [#11: SM2: reported crash before the Shocker encounter](https://github.com/GTTeancum/SpideyPS1recomp/issues/11) — notes: [issue-11-shocker-crash](docs/issues/issue-11-shocker-crash.md).
- [ ] [#15: SM2: Stan Lee narration reportedly repeats during costume selection](https://github.com/GTTeancum/SpideyPS1recomp/issues/15) — fixed locally (narration plays once per selection); not yet confirmed by the reporter or closed.
- [ ] [#16: SM2: reported white line obscures half the screen in level-two four-thug encounter](https://github.com/GTTeancum/SpideyPS1recomp/issues/16)
- [ ] [#18: SteamOS/Wine/Proton: reported startup crash after ROM import](https://github.com/GTTeancum/SpideyPS1recomp/issues/18)
- [ ] [#19: SM1 on Linux: diagnostic logs stop while captures and snapshots continue](https://github.com/GTTeancum/SpideyPS1recomp/issues/19)
- [ ] [#20: Reported rainbow horizontal bands on world textures while custom player renders correctly](https://github.com/GTTeancum/SpideyPS1recomp/issues/20)

### Enhancements

- [ ] [#4: Add SFD Dreamcast FMV support](https://github.com/GTTeancum/SpideyPS1recomp/issues/4) — use optional Dreamcast SFD videos when present, keeping PS1 videos as the fallback.
- [ ] [#6: Linux port](https://github.com/GTTeancum/SpideyPS1recomp/issues/6)
- [ ] [#9: Add N64-styled stencil shadows](https://github.com/GTTeancum/SpideyPS1recomp/issues/9)
- [ ] [#21: 60 FPS interpolated](https://github.com/GTTeancum/SpideyPS1recomp/issues/21) — Windows and Linux only; interpolate drawing without changing the game clock.

Community report details: [community-reports-2026-10-29](docs/issues/community-reports-2026-10-29.md).

## Closed GitHub issues

#1 Poor performance, #2 No dialogue or title music, #3 Framerate variance, #5 Game crash, #7 Project openness.

## Local work (not GitHub issues)

- [ ] Suit still unticked in [SUITS.md](SUITS.md): Steel (issue not recalled; needs a look).

## Done

- [x] Native **Video Setup** under **OPTIONS** in SM1 and SM2: 4:3 and 16:9 windowed resolutions from 640×480, plus Fullscreen On/Off. [Implementation and tests](docs/native-video-setup.md).

Historical evidence: [Timing and audio repairs](docs/timing-and-audio-repairs.md), [repair handoff](docs/performance-repairs-2026-09-17.md), [P1 rendering evidence](docs/rendering-p1-2026-09-27.md). Additional SM2 work: [Spider-Man 2 to-do](spiderman2/TO_DO.md).
