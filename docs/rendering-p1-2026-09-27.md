# P1 rendering investigation

Goal remains open. No rendering fix or release-stage promotion is claimed here.
Performance/stalls and retargeting/shoulders remain closed by user direction.

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
