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
  - [ ] Distant-building gaps: the remaining specks were hairline cracks (undrawn pixels showing older frame content). Opaque world triangles are now drawn 0.6 px larger; the magenta hole test (`geometry_debug=2`) shows none left on buildings at 480 and 720p in XEMU. Awaiting the user's check.
  - [x] Xbox sound (2026-11-07): sound chip port (xbox/runtime/spu.c), CD-XA music streaming, controller input. Title music matches the PC recording in XEMU; voices sound in gameplay. Title music also confirmed on Windows and Linux (recordings). Speed cost of the sound mixing not measured on hardware.
  - [ ] Linux: the title screen captures black in the headless WSL setup (music plays; the copyright screen before it draws). The older Linux build does the same, so not a recent regression; needs a check on a real Linux desktop.
  - [ ] Measure the vertex-tag speed cost on hardware (level 3, Miles); 15% of textured polygons still arrive untagged and draw affine.

Community report details: [community-reports-2026-10-29](docs/issues/community-reports-2026-10-29.md).

## Local work (not GitHub issues)

- [ ] Default Spider-Man model shows a seam on the head: reported November 2, 2026 during the same Chase Venom run, visible from behind.

Historical evidence: [Timing and audio repairs](docs/timing-and-audio-repairs.md), [repair handoff](docs/performance-repairs-2026-09-17.md), [P1 rendering evidence](docs/rendering-p1-2026-09-27.md). Additional SM2 work: [Spider-Man 2 to-do](spiderman2/TO_DO.md).
