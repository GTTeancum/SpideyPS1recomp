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
  - [x] Review: texture projection warping still visible on real Xbox hardware (level 1, 720p widescreen). Cause: the C output carried no GTE provenance, so depth came from a screen-point lookup (another vertex's at a collision) and corners were whole pixels (slivers between split faces on far buildings). Fixed 2026-11-06 with vertex tags (xbox/runtime/vtag.c); the warping fix is confirmed by the user.
  - [ ] Building seams (2026-11-07): caused by corners losing their exact position (Xbox-only rules, fixed to match the PC; plus two-half coordinate writes, fixed in both runtimes). Lost corners in the level 1 test went from ~10,500 to under 500 (all behind the camera); the long seams are gone in XEMU. Left: one dotted line on a rooftop floor in one test frame (not from lost corners, cause unknown) and a 1-pixel line between a building edge and the fog beside it. Not yet checked on the console or by playing the PC build.
  - [ ] Xbox build: header changes do not rebuild the generated code (nxdk's dependency files use Windows paths make cannot match); delete the objects by hand after changing runtime/recomp.h.
  - [x] Xbox sound (2026-11-07): sound chip port (xbox/runtime/spu.c), CD-XA music streaming, controller input. Title music matches the PC recording in XEMU; voices sound in gameplay. Title music also confirmed on Windows and Linux (recordings). Speed cost of the sound mixing not measured on hardware.
  - [ ] Linux: the title screen captures black in the headless WSL setup (music plays; the copyright screen before it draws). The older Linux build does the same, so not a recent regression; needs a check on a real Linux desktop.
  - [ ] Speed on hardware (level 3, Miles), 2026-11-07: commit 70b950f ran out of memory (texture allocations failing) and fell to 24 ticks/s; tag work in every load/store cost ~20 ticks/s (51 with it compiled out). Fixed: tags only in GTE functions, 12-byte tags, smaller XA ring (10.4 MB XBE, 2.4 MB free in XEMU, 99.6% of world polygons precise). Not yet measured on the console: it locked up during the 16 MB inline experiment and needs a power cycle.

Community report details: [community-reports-2026-10-29](docs/issues/community-reports-2026-10-29.md).

## Local work (not GitHub issues)

- [ ] Default Spider-Man model shows a seam on the head: reported November 2, 2026 during the same Chase Venom run, visible from behind.

Historical evidence: [Timing and audio repairs](docs/timing-and-audio-repairs.md), [repair handoff](docs/performance-repairs-2026-09-17.md), [P1 rendering evidence](docs/rendering-p1-2026-09-27.md). Additional SM2 work: [Spider-Man 2 to-do](spiderman2/TO_DO.md).
