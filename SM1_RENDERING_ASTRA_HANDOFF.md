# Stock SM1 rendering investigation — Astra handoff

## SM2 follow-up: exact homogeneous projection verified and staged (2026-09-08)

Committed and pushed as 48b96eb on origin/master (following e62d7ba and a0dffe5). Automatic approval review rejected cleanup of redundant SM2 test files with "blocked by policy"; no cleanup occurred and no alternative deletion was attempted.

SM2 now uses shared subdivision and frustum hooks; default backdrop pixel repetition is disabled. Native routine addresses and tests are documented in docs/sm2-rendering-repairs.md. A remaining warehouse floor crack survived the first repair and a hook-routing correction. The actual remaining cause was rounded camera IR/SZ and approximate reciprocal in the host projection. GTE host X/Y/Z now use the same unrounded homogeneous transform; native screen words/register calculations stay unchanged. Rotated-edge regression fails before and passes afterward in both games. The separate local-hook execution test passes, including RA/stack restoration.

Native GPU proof: proof_render/sm2-render/index.html. Same warehouse camera/character pose, interior-mask/frame_04498.png before versus exact-mask/frame_04498.png after. Floor ROI x340..599/y645..704: 363 nonwhite pixels before, zero afterward. Both subsequent mask frames clean. Four normal warehouse, two outdoor widescreen and two 4:3 warehouse captures individually inspected. Normal gameplay about28–29.5FPS. No videos or host input. This verifies reproduced defects and sampled views, not all levels/frames.

SM2 staged EXE SHA256 F9D75D82B830B6D504667BF28918D1AC7BCE22AEC8B196B73A66D9D7B8B02462 at proof_render/user-facing-stage/ready/Spider-Man 2/SpiderMan2.exe. EXE-only copy, source/destination hash matched; settings, carda, cardb, interface hashes preserved. SM1 staged executable was not replaced by this SM2 follow-up. Its shared-source regression passes.

## Latest verification: user's Iron Spider mod

Tested the unchanged staged build `66664E4242136772A8E6DFE34220FF8208D04007AF8C52EAC8B589AEFDC679A9` with the user's installed Iron Spider suit (Hollywood-SN, spiderman model, unlimited ability profile). Copied its nine files (159,465 bytes) into repair/mod-test-suits; all hash-identical to installed originals. SPIDEY_SUIT_MOD_DIR and SPIDEY_COSTUME=ironspider target only the test copy, leaving user selection/settings/mods untouched. Log confirms activation and all eight external texture bindings.

Native captures in `proof_render/sm1-visual-ab/repair/ironspider`: frame_05006 (spawn), frame_05226 (jump), frame_05306 (landing crouch), each with an exact crop. All three full frames and three crops inspected: no visible floor holes or joint separation in these poses. Jump issued solely by the process-local harness at5200. Stills/crops total5,424,670 bytes; no video or duplicate executable. `repair/log-ironspider/spidey.log` exits at5620 with zero span rejects; late57.7–58.8 vblanks/sec (~28.9–29.4 actual FPS), game33.7–34.3ms. This verifies the sampled poses, not every animation frame. No code/build/stage change needed.

## Latest: spawn floor holes and thin joint faces repaired, verified, staged

2026-09-08: User reported large yellow openings in the spawn floor and small gaps at Spider-Man's joints. Staged new EXE SHA256 `66664E4242136772A8E6DFE34220FF8208D04007AF8C52EAC8B589AEFDC679A9` to `proof_render/user-facing-stage/ready/Spider-Man/SpiderMan.exe`. Destination hash matches candidate. Only EXE copied; carda/cardb/settings/interface hashes preserved, assets and mods untouched. No game process left running and no Git staging.

Floor repair: Gte.Rtp now retains true host projection when positive depth is below the native reciprocal saturation threshold H/2. Native SXY/divide/flags remain intact; NativeScreen certifies the native word separately. WorldSubpixel.CertifyClamp accepts that certificate through native subdivision clamps. Before capture `repair/spawn/frame_05000.png` reproduced the holes; after `frame_05002.png` shows continuous foreground floor.

Joint repair: NCLIP uses precise signed area only when integer rounding erases or reverses winding, retaining native magnitude otherwise. Precision validity now travels explicitly with the GTE screen FIFO and tagged writes; depth-only tags no longer become false precise (0,0) coordinates. Source joint audit checked 58,254 references with no mismatches; stock topology has no open edges. No model/color changes. Opt-in RECOMP_SOLID_GEOMETRY_CLUT isolates actual coverage from texture transparency: before mask `repair/spawn/frame_05004_closeup.png` has holes at both knees; after masks 05006/05026/05046 have connected joints (different poses, not exact matched-frame comparisons). All inspected. Diagnostic mask is inactive on normal launches.

Final normal-textured native captures in `proof_render/sm1-visual-ab/repair/spawn-final/`: frame_05006.png, frame_05046.png, frame_05606.png, plus exact native crops. All three full images inspected: foreground floor continuous and player joints closed. `repair/log-final-spawn/spidey.log` exits normally at5620; later measured56.3–58.8 vblanks/sec (~28.2–29.4 actual FPS against target30), game33.8–35.1ms, zero span rejects. Only three stills, no concurrent compilation/encoding. These captures establish the reproduced spawn defects, not every pose/level or the original brief darkening complaint.

RenderingRegression passes including near reciprocal/native-word preservation, both thin-triangle winding directions and depth-only provenance; see `joint-cull-regression.log`. Latest default publish succeeded in `publish-joint-cull.log`. Existing perspective/subdivision/frustum regression cases also pass. Prior cleanup denial below remains unresolved; no disk space claimed freed and no alternate deletion workaround attempted. Reused repair folder and small targeted stills; no new video capture.

## Latest: staged for user testing; cleanup blocked by automatic approval review

User explicitly requested staging for testing and deletion of proof bloat after reporting poor capture-run FPS. Staged verified EXE `0BA2299FC19F174DD5C9DD0AC834018D9BED416801B6EA60FBC1668F86269722` to `proof_render/user-facing-stage/ready/Spider-Man/SpiderMan.exe`; all687 installed builtin assets hash-verified against candidate manifest. Existing carda/cardb/settings/interface hashes preserved; mods untouched. This is a user-requested test build, not a declaration that all original4 defects are proven resolved. No Git staging performed.

Canonical `spiderman/port/bundled/runtime-assets.zip` now contains the verified candidate payload, eliminating reliance on temporary publish override. Compared before copying: same entry set, only henchman.psx/henchngt.psx/thug.psx/bundle.json differ. Default future builds retain the3actorLODrepairs.

Cleanup NOT completed: automatic approval review rejected both a guarded deletion of temporary sm1-* proof directories and a narrower guarded deletion of the exact `proof_render/sm1-visual-ab` directory. Both returned only `blocked by policy`; no reason given. Do not claim any disk space freed. The visual-ab directory is about3.05GiB, with older temporary SM1 directories also remaining. Never delete user-facing-stage: it holds the staged installation and user's disc/saves/settings. All game/encoder processes finished. Dense PNG proof capture caused9–14actualFPS; clean candidate measured27.5–29.2FPS against target30. Future video encoding only after game exit.

## Latest repair: marked seam and roof opening verified; proof capture FPS overhead resolved

See `proof_render/sm1-visual-ab/REPAIR-VERIFICATION.md` for evidence and limitations. Latest candidate `repair/SpiderMan.exe` SHA256 `0BA2299FC19F174DD5C9DD0AC834018D9BED416801B6EA60FBC1668F86269722`; nothing staged. Original stage remains8B5245.

Implemented projective subdivision reconstruction in WorldSubpixel: prehook8007D2D8 captures original precise triangle corners; scratch GTE subdivision weights reconstruct homogeneous XY and fractional depth. Gte.ScreenZ and GteScreen.RamDepth.Z now float. Native RAM/CPU output preserved; real subdivision midpoint regression passes. This removes the marked dotted facade crack in matched capture.

Implemented WorldFrustum pre/posthook8007B1B4: native object selector uses separately computed 4:3 planes, unaffected by GTE projection widening. Color-matrix rows1/2 are left/right inward planes; combine sum/difference to scale horizontal component by FovNum/FovDen, normalize to4096, run native sphere/AABB tests, restore5controlwords. This restores missing mesh118 roof face0x8013C904 at the marked right-side opening and brings buildings into widened margins earlier during camera turns. Tests verify margin admission, horizontal/vertical/behind-camera rejection, central visibility and exact matrix restoration. Hooks in both config/mkconfig; regenerated main, no manual generated edits.

All five repaired stills inspected; matched roof frame03020 in repair/shots. 601-frame native clips: before-motion.mp4 (original stage), repair-motion.mp4, comparison-motion.mp4 (before left), blackcat-motion.mp4. Capture offsets300..1500 and1800..3000 step2, at30fps. Inspected eleven extra intro checkpoints/seven later Black Cat checkpoints, not every frame. Do not claim universal absence of seams/warping/pop-in across every level. Original four-defect gate and brief player-darkening cause remain unclosed; user steered away from color data.

User reported atrocious FPS during dense proof capture/encoding. Clean same-candidate run `repair/log-performance` with no capture/trace/encoding: rooftop58.1/58.3vblanks/sec, BlackCat54.9 (~27.5–29.2 actualFPS; native game target30), versus dense capture18–27vblanks/sec (~9–14FPS). Game-frame work34ms vs80–99ms. Stop synchronous PNG capture and concurrent encoding for ordinary play; future encoding only after game exit. Native proof captures can heavily perturb performance. All game/encoder processes finished after verification; do not launch overlapping SpiderMan instances.

Opt-in WorldGeometryTrace (sources) and GeometryTrace (triangles) retained. Hardcoded roof transform diagnostics only active with RECOMP_GEOMETRY_DUMP and frame window. `repair/geometry.jsonl` baseline beforefrustum source3034; `repair/frustum.jsonl` afterfrustum source3018. `verify_roof.py` confirms missingface0→1 and actual nearroof coverage. `project_world.py` projects original meshes by fit to tagged source coordinates; it is diagnostic inference, not exact native replay. `run.ps1` now resets copied cards/settings before eachrun and supports Motion, BlackCat, Performance. Dense output is under ignored proof_render. Original assets intact; publish MUST retain candidate-runtime-assets.zip as BundledAssetPayload to preserve prior3actorLODfixes.

## Latest: native visual proof requested; seam gate FAILED (2026-09-08)

User supplied annotated screenshot identifying facade dotted crack and right-side roof opening. Added opt-in `GeometryTrace` at actual HLE triangle submission (`RECOMP_GEOMETRY_DUMP`, START/END absolute VSync range), published diagnostic-only EXE to `sm1-visual-ab/trace`. No rendering fix in this step. Second trace run exited normally at frame3368; do not assume global frames: archive offset varies (latest capture3058). Geometry JSONL covers2800..3100; analysis script selects latest submitted frame before fourth capture (3056 before3058), and corrects double-buffer offset independently of the letterbox clip30. At roof point(500,185), only distant background TPage40/Clut165 covers it; nearby(475,185) also has roof TPage56/Clut418. Thus roof coverage is missing at that sample, while background geometry is present. Renderer span rejects0. Earlier preliminary trace used mismatched global camera and found0 world coverage; do not cite that as exact-frame proof. Facade-edge neighboring materials are TPage174/Clut1696 and175/Clut1632. Root cause still unproven; tracing supports investigating upstream roof geometry and shared face boundaries. `trace/geometry-analysis.txt` retains triangle evidence. Diagnostic build passed; no staging.

User explicitly requested visual proof, superseding earlier no-image restriction for this verification. Native Capture.cs GPU readback only; no desktop/UI control. See `proof_render/sm1-visual-ab/REPORT.md`, five matched still pairs (all ten inspected). Before staged hash8B5245 / candidate0ACC39; both archive anchor1842, shots2142/2442/2742/3042/3342, normal exit3352. Separate copied folders, nothing staged, both processes exited.

Actual captures show local texture-line improvement and removed distant actor spikes, BUT candidate still has visible world seams: frame3042 right rooftop large bright opening, background facade dotted bright crack near x1070–1300/y315; frame2742 facade cracks; frame3342 upper-left roof-edge discontinuities. Do not claim geometry repairs fully verified or that only a darkening clarification blocks progress. The new visual evidence provides an actionable reproduction; investigate world boundary/subdivision/widescreen geometry next, not player color data. Still pairs do not establish absence of temporal affine warping. Previous blocked assessment below is superseded by this new evidence.

## Current focus: warping, edge separation, widescreen (user steering)

Latest candidate supersedes the earlier offset-following hook:
- Requirement-by-requirement audit is now `sm1-astra-package-check/VERIFICATION.md`.
  Candidate hash and unchanged staged hash revalidated; installed stock spidey.psx
  byte identity against original builtin manifest revalidated. No game process live.
  All available geometry/startup/package checks are complete. The remaining gate is
  a repeatable event or sufficiently specific text description of brief darkening;
  counts of changed compositor pixels do not identify it, and new runs of the same
  geometry checks cannot establish its cause. Pending text clarification unanswered.
  The same missing-reproduction blocker persisted for three consecutive audit
  turns after independent work finished. Third turn revalidated no new evidence
  or live process and marked the goal BLOCKED. Candidate is retained; nothing
  staged. Resume when the user supplies a usable darkening reproduction description
  or changes the gate. A resumed goal starts a fresh blocked audit.
- LATEST PACKAGED EXE: SHA2560ACC3917556403890CC8DD7432F7625DEC390D13F3DE3B3AC48ED784862D9212
  in sm1-astra-package-check. `log-full-startup-gameplay` finished frame6000,
  PID46236 exited, no boot skip, process-local title inputs, normal asset loader.
  Actual GL flag before=True/after=False confirms no-dither correction. Opening
  plus gameplay mixer delivered1840378 XA samples. 42772 actor stitch checks:
  no native/projection mismatches,missing projection,premature reads. Final
 1694844/1694844 world fractional vertices,mixed-depth-prims0,zero loss log entries.
  Summary JSON in that log folder. Run slowed temporarily but recovered; CallRing's
  'stall breaks' counter calls IdleTick to service native polling, does not skip
  native control flow, and also increments in prior unskipped startup runs.
- Pending optional text clarification asks whether brief darkening was confined
  to separating edges or affected a larger suit area. No answer received yet;
  do not assume one or restart color tracing. All-four staging gate still holds.
- Unskipped startup preservation check completed frame4000 (PID42908 exited):
  `sm1-astra-package-check/log-unskipped-startup`, script0/no boot-skip/shots0.
  Mixer reports1263922 XA samples across opening intervals, peaks9892/22055/29098,
  RMS1435/3517/3186. Opening streaming produces nonzero output through normal loader.
- No explicit GL dithering disable existed in hardware renderer. GlCore.InitGl
  now queries initial Dither state, disables it, logs actual before/after state.
  This enforces the user's no-dithering requirement; no claim it caused edge defects.
  Re-publishing same candidate payload to package-check in session77744,
  `publish-no-dither.log`. New EXE supersedes previous candidate hash once build ends;
  needs combined unskipped startup/gameplay run and actual flag verification.
- PACKAGE RUN FINISHED: PID47736 exited at frame4000. All687 installed files in
  sm1-astra-package-check/assets/builtin match candidate manifest hashes. Runtime
  loader selects that local pack with no override variables. Henchman slot8 loads
  meshes15/parts15. 42846 stitch checks: native/projection mismatches0,missing0,
  max screen delta0,premature reads0. Final1353624/1353624 world fractions and
  mixed-depth-prims0. Machine-readable summary: log-native-loader/verification-summary.json.
  Candidate EXE SHA25605A8D543751FE2F089E4D32ACF9350DBD32834C73B7447DEFA0CC450AB5A2B9B.
  User-stage EXE remains8B5245CB2762CE1B03B41F525A16CF399CB5B4116EAED064606223BC84F38764.
  This is a copied-folder normal-loader verification, not a staged replacement.
  The user's brief darkening report is not independently tied to a proven cause;
  do not claim all4 complete solely from these geometry checks or resume color
  tracing against the user's steering. No staging yet.
- Actual Spider-Man stitch verification: `log-actor-shared-vertices` completed
  frame4000,1860 mesh batches/22630 stitched vertices. Native-word mismatches0,
  projected-coordinate/depth mismatches0,missing projections0,max screen delta0.
  Actual actor camera-depth range365..3572. Final renderer1306344/1306344 fractions,
  mixed-depth-prims0. New ModelDiagnostics opt-in layout trace compares each stitch
  to the actual source at outputBase+1F38-parsedByteOffset.
- HENCHNGT/THUG candidates re-audited and copied into isolated override (all3
  repaired actors there now). THUG has16 objects/meshes, not15; HENCHNGT15.
- Packaging verification underway: copied user-stage assets/settings/cards to
  `proof_render/sm1-astra-package-check` (no shots copied). Prepared
  `sm1-astra-lod-isolation/candidate-runtime-assets.zip` by changing ONLY henchman,
  henchngt,thug PSX plus bundle.json. All687 manifest hashes validated; all other
  uncompressed file contents match original bundled ZIP. Original ZIP untouched.
  SpiderMan.csproj accepts optional BundledAssetPayload for isolated payload builds;
  default still uses its original bundled/runtime-assets.zip. Published candidate
  with this property to sm1-astra-package-check (publish-package-check.log).
- PID47736 running package candidate, log-native-loader, exit4000, henchman trace.
  Cleared SPIDEY_ASSET_DIR and RECOMP_ASSET_PACK_DIR so real installer/default asset
  loader runs; saved user-stage settings are used. No staged EXE/assets changed.
  Revalidate process/log and extracted candidate hashes before considering success.
- FINAL FOCUSED RUN: `log-stock-wide-geometry` completed at frame4000 (PID36752
  exited). Final trace retains1306344/1306344 world vertex fractions and reports
  mixed-depth-prims0; whole log has0 subpixel-loss messages,0 composite-audit
  messages despite RECOMP_AUDIT_COMPOSITE=1 (pass is disabled),0 lighting records.
  No exception/fatal/error log entries were found. This verifies the tested intro/
  gameplay route and the concrete mechanisms; it is not blanket visual proof of
  every scene. All original four-defect/exact-user-path staging gates remain.
  Candidate executable is isolated in `proof_render/sm1-astra-lod-isolation`.
  No production actor assets or user-facing staged EXE were replaced.
- `log-precise-edges` completed frame4000 (PID39004 no longer live). Final
  perspective sample:1353660/1353660 fractional world vertices, mixed-depth-prims0,
  no subpixel-packet-loss entries anywhere. Modern4x/FXAA on confirmed in log.
  Consecutive compositor audit covered541 rendered frames157..697: it changed
  5705248 pixels summed across frames,743 already-drawn pixels. This demonstrates
  synthetic side-area changes persist even with repaired geometry provenance.
- Stock Program.cs now calls Wide.Install() without completeBackdrop:true, removing
  boundary-pixel repetition and one-pixel color substitution from stock presentation.
  Projection-based16:9,4x,FXAA remain. New publish `publish-stock-wide-geometry.log`
  running in session24520; needs isolated verification before any broader claim.
- `WorldSubpixel.EdgeExit` now preserves the ORIGINAL projected host position.
  The native integer expansion still occurs unchanged, but applying that expansion
  to precise host geometry displaced shared edges and their texture mapping.
  Tags now carry an optional `NativeScreen` certificate (bit31 presence, low11-bit
  xy expected) so final GP0 validation accepts exactly the known native adjustment
  and still rejects later unrelated coordinate changes. This field is carried by
  CpuContext, RAM, GTE reloads/FIFO, and packet reads.
- `WorldSubpixel.SubdivisionEnter/Exit` scope a RAM-tag transform around real
  func8007D33C. It certifies only matching known X[-510,1022]/Y<=510 software caps
  in scratch grid stores, preserving the true offscreen projection. This must
  happen during stores: the generated function includes tail-return draw paths,
  so merely scanning scratch in a post-hook would occur too late for some packets.
- `software-clamp-regression-corrected.log`: actual native `_Impl` vs hooked
  routine produce identical words/GPRs (nativeC1FE4111), originalY519.5 reaches
  final packet instead of dropping at cap510. Subsequent coordinate edit rejects
  the tag. Initial test omitted S5's coordinate mask; corrected test uses3FFF3FFF.
- `precise-edge-regression.log`: baseline GP0 shared-edge tests fail continuity
  at depths1000 and20000, repaired versions pass even when the adjacent face
  additionally traverses the real edge-expansion routine. Native CPU behavior
  stays identical; host projections stay original. Edge-borrow expected host
  position is now original(0.5391998,130.03665), NOT previous offset position.
- `log-edge-borrow-world` completed frame4000, final sample1354997/1354998
  fractions retained; the sole remaining drop was the software cap now repaired.
- Combined candidate published (`publish-precise-edges.log`) and running as
  PID39004 in `log-precise-edges`, exit4000. Check authoritative process/log state.
  This run enables consecutive numeric composite checks and a new
  `mixed-depth-prims` counter BEFORE world/HUD classification (avoids tautological
  world-only missing-depth counts). No color-data diagnostic enabled.

The user explicitly stopped the color-data investigation and asked to fix texture
warping and polygon-edge separation first, then report. They clarified that edge
separation also affects distant Spider-Man and that widescreen's extended view is
problematic. Do not resume the color investigation. No staging is authorized until
the original all-repairs verification gate is satisfied.

New verified mechanisms/candidates:
- Recompiler MTC2 emitted `Gte.Write` for CPU->SXY transfers, losing depth and
  fractions even when CPU registers retained exact provenance. Added
  `Gte.WriteFrom`, sharing `WriteProjected` with LWC2, and emit it for registers
  12..15. Rebuilt Release recompiler and regenerated SM1 via build.recompile.
  Actual DrawPrimSet now calls WriteFrom(c,12,25), as does another game path.
- `mtc2-regression.log`: all4 SXY destinations retain native words and full tags;
  legacy writes lose depth. `gp0-shared-edge-ab.log`: through actual GP0 decoding,
  SM1 Wide classifier and HLE submission, neighboring faces lose continuity in
  legacy mode and retain identical fractional endpoints/depth in repaired mode,
  at depths1000 and20000. Numeric backend receives vertices only, no images.
- `log-mtc2-world` completed frame4000 (PID11908). Last trace retains
  1207473/1207488 world vertex fractions. Counts cannot be compared directly to
  earlier runs because startup/global-frame timing changes animation coverage.
- This trace reproduced native packed-coordinate borrow at X=0: word0080FFFF
  (-1,128) retained stale projected(0.5391998,130.03665), depth1155. The edge hook
  rejected |dy|=2 because it assumed independent +/-1 halfwords. `WorldSubpixel`
  now accepts carry/borrow only across X=0/-1 and follows actual native dx/dy.
  Exact-routine regression passes with tag(-0.46080017,128.03665), same native
  word0080FFFF. `edge-borrow-regression-corrected.log` is the passing log; initial
  test used the wrong edge-direction input, corrected to T9=0x20000000.
- Edge-borrow candidate publish running in session42271, output
  `publish-edge-borrow.log`; needs fresh isolated runtime verification next.
- Remaining trace includes CPU subdivision clamp Y=510 vs true519.3389 (packet
  41FE01EE) and wider offscreen scratch clamps. Do not equate high retention with
  proof all clipping/warping is fixed. `func_8007D33C` clamps X[-510,1022],Y<=510.
- Widescreen uses projection squeeze within original framebuffer (WideAspect0,
  SourceAspect16:9), but WideComplete still synthesizes uncovered side pixels
  from the authored-view boundary. Do not claim this repairs underlying geometry.


## User clarification and subsequent evidence (2026-09-08)

- The user clarified the Spider-Man artifact: **"It looked like momentary
  darkening"**. Investigate temporal shading/lighting/compositing; do not assume
  detached geometry or make terminology a prerequisite.
- Prior `log-composite-and-unclamped` sampled every30 rendered frames and found
  drawn-changed=0/world-changed=0. That sampling cannot exclude brief darkening.
  The opt-in composite diagnostic now checks every distinct rendered frame.
  Fresh isolated candidate PID37696 completed at frame4000. Text logs are in
  `proof_render/sm1-astra-lod-isolation/log-consecutive-composite`.
  Audited all547 consecutive rendered frames156..702 (no gaps):57 frames have
  drawn changes,741 total changed pixels,max56/frame,all world-covered.
  No non-world-covered drawn pixel changed. This is narrower evidence than an
  actor-specific audit; it does not establish the cause of player darkening.
  Next investigate actor lighting/color packets and temporal shading. DrawPrimSet
  reads color-table base0x800B5914 and uses DPCS/DPCT and DCPL commands; compare
  successive actor submissions, keeping actual actor identity explicit.
  `consecutive-candidate-regression.log` passes, process exit0. Staged EXE hash
  rechecked unchanged. No staging occurred.
- `log-player-geometry` completed exit4000: 2528 audited player face batches,
  zero invalid indices. This does not verify the darkening defect.
- The offscreen UV regression compares interpolation with an independent
  ray/plane intersection. Old clamped result12.415554,11.865585 disagreed with
  expected5.203252,10.406504; unclamped result matches within1e-6.
- `ProjectionFloat` also prevents float rounding from crossing the native
  fixed-point integer boundary (273.999984 becoming274). Regression passes with
  native273/render273.99997 and retained subpixel tag. This latest correction
  is included in the consecutive-composite candidate publish.
- All four defects still require final integrated verification. Older checkpoint
  descriptions of pending publishes and every30-frame audit are superseded here.

## Astra continuation checkpoint (2026-09-08, after 09:09 local)

The active goal is now in task `01a08107-aa77-79c0-9227-e2b6748c4fba`. Work continues;
no staging or bundling has occurred. Staged EXE remains SHA256
`8B5245CB2762CE1B03B41F525A16CF399CB5B4116EAED064606223BC84F38764`.
No screenshots/images were inspected or created. All new runs use the hidden,
process-local route and text diagnostics in `proof_render/sm1-astra-lod-isolation`.
Read this checkpoint before the older hypotheses below.

### Verified thug mechanisms and candidate

- ParsePSX (`func_80074C98`) counts mesh headers with LOD link `+0x1A == 0xFFFF`
  and stores that as model entry `+0x34` (body-part count). The old duplicated tier
  creates **30 parts for the 15-bone henchman**. SM1 follows links per part; the
  older claim that a third tier must be synthesized was incorrect.
- DC reduced-tier stitches reference file-wide source entries 72..133, while SM1
  resets the source buffer per actor. Runtime baseline at caller `0x80077C54`
  reads unwritten entries (mesh16 has prior18, references through89).
- Candidate converter emits exactly the first `object_count` meshes for these
  incompatible actors, with each header threshold32767/linkFFFF. This preserves
  full-detail geometry, skeleton/object records, names, normals, and textures.
  It deliberately avoids reduced stitched LODs, which cannot safely mix source
  counts under SM1's per-part selection algorithm.
- `sm1_far_lod_sources`/`additional_mesh_sources` were replaced by
  `sm1_requires_single_lod`/`sm1_single_lod`; both CLI and port_all callers updated.
  Census: only HENCHMAN, HENCHNGT, THUG qualify. No production asset was replaced.
- Candidate henchman: `single-lod-candidate.psx`, 253344 bytes, SHA256
  `68BE326B9C1D1DC79E667EAD07D4781DC9D691981C3269F32BA4DC5058EF2844`.
- Controlled same-executable/all-other-assets A/B:
  `log-baseline-validated/spidey.log`: 7110 transforms, **15642 premature reads**,
  max per-mesh spans xyz **456,525,5767**, runtime meshes45/parts30.
  `log-single-lod-validated/spidey.log`: 3795 transforms, **0 premature reads**,
  max spans **22,26,80**, runtime meshes15/parts15. Both exit frame3000.
  Counts differ because startup/global-frame timing differs; compare invariants,
  not raw call counts. Source-buffer sequence resets are validated per actor now.
- `spiderman/tools/audit_sm1_actor_lod.py` checks retained mesh bytes and texture
  payloads, normalizing relocated texture-header pointers. All three candidates
  pass (henchman/henchngt 72 sources74 refs; thug68 sources70 refs).
- Discard `log/` first isolation as repair proof: simply disabling all links
  produced45 parts. `log-chain/` fixed the terminal chain and demonstrated compact
  full-detail rendering, then proper15-mesh conversion replaced that experiment.

### Renderer mechanisms and edits under verification

- `GteScreen.LoadU16/LoadU8` discarded fractional coordinates. Now preserve the
  containing word's complete tag; memory overwrite invalidation still applies.
- `GteScreen.StoreU32` prematurely validated intermediate scratch words, which
  SM1 decorates with clipping flags (`0x4000/0x8000` in halfwords). Now preserve
  tags through intermediates and validate in `GpuRaster.DrawPolygon` using
  `ValidatePacketVertex`, matching the actual signed11-bit GP0 coordinates.
  Real coordinate edits still reject stale fractions at packet consumption.
- `func_8007D534` intentionally adds one-pixel x/y offsets along subdivision edges.
  New `WorldSubpixel.EdgeEnter/EdgeExit` hooks retain its exact native memory and
  register behavior and apply the same measured +/-1 translation to sidecars.
  Config and mkconfig updated, generated output regenerated through
  `import build; build.recompile()` (includes costume-viewer/texture-registry fixes).
  Do not hand-edit generated output.
- `tools/RecompOne/tests/RenderingRegression` references the SM1 port and tests:
  whole-word vs halfword shared vertices; clipping-flag round trip; edited and
  overwritten coordinate rejection; actual `func_8007D534_Impl` vs hooked routine
  (identical registers/native word, corrected fractional tag); offscreen projection.
  Old/new logs are `halfword-before/after.log`, `outcodes-before/after.log`,
  `edge-routine-test.log`, `clipping-before/after.log`. Last test suite passed.
- Runtime halfword-only change made little improvement. Final packet validation
  raised fractional retention to ~92%. `log-edge-translation/spidey.log` then
  retained **576907/576912** world vertices (only5 lost) versus ~76% before.
  This is actual GPU submission evidence, not independent proof of all visuals.
- New `Gte.cs` candidate keeps **unclamped** ScreenX/Y sidecars while native SXY
  stays PS1-saturated. Regression: projectedY2168/nativeY1023 previously rendered
  Y1023, now renders2168. This avoids changing visible interpolation when the
  host GPU clips an offscreen endpoint. Runtime candidate verification pending.
- Important remaining lead: SM1 itself also clamps scratch coordinates to510
  (`0x41FE` with outcodes) in subdivision preparation. Final packet validation can
  still reject an unclamped sidecar for such points. Need inspect `func_8007D4A0`
  and related routines; don't assume the GTE clamp fix solves every clipping case.

### Next run / outstanding work

- `GlCore` now has opt-in `RECOMP_AUDIT_COMPOSITE=1`: every30 rendered frames it
  compares native color/coverage buffers before/after widescreen completion,
  logging only counts (`drawn-changed`, `world-changed`), with no image output.
  This investigates the silhouette defect independently: WideCompleteFs uses
  pixel color as a proxy for being undrawn even when coverage says drawn. Do not
  change the shader until runtime evidence supports the mechanism.
- Current publish with GTE-unclamped candidate and numeric composite audit was
  started into `publish-composite.log`. Check completion before running it.
- Runner: `proof_render/sm1-astra-lod-isolation/run-text.ps1 -LogName <unique>`.
  It asserts no SpiderMan exists, clears capture/dump env vars, sets explicit
  full override directory and fixed pack root, starts hidden and returns PID.
  Set `RECOMP_AUDIT_COMPOSITE=1` in its parent terminal and copy the new published
  EXE to the trial root first. Override currently contains only henchman repaired;
  henchngt/thug candidates exist separately and passed byte audits.
- `ModelDiagnostics` now logs parsed part count and per-transform caller/output/
  source addresses, prior-source count, references, and premature reads.
- Affine warping, seams, and silhouette still require final independent proof;
  no claim that all four user defects are resolved. No bundle/stage until all
  verification and exact user-path run are complete.

Paused at the user's request on 2026-09-08. The user will create a separate Astra chat. No game process was running at the pause. No screenshot or image context is included here. This task is unfinished; none of the four reported visual defects has been demonstrated fixed.

## User objective and constraints

Take a very deep dive and correctly fix all four defects: affine texture warping, world polygons separating at seams, artifacting around Spider-Man's silhouette, and distant thugs exploding into separated polygons during the intro/gameplay. The user tested baseline Spider-Man and reports ZERO visible improvement from previous staged builds. Treat that as ground truth. They explicitly authorized several hours of work and asked to set a goal.

- Workspace: `C:\Programming\GitHub\OpenSpideyPS1`; PowerShell.
- No screenshots or image context. No desktop/UI automation, capture app, host keyboard/mouse/controller injection. Use code, files, logs, numeric evidence, and process-local game input scripts only.
- One SpiderMan game process at a time. Ordinary terminal launching/monitoring is authorized. Start-Process helpers use `-WindowStyle Hidden`.
- Preserve modern renderer 4x, FXAA, no dithering, widescreen, opening audio, baseline stock Spider-Man.
- After proof, replace only `proof_render/user-facing-stage/ready/Spider-Man/SpiderMan.exe`. Do not stage another speculative build.
- Dirty worktree contains substantial user/prior work. Preserve unrelated changes. Use apply_patch for source edits.
- Do not spawn subagents unless user explicitly authorizes delegation.
- Keep communication direct. Do not preface corrections with praise or 'You're right.'

## Goal state

An ACTIVE goal was created in source task `01a07f30-a130-7823-a242-90fe1c6f77e8`. It requires independent reproduction and proof of all four defects, deterministic text/numeric regression evidence, old/new primitive or framebuffer differences, runtime address validation, and an exact user-path run before staging. It has no token budget. It remains active because the user requested a pause, not completion. Tools cannot pause goals. A new task may need its own goal if this state does not transfer.

## Ground truth from the user's real run

`proof_render/user-facing-stage/ready/Spider-Man/spidey.log` contains the user's approximately 08:09 run. It proves intended binary/settings/assets were used:

- Staged exe SHA256: `8B5245CB2762CE1B03B41F525A16CF399CB5B4116EAED064606223BC84F38764` (91,565,351 bytes).
- GL45, modern 4x, FXAA on, perspective correction on.
- Correct staged texture pack root and baseline `spiderman <- spiderman` ability config.
- LevelIntro exited, converted `henchman.psx` loaded (279,928 bytes), four thug actors spawned.
- User still sees all defects. Do not blame wrong binary, mods, or wrong assets.

Previous staged changes (still in worktree) propagated subpixel tags via CpuContext.SetDerived, validated GteScreen packed coordinates, used RenderX/Y in GpuHleForward.HV, premultiplied replacement texture alpha, classified world draws in 4:3, duplicated actor far LOD meshes, and updated bundled assets. Builds/traces passed, but user-visible results did not. These changes must be audited rather than assumed effective.

## Strong leads — hypotheses, not established root causes

### Actor LOD layout and stitch references

Dreamcast HENCHMAN has 15 objects / 30 meshes. Retail SM1 henchman has 15 / 45. Current converter duplicates the Dreamcast second tier to provide 45 pointers, but retains Dreamcast mesh names/order. The first 15 names match retail. The second-tier name set matches but order diverges:

| Retail slot | Retail name | Dreamcast source index |
|---|---|---|
|22|AD01C7E0|27|
|23|7500A8E4|23|
|24|33040F2E|22|
|25|0A37C983|24|
|26|0B8584F6|28|
|27|0883A9C9|26|
|28|642918D4|25|
|29|8EA8550C|29|

Retail third-tier names are distinct and absent from Dreamcast. A potential repair is to map first two tiers by retail names, synthesize the third from the reordered second, and emit retail names. HOWEVER runtime tracing shows actual selected order can differ, suggesting the game resolves some order by names. Do not assume bone-slot mismatch alone explains it.

The current actor trace is more specific: second-tier rigid points lie around x450/y167/z4150, while stitched points in the same mesh lie around x206/y95/z379 or zero. This looks like stale/wrong transformed stitch sources, possibly tier-specific reference numbering. Investigate the source-buffer reset and reference remapping across LOD tiers. The preceding commentary overconfidently called mesh order the mechanism; that conclusion remains unproven.

### Internal object pointer relocation

Each 36-byte object record's last dword appears to point into metadata:

- DC: 0x4038C; mesh names begin at 0x403A8 (28 bytes later).
- Retail: 0x368A0; names at 0x368BC.
- Converted: record still 0x4038C, names moved to 0x41BF0 (analogous pointer would be 0x41BD4).

Converter copies records and moves metadata without relocating this field. Its semantics must be established in the parser before changing it.

## Changes made in this investigation

Only new source edit since the resumed deep dive: `spiderman/patches/ModelDiagnostics.cs` now supports `RECOMP_TRACE_MODEL_LAYOUT=henchman`.

- ParseExit reads model name from model table `0x800A0904 + slot*64`, pointer table from entry+0x10, mesh count at pointerTable-4.
- Maps vertex pointer `mesh+0x1C` to slot/name/index/count.
- TransformEnter identifies active mesh; TransformExit logs numeric xyz bounds for matching meshes.
- Optional aggregate sequence bounds.
- Existing hooks already present in spiderman/config/spiderman.json: parser func_80074C98, transforms func_8007B798/8007B9CC/8007BBD4/8007BD04, DrawPrimSet.
- Build and publish succeeded. Diagnostic published exe: SHA256 `7711B5D020DB9473CFEB872D649A43F1EA6792CDD4D7DADA95E749FED8AAF87B`, 91,569,447 bytes, at `spiderman/port/dist/SpiderMan.exe`.
- User-facing staged exe was NOT changed during this investigation.

Diagnostic caveats: sequence detection is inherited and based on source pointer increasing; it may group multiple actors. Bounds read output+4 as signed depth; verify representation. Mesh mapping should be gated to avoid overhead in other diagnostic modes. Face audit remains player-only. Numeric logs are evidence to interpret, not sufficient proof by themselves.

## Runs and evidence

Two isolated directories copied from an earlier stage verification clone:

1. `proof_render/sm1-deep-layout-current-20260908`
2. `proof_render/sm1-deep-layout-retail-20260908`

They contain the diagnostic exe, settings, memory cards, mods and assets. Their inherited settings use `spiderman/extracted` for game data, widescreen=true, ActiveMods=[]. No new images were generated (capture logs show shots=0 every=0). Copies may contain pre-existing shots directories; do not inspect or transfer those images.

### Current converted actor run

`proof_render/sm1-deep-layout-current-20260908/deep-log/spidey.log`

Exit 0 at frame 2200. Henchman loaded slot8/45 meshes, source 279928 bytes, arena 0x8035C000. At f2198, second-tier transforms show mixed depths and huge stitched spans:

- mesh15 vertices18: (450,167,4122)..(454,169,4166)
- mesh16 vertices14: (206,95,379)..(450,170,4192)
- mesh17 vertices12: (204,94,369)..(450,174,4180)
- mesh28 vertices22: (0,0,0)..(449,169,4226)
- mesh27 vertices33: (0,0,0)..(453,163,4225)

### Invalid initial retail comparison

`...retail.../deep-log/spidey.log` is INVALID as a retail comparison: the installer rematerialized builtin assets, overwriting the copied retail henchman with bundled converted bytes. Runtime logged 279928, not 252532. Never rely on replacing builtin assets before startup for an A/B.

### Correct explicit retail override

Created separate `...retail.../retail-override/henchman.psx`, copied from `spiderman/extracted/wad/henchman.psx`. Explicit SPIDEY_ASSET_DIR bypasses bundled overwrite. Retail SHA256 `4235A1BB02E59491624EF56DE8DD413D5D6D0FF335A7FCA742B1FA4E95128FBB`, 252532 bytes.

- `...retail.../deep-log-retail-override/spidey.log`: verified correct 252532-byte actor, exit2200 too early for actor transform evidence because title timing varied.
- `...retail.../deep-log-retail-2500/spidey.log`: latest completed run, exit0, process51408 exited. Log is 206233 bytes. Reading its filtered contents was interrupted by user; THIS IS THE NEXT EVIDENCE TO READ.

Important comparison caveat: retail-override contains only henchman, so all other loose actors fall back to retail data. For a tightly controlled A/B, use an external override directory containing the full current asset set plus only henchman changed, explicitly set SPIDEY_ASSET_DIR, and keep pack root identical. The previous run is useful preliminary evidence but changes more than one actor asset.

## Process-local route

Use env settings in a terminal subprocess, launch hidden, wait for exit; no host input:

```
SPIDEY_BOOT_SKIP_UNTIL=title.bmr
SPIDEY_SCRIPT_EXCLUSIVE=1
SPIDEY_SCRIPT=title.bmr+120:start:12;title.bmr+420:cross:12;title.bmr+720:cross:12;title.bmr+1100:cross:12
SPIDEY_EXIT=2500
RECOMP_TRACE_MODEL_LAYOUT=henchman
SPIDEY_LOG_DIR=<unique existing directory>
SPIDEY_ASSET_DIR=<explicit external override directory when testing assets>
RECOMP_ASSET_PACK_DIR=<clone>/assets/builtin/packs
```

Unset SPIDEY_SHOTS and SPIDEY_SHOT_EVERY, and audit any inherited capture/dump environment settings. Absolute exit frame varies relative to level load because startup timing varies; anchor tests by game events where possible. Correct boot variable is SPIDEY_BOOT_SKIP_UNTIL, not SPIDEY_BOOT_SKIP.

Build: `dotnet build spiderman/port/SpiderMan.csproj -c Release`.
Publish: `dotnet publish spiderman/port/SpiderMan.csproj -c Release` (do not use --no-restore after a non-RID build; assets need win-x64 restore).

## Relevant source and next work

- `spiderman/tools/port_dc_character.py`: build_character (~2061), attachment_reorder_map, remap_attachment_references, sm1_far_lod_sources (~2226). Current additional_mesh_sources only duplicates a tier. Investigate attachment source indices per tier and actual runtime selected order before editing.
- `dreamcast/tools/port_all_characters.py` uses sm1_far_lod_sources.
- `spiderman/generated/main.cs`: func_80074C98 (~213725), transforms (~221642), LoadPsx (~199912). Read for semantics; don't hand-edit generated output.
- `spiderman/patches/ModelDiagnostics.cs`, `DcModelCompatibility.cs`, `GameTrace.cs`.
- Renderer: tools/RecompOne/RecompOne.Runtime/Gpu/GpuHleForward.cs, GpuRaster.cs, Hle/GpuHle.cs, Backends/Common/GlCore.cs and GlShaders.cs; Hardware/GteScreen.cs; Context/CpuContext.cs; spiderman/patches/Wide.cs.

Next: read latest retail 2500 log, compare rigid/stitch bounds, trace parser relocation and LOD stitch references; build a controlled A/B that changes only henchman. Then separately instrument actual world/player GPU submissions and shader interpolation. Prior logs only proved perspective metadata existed; they did not prove it reached rendering correctly or fixed seams/warping. Need old/new numeric evidence on actual paths, not merely build success or nonzero depth counts.

No repair, rebundle, or staging has yet occurred after the diagnostic additions. All four user-reported issues remain open. Do not claim 100% verification from logs that merely lack errors.
