# Community reports: v1.1 discussion

Reviewed: 2026-10-29 (Eastern Time). Source baseline: `bac45571a5853dc8e84f993087fc0c558bfdeb0d`.

## Evidence and status

Source: [user-linked comment](https://www.reddit.com/r/decomps/comments/1wth8os/comment/pcu6yf0/)
and the [surrounding release discussion](https://www.reddit.com/r/decomps/comments/1wth8os/openspideyps1_v11_released_spiderman_sm1_enter/).
The indexed discussion was readable; direct page retrieval failed. This intake
covers the available text, not verified comment IDs, screenshot contents, or a
complete live thread. Reports occur in the v1.1 discussion dated September 29
through October 2. Individual executable hashes and installation states are unknown.

**All entries below are reported and awaiting reproduction on the current build.**
The IDs are tracking keys in this document, not GitHub issue numbers. Priorities
are proposed engineering order: P1 for core crashes, P2 for functional/visual
regressions and compatibility investigation. No cause or fix is inferred from
similar symptoms alone.

## Reproduction queue

| ID | Priority | Game | Reported symptom | Existing coverage / disposition |
|---|---|---|---|---|
| R01 | P1 | Unspecified in two-game context | Spider-Armor pickup crashes with custom models. | New case; separate from the L5A3 overflow in [#5](https://github.com/GTTeancum/SpideyPS1recomp/issues/5). |
| R02 | P1 | SM2 | Crash before Shocker encounter. | New case. Identify archive and transition before associating it with historical model-initialization failures. |
| R03 | P2 | Both | Health-icon/model graphical corruption. | New case. Establish which on-screen object is affected. |
| R04 | P2 | Both-game context | Enemy texture corruption. | New case; compare with R05 without assuming shared ownership or cause. |
| R05 | P2 | Both-game context | Suit texture corruption. | Existing [#8](https://github.com/GTTeancum/SpideyPS1recomp/issues/8); retain this source as added evidence, not another issue. |
| R06 | P2 | SM1 | Scorpion introduction skipped. | New case; [#4](https://github.com/GTTeancum/SpideyPS1recomp/issues/4) concerns optional Dreamcast FMVs. |
| R07 | P2 | SM2 | Stan Lee narration repeats during costume selection. | New case; closed [#2](https://github.com/GTTeancum/SpideyPS1recomp/issues/2) concerns different audio symptoms. |
| R08 | P2 | SM2 | White line obscures half-screen in level-two four-thug encounter. | New case. Closed [#3](https://github.com/GTTeancum/SpideyPS1recomp/issues/3) describes an SM1 web-line symptom, not this encounter. |
| R09 | P2 | Confirm game | Stock symbiote lacks infinite webbing. | New gameplay case; keep stock behavior distinct from mod inheritance. |
| R10 | P2 | Confirm game | SteamOS/Wine/Proton startup crash after ROM import. | Related to [#6](https://github.com/GTTeancum/SpideyPS1recomp/issues/6), which reports successful launch with choppy menus; preserve this different failure stage. |

## Proposed checks and closure evidence

These are test plans, not assertions that the reporter performed them.

- **R01:** Compare the same route with default, stock alternate, and two custom
  actors. Record the precise failing transition and stack. Exercise activation,
  damage, expiration/reversion, restart, and a subsequent level transition.
- **R02:** Record the last loaded archive, exception stack, selected actor, and
  whether entry was through campaign progression or a level override. Acceptance
  includes the preceding transition, encounter entry, and retry.
- **R03–R05:** Preserve the exact pack/version and selection sequence. Capture
  equivalent scenes before and after a switch. Distinguish world objects, HUD,
  geometry, palette/UV data, and texture ownership. Check a clean install and a
  stock-asset control before attributing the problem to custom content.
- **R06:** Enter through normal progression with no held input, capture the event
  sequence, and compare against supported retail behavior. Inspect skip flags,
  video/stream events, and timer behavior only after the missing segment is
  identified. Direct-level entry alone does not establish cinematic correctness.
- **R07:** Compare entering, moving through, selecting, and leaving the menu;
  record audio start/stop events. Acceptance includes rapid navigation and reentry.
- **R08:** Capture the exact route and a short preceding sequence at 4:3 and 16:9.
  Compare projection, primitive, and clipping evidence. Do not classify it as
  timing, widescreen completion, or a texture fault without a reproduction.
- **R09:** Use the native selection path and record the effective stock slot,
  ability bits, and resource counter while exercising the ability. Repeat after
  restart. A visually matching model does not establish the selected power set.
- **R10:** Record game/build, SteamOS and compatibility-layer versions, GPU driver,
  prefix state, and install/launch logs. Separate importer completion from host
  initialization and first render. Compare first launch and relaunch. Shared
  Ubuntu CI does not establish native Linux or Proton gameplay compatibility.

For every case, retain the executable commit/release, supported disc revision,
mods/settings, minimal route, expected versus actual behavior, and evidence.
Retail files, saves, credentials, private paths, and memory dumps remain private;
only appropriately redacted findings belong in public tracking.

## Related work and boundaries

- [Current checkpoint](../../CHECKPOINT.md): #5 has an isolated native reproduction
  and bounded repair, but still needs gameplay acceptance. #8 remains unattributed.
- [Texture parser and allocation changes](https://github.com/GTTeancum/SpideyPS1recomp/commit/b7e02a1af4ed2670c3aa54a08e850e7accad944b)
  are relevant to investigation, not proof that R01/R03/R04/R05 are resolved.
- [SM2 open-work notes](../../spiderman2/TO_DO.md) list historical pre-render failures
  at `e3m1`, `e3m2`, `e4m1`, and `e5m2`. The location in R02 is not mapped to those
  prefixes by the available report. Historical level-entry captures do not close
  any current gameplay report.
- Existing timing/audio fixes, closed issue states, and source-level ability
  configuration are leads for comparison. A newer report needs its own build
  identity and reproduction before being labeled a recurrence of a known defect.

## Next action

Supported private retail input and full Linux builds are now available. Current
native launch observations and CI results are recorded in
[the checkpoint](../../CHECKPOINT.md); they do not reproduce or close these
reports. Test the previously repaired L5A3 route plus R01 and R02 first, and keep
the remaining targets visible through campaign, costume, audio, renderer, and
compatibility testing. The original intake was documentation-only.
