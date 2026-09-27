# P1 rendering investigation

Goal remains open. No final release-stage promotion is claimed here.
Performance/stalls and retargeting/shoulders remain closed by user direction.

Update: the meter repair below is now implemented and visually checked in a
candidate build. The fade, menus and final release-stage promotion remain open.

## Baseline builds

Built from tracked source at 81b631d, before the trace-only changes below:

- `work/render-p1-baseline/sm1/SpiderMan.exe`, SHA256
  `be1dc1667f6e313cb265c17a0b33107f548b72264932a331014aeb6a64a92312`.
- `work/render-p1-baseline/sm2/SpiderMan2.exe`, SHA256
  `0a99999e818b9624484594d6c88312909cc9a12609c36a111736acb652b740f4`.

Both are fresh self-contained publishes, not the older canonical dist EXEs.
All runs below used hidden native windows, process-local scripted controller
state, and the game's framebuffer capture. No desktop input/capture was used.
Each run has environment.json, console.log and result.json under
`proof_render/performance-repairs/`. All four runs exited 0 without timeout.

## Inspected native evidence

- `p1-menu-baseline-01`: SM1 l5a3, Start at trigger+520, Down at+680,
  Start at+800, exit2100. Individually inspected all five captures in order:
  1596,1666,1668,1786,1936. The initial capture is the sewer introduction,
  not free gameplay. Pause overlays it. At1666/1668 the SELECT area has bright
  red/white/blue horizontal corruption; these two frames look identical.
  The menu panel is still expanding at1786; labels are present by1936, but
  the SELECT corruption remains. This does not establish Pause/resume input
  correctness or rule out flicker on uncaptured frames.
- `p1-chase-baseline-01`: SM1 l5a1, no route movement/follower, exit2800.
  Individually inspected1736,1936,2186,2386. At1736 the long top chase bar
  has a clear gap between its transformed left segment and remaining bar.
  No special route is needed to reproduce this. Native LostVenom fires1886;
 1936 shows Venom in the loss sequence. At2186/2386 the Retry/Quit labels and
  tinted city backdrop are visible and identical. This is not a fade-sequence
  verification, full Retry flicker test, or chase-completion test.
- `p1-menu-sm2-baseline-01`: SM2 e1m0, Start at trigger+3400, Down+3900,
  Cross+4100, exit5100. Individually inspected4388,4758,4760,5008.
  Free gameplay is visible first, then Pause. Continue is selected at4758/4760;
  Restart Level is selected at5008. The background question-mark object has
  unexpected pale coloring against the overlay. This needs draw-order/blend
  attribution, not an assumed fix. The final Cross press is beyond the exit
  tick (trigger loaded1008), so restart was NOT exercised in this run.
- `p1-menu-gl21-baseline-01`: the requested gl21 option actually selected
  Gl45, as console.log confirms. Inspected1666 and2086; both reproduce SM1's
  corrupted SELECT trail. This is a repeated GL45 run, NOT independent GL21
  evidence. Legacy renderer selection is compile-gated. The runner now rejects
  unsupported backend names instead of silently accepting this mistake.

## Source findings and instrumentation

`Wide.InHudCorner` excludes rectangles wider than one quarter of the draw area
and those extending beyond its top-left region. The chase screenshot supports
the known mixed-transform hypothesis. Exact meter packet bounds are still
needed before choosing a rule that preserves unrelated scenery and overlays.

The existing geometry trace recorded only triangles and omitted vertex colors,
texture-window/mask state, blend mode and HUD/coverage classification. It now
records these fields and traces rectangle primitives using the backend's
triangulation. Rendering submission itself is unchanged. The trace still uses
its explicit opt-in path and bounded frame interval; no trace output by default.

Runtime build and the existing RenderingRegression executable pass (exit0).
A second regression run with RECOMP_GEOMETRY_DUMP set emitted the new fields;
first record inspected in `work/render-p1-trace-regression.jsonl`.
These numerical checks do not prove any requested visual repair. The baseline
EXEs above predate the enriched trace and must not be used to claim its native
coverage. Publish a fresh diagnostic candidate for the next packet captures.

## Next evidence needed

1. Capture the chase HUD around trigger+650 with enriched packet tracing,
   compare wide and native4:3 bounds, and add a regression for joined segments.
2. Capture Venom's actual disappearance interval sequentially and identify
   its texture source, STP handling, primitive colors and blend mode.
3. Attribute the Pause trail corruption and SM2 overlay behavior; test settled
   menus, both buffer phases, selection changes, resume and Retry transitions.
4. Verify fixes visually in fresh native builds, stage both games, update TODO,
   commit completed repairs without pushing. Preserve scenery/timing/audio/suits.

## Meter repair and rectangle state

`p1-meter-packets-wide` and `p1-meter-packets-native` both exited0 and captured
1736 with enriched traces1734..1736. Both images were inspected individually.
The native4:3 rail is continuous; the wide rail has a gap. The actual draw area
is512x240, not320x240. Contrary to the earlier width-limit hypothesis, the rail
is made of small quads: the native segments cross the top-left corner boundary
and receive different transforms. At x174..192 the wide quad becomes130..144,
while its neighbor remains192..210. The right-overlay flag alone does not
transform the remaining pieces.

The SM1 patch now identifies the rail, both caps and both head markers by their
native atlas page/CLUT/UV regions within the top HUD. All share the right-edge
transform, regardless of their position along the rail. GTE-derived geometry
is excluded first, and SM2's independently selected gameplay address excludes
this SM1-specific rule. It does not broadly reclassify top-row menu text.

Separately, the reused RenderPrimEvent retained HasDepth/Depth/U/V fields from
the preceding polygon when decoding a rectangle. Clear its depth and initialize
its UVs. This makes sprite classification independent of prior world draws.
The GP0 regression reproduces the old failure and passes after the change.

Tests: RenderingRegression fails before the fixes (rectangle case and12 rail
positions/buffer combinations), then passes. Added cap/marker checks also verify
that world-provenance packets remain untouched. Both SM1 and SM2 regression
executables pass; the SM2 suite includes the shared rectangle regression.

Fresh candidate publishes (not canonical release promotion):

- `work/render-p1-meter/sm1/SpiderMan.exe`, SHA256
  `5fdd81aa6c219a02066da39e9a6f51c635af55c62b68919e4c03df5552e82d57`.
- `work/render-p1-meter/sm2/SpiderMan2.exe`, SHA256
  `ca2218be04c92dc39a59252ade3c8a933e387eaa2316780cd2763555d006ae5a`.

Native evidence:

- `p1-meter-fixed-wide`, exit0: individually inspected1680,1736,1790.
  1680 is still the introductory motion without HUD;1736 and1790 show a
  continuous rail, both heads, and changing Spider-Man marker position.
  Filtered frame1734 packets by the five meter texture identities before
  comparison:46 triangles/138 vertices match the native packet order, UVs and
  shared transform with zero mismatches. Do not compare whole trace row indices
  between aspect modes: world culling changes the number of scene triangles.
- `p1-meter-fixed-native`, exit0: individually inspected1736 and1790.
  Native4:3 remains continuous with its original placement; marker motion is
  visible. This is a stationary HUD test, not chase completion.
- `p1-menu-rectstate-sm1`, exit0: individually inspected1669 and2089.
  Pause's SELECT corruption remains. Thus stale rectangle provenance was a real
  independently reproduced bug, but does NOT fix this menu defect. Trace2086
  identifies the trail as a textured additive quad, TPage40/CLUT160,
  UV120..127/208..254, screenX512..244/Y157..180, RGB64,64,123 fading to0.
  The button icon is TPage40/CLUT36, UV0..20/225..237, RGB64. Next identify
  whether these use replacements and inspect their source palette/STP state.
- `p1-menu-rectstate-sm2`, exit0: individually inspected4758,5008,5358.
  Continue and Restart selection states are visible; Up then Cross returns to
  gameplay by5358. The pale question-mark artifact persists while paused and
  returns to its blue appearance after resuming. This verifies the bounded
  selection/resume flow, not the unresolved overlay rendering or Retry flow.

No disappearance fade repair is claimed. Retry/Pause transitions and temporal
flicker acceptance still require further diagnosis and native validation.

## Texture A/B and cyclic Pause ordering table

All following runs exited0 without timeouts. Diagnostic builds were published
under work/render-p1-textures, render-p1-orders, render-p1-dma and render-p1-stack;
none were promoted to user staging. The native-textures switch disables host
texture replacement only, not replacement actor geometry. Texture trace palettes
are CPU VRAM-shadow values, not GPU readbacks.

- p1-menu-native-textures and p1-menu-replacement-textures: inspected2086
  individually. The SELECT trail corruption is unchanged without replacements.
  TPage40/CLUT160 trail and CLUT36 icon both resolve to native atlas pixels.
  Their CPU palettes are grayscale with STP bits, blend mode1/additive.
- p1-menu-order-trace: inspected2089, same corruption. Geometry contains
  11015/11010/11010 triangles in2086/2087/2088. Submission-boundary counters
  produced no records; this is NOT proof of an alternative rendering path.
- p1-menu-submission-stack: first-triangle stacks in each traced frame identify
  LibGpu.DrawOTag -> func_80061308 -> func_8002C174. SDK/DMA counters enabled
  only at submission entry missed this interval and were removed.
- The2088 RAM snapshot in p1-menu-submission-stack has a concrete OT cycle.
  GP is800B47F4 (not800B0000). Read active environment at800B54A8 ->8009A6E4;
  env+70 ->800C65EC; begin traversal at OT+3FFC, mask addresses007FFFFC.
  Node2640 is00300000 (header02316CEC, commandE3000000). Node2842 at00316CF4
  has header0C300000, command3C21197F, returning to node2640: a203-node cycle.
  The SDK loop otherwise permits1048576 visits. This explains repeated menu
  submissions, but the writer that introduced the bad link remains to be found.
  FramePackets allocates its first expanded pool at80300000; preserve expanded
  capacity while diagnosing the menu packet link/cursor ownership. Do not simply
  truncate the list and declare the source bug fixed.
- p1-fade-loss-bracket: inspected1940,1980,2020,2060,2100 in order. Actual
  LostVenom disappearance begins after2020: mixed opaque/cyan patches, then black.
- p1-fade-native-atlas and p1-fade-replacement-atlas: inspected every captured
  image,2050..2120 at10-frame intervals, sequentially in each run. Native atlas
  gives translucent cyan disappearance without black patches; replacements retain
  opaque body regions and then black silhouettes. These are sampled intervals,
  not an every-frame temporal acceptance pass. No chase completion test was used.
  Trace2058..2060 identifies semi-transparent replacement hits on TPage169/172/173,
  CLUT1248, RGBA32x32/16x16 tiles enlarged4x, no replacement CLUT. The256-entry
  CPU palette has90 STP entries. PrimFs replacement mode6 currently derives STP
  only from replacement image alpha; original texture mode derives it from native
  texel alpha. Investigate preserving authored native blend eligibility without
  disabling HD art globally or breaking custom suit alpha.

RenderingRegression was rerun successfully with the diagnostic runtime changes.
Meter/rectangle fixes remain passing. Menu cycle source repair, Venom material
repair, both-game native validation and final staging remain open.
