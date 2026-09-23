# Chase Venom: ordinary-control verification

Full completion is **not verified**. These fixtures use only process-local
controller buttons. Never enable `--chase`, `SPIDEY_CHASE_FOLLOW`, region pulses,
or actor-position writes for this acceptance task. Snapshot/capture runs are
route diagnostics, not performance baselines. The native game remains unchanged.

## Pause decision pending after nine-hour overrun

Do not launch another speculative replay while awaiting the user's answer.
The already-finished chase-west-building-corner-normal was reviewed read-only:
3099Left leaves player atsamewall;3109jump detaches;3119(13256,1542,11202)
state400;3139(11304,1475,10318)wall1;3229(10705,898,10663)ground10.
It passes the corner but LostVenom script800E4CAC fires atframe8644. Full chase
failed. No new build/run; current full ZIP unchanged. This was route tuning,
not a new game fix.3719 positive presentations,max48.0189ms,0>100ms;
0audio recoveriesafter5s, recording ends179.345s. Allownedexits0.

## Current next action - west building corner

`chase-intermediate-roof-jump-normal` verifies the3040jump and reaches a new
building:3040(16189,1332,11669)ground10;3050(15899,969,11581)state4;
3070(15156,1356,11383)state400;3090(13520,1248,11311)wall1,normal
(3547,-1,-2048),contact80106058. Up climbs toY1014 at3120 and623 at3150,
then LostVenom by3190. The route is still incomplete, but this prefix progresses.
Source pointer joins original mesh75face27, vertical bandY444..1850 from
XZ(13389,11279) to(14309,12872). The whole mesh top isY-2633. The player
contacts near the north end; going around that end is the next route hypothesis.
Source join: chase-west-building-contact-review.json. No native edits justified.

Next untested native-chase-west-building-corner-script.txt preserves through3058,
holdsUpR2 until3090, thenLeft14,3104UpLeftCross6,3110UpLeftR2:12,3122UpR2.
Check whether Left travels toward the nearby end before retaining this tail;
wall-relative steering direction is not yet verified. Use at most8 snapshots
around3090..3220 and keep normal input only. Suggested level offsets7140,7160,
7180,7200,7240,7300,7360,7420 (read actual native counters from snapshots).

The completed run used Normal game creation priority,181samples allNormal,
3582positivecounter presentations,max40.1513ms,none>100ms,0audio recoveries
after5s. HostCPUmax61%,queue0;allownedexits0,20747runtimeevents,lost0.
No new images or audible acceptance. Earlier failed runs remain evidence.
All diagnostics are stopped at this handoff; current ZIP/binaries unchanged.

## Current next action - September 18, later update

`chase-lower-roof-relaunch-witness` tested2980jump:2989(18353,-71,11087),
3009(17430,223,11335)state400 with a live web object;3039(16225,1332,11660)
state8/animation229;3069(15612,1724,11824)fallstate4. By3109 the player has
teleported to the fall-failure position(27502,2213,-24056), then counter resets.
This is failure, not completion. All owned exits0; gameplay max33.9059ms,
no intervals>100ms or audio recoveries after5s.20952runtime events,lost0.
The matched `chase-lower-roof-normal-priority` run starts only the game at
Normal creation priority (default runner mode still inherits). All8 sampled
player states/counters match exactly. Gameplay max33.5601ms,0recoveries;
20786runtime events,lost0. Both hosts were relatively quiet: sampled CPUmax
50.05/57.29%,queue0, so this does not isolate priority under saturation.

Next untested ordinary-input candidate: native-chase-intermediate-roof-jump-script.txt,
3040UpLeftCross6,3046UpLeftR2:12,3058UpR2. Source state8 can call jump handler
8004EC20 with freshCross; snapshot3039animation229 is not one of the two
excluded animations178/180, and gates900/F68 are0. Relaunch before walking off
this lower roof and steer west/north toward Venom. Saved800B4F64 is0 (manual
swing), so R2 is appropriate; do not blame auto-swing mode. Read-only source/RAM
join: chase-post-building-auto-swing-review.json. No new images, no game edits.

## Latest progress and next action

**All three interior triggers and the authored player sequence now run through
ordinary inputs; the complete chase still fails afterward.** Best fixture:
`native-chase-north-wall-end-script.txt`. It preserves both earlier triggers,
turns1886up+right24,1910up80,1990right18,then2008up. Trigger44 activates and the
player enters the guarded building script. `chase-authored-sequence-long` shows
script cursor advancing and player script flag returning to0 bycounter2909.
Cadence returns from20Hz to30Hz nearcounter2778 (inferred from presentation trace).
The replay keeps holding Up and loses Venom atframe7996; it is NOT completion.

Next route work: stop steering Left after the later second takeoff.
`chase-south-wall-end-witness` uses native-chase-south-wall-end-script.txt:
keep2815firstjump, delay secondjump from2860 to2900(left+cross6),then2906left+r2.
It clears the old contact:2899(19566,257,9455),2919(19358,-323,9805)state400,
2949(18516,-252,11003)state4/wall0. ContinuedLeft then rotates away:
2989(18108,257,10201),yaw3825/camera602, losesVenom. Prefix/clearance useful;
terminalLeft is not accepted. Candidate native-chase-west-heading-script.txt
changes2906LeftR2 to12updates,2918UpLeftR2:12,then2930UpR2.
`chase-west-heading-priority-witness` tested this: 2939(18722,-335,10915),
state400,wall0,yaw1478/camera1481;2959(18608,-316,11077),state1,wall1;
2989(18408,-449,11193),ground10,yaw1365. It clears the former surface but
hits/climbs a raised surface, then walks too slowly and loses Venom by3069.
3454 positive-counter presentations, max48.7326ms, none>100ms; one audio
recovery after5s, at7.320s. All168 production priority samples BelowNormal.
24663 runtime events, lost0; all owned processes ended. No new images.
Current candidate: native-chase-raised-roof-jump-script.txt,2956UpLeftCross6,
2962UpLeft6,2968UpR2:100,3068UpCross6,3074UpR2.
Run chase-raised-roof-jump-witness completed204.625s,all owned exits0.
2954(18608,-316,11077)wall1;2961(18608,-244,11077)state4wall0;
2969(18608,234,11077)state200;2984(18476,257,11051)ground10yaw1191.
It walks until3068 jump;3099(15559,-77,11814)state400, then LostVenom by3149.
Next untested candidate native-chase-lower-roof-relaunch-script.txt moves the
3068 jump to2980: keep2968UpR2 only12updates,2980UpCross6,2986UpR2.
This targets the avoidable ground-walking interval after landing; full chase
still fails and the later route has not yet been validated.
Retain performance failure:13 positive-counter intervals>100ms,max1537.0901ms,
48 audio recoveries after5s.20688 runtime events,lost0; no images. At1537ms
hostCPU100%,queue48/39,1535.694ms after callback, no GC/managed contention.
See largest-stalls-review.json. No games/collectors remain running. Keep this ordinary-input route work
separate from performance/visual acceptance; no completion is claimed.
Original contact was mesh125 face3, pointer80117CC8, a150-high face from
(17891,-375,10003) to(18226,-525,9806). Original vertex bytes match saved RAM;
player2909 was97.75units from this face. Source evidence in
chase-post-building-wall-shape.json. Do not delete authored collision/geometry.
Venom goes via214(16233,237,11701) to216(10936,953,10226). Full completion open.

Keep full P0 witnesses. Shared-host scheduling-pressure stalls are retained in
chase-camera-aligned-witness and the long run's game-over phase. The reproduced wall seam is repaired in the promoted shared per-corner precision
hook: chase-partial-corners-wall-seam/seam-repair-review.json records aligned
edges and the inspected image. The earlier solid diagnostic ruled out texture
cutouts. Both games pass49 rendering checks. Full chase and other unverified
geometry/material/camera behavior remain open; see the repair handoff.

Aligned native-update replay matches the retained route and repeats exactly. Opt-in
`--native-script` (SM1,l5a1 only) uses `update:buttons:duration` steps.
Capture samples raw gamecounter0x800B4F38 during VSyncInputEvent and supplies
normal Controller.ScriptHeld; the normal runtime refreshes pads. No direct
pad-buffer, actor, counter or trigger writes. It arms only after l5a1_t.trg,
waits to observe counter0, and stops permanently for that process if the
counter decreases after starting. This intentionally does not replay on retries.
Menu scripts retain archive/console scheduling. Native schedule unit checks
cover repeated ticks, overlapping buttons, release boundaries, epoch reset,
and delayed arming; total332 timing/XA/input assertions pass.
`native-chase-baseline-script.txt` was derived from first requested script mask
at each native counter in retained chase-input-phase-a (counter98 onward).
It is a candidate recipe, not accepted route equivalence or full completion.
Completed native pair: chase-native-update-a/b,3800ticks each,read-only PadTrace,
no snapshots/images:2046matched native keys,0unmatched,0input/pose differences.
Canonical SM1 now uses sm1-native-input/SpiderMan.exe. Recipe starts must shift
-1 to account for measured sampling-to-consumption delay. Prepared
`native-chase-aligned-script.txt` ran twice:2044matched native keys,0differences,
0unmatched. Aligned-a also matches retained phase-a on2044keys. It reproduces
the known route and its failure; full completion remains open.

## Historical input investigations (superseded by native-update replay above)

**Input sampling repaired; scheduling alignment still open.** Both capture
harnesses now prepare ScriptHeld via VSyncInputEvent before the tick samples pads,
and exclusive mode applies Controller overrides immediately.324 timing/XA/input
checks pass. New phase-a/b traces have zero stale controller-vs-script masks
afterrelative400 (old traces50/10); however anchors1087/1062 now request different
script masks at matching native counter98. This is remaining archive/console
schedule alignment, not stale delivery. First yaw difference is counter250.
Next schedule controlled replay against native updates, without actor writes.
P0 also recurred during phase-b:2069ms gameplay interval. Prioritize detailed
phase/GC/host-witness capture on this moving route; keep route and performance
acceptance distinct. Both3800tick traces and SM2 native integration are terminal.

**Input-phase divergence captured:** traced-roof-baseline/jump have anchors
1089/1088,2829/2831 contiguous records,3800ticks,68.328s/67.907s,exit0.
At native gameCounter98, same player pose and ScriptHeld16400 (Up+Cross),
baseline Controller49135/nativeheld1032 sees the press, variant Controller65535/
nativeheld0 does not yet. First pose divergence is yaw at counter458: XYZ equal,
baseline yaw2261/held1024 (Up),variant yaw2465/held1536 (Up+Right),while both
ScriptHeld16 request only Up. Thus the short turn's release reaches the game on
a different update, long before the planned roof-jump difference atrelative2430.
NEXT fix/validate scripted input delivery phase for controlled route tests (or
schedule/record inputs against native consumption). Do not change production
game speed or claim the variant jump caused its different route. Merely assigning
ScriptHeld does not guarantee native pad buffers already reflect it. Trace the
full Controller.State -> BIOS/libpad buffer -> decoder path before editing.
Full comparison/first pose divergence:chase-traced-roof-comparison.json.
`tools/chase_pad_compare.py` reproduces matching-by-counter/caller/occurrence and
preserves missing/capped/unmatched records; it does not silently align trace rows.

Newest input evidence: opt-in `--pad-trace` records raw state after native
8006B514, bounded4096 records; no RAM-read idle-breaker or game writes.
`chase-pad-trace-a/b`:3250ticks,59.344s/59.406s,exit0,no images/snapshots.
Anchors1088/1091;2281/2280 contiguous trace records. Matching gameCounter,
caller and occurrence within that pair gives1760 comparable calls after
relative400,zero differences in native held/pressed,script/controller state,
XYZ/yaw. Two unmatched tail records in A are retained, not compared as equal.
This pair does not reproduce the prior divergence; do not declare it explained
or assume instrumentation has no timing effect. Next retain this trace when
repeating the ledge experiment so any recurrence can be localized at the native
pad boundary. Native pad processing has multiple calls/update; never align by
trace row index alone. Evidence:chase-pad-trace-comparison.json and per-run
pad-trace.json. Canonical SM1 SHA9F2758F6... includes only opt-in diagnostics.

**Extended controls diverge; do not resume blind jump tuning.**
`chase-ledge-control-a/b` use identical full inputs and offsets
1890,2090,2230,2320,2370,2430,2470,2510;3650ticks,66.000s/66.703s,exit0.
Anchors1088/1089. All sampled game counters and buttons match, but positions
already differ at1890; A is(5078,1203,5577),B(5227,1220,5264). At2510
A remains on roof(5179,578,10102),B falls(6050,2404,10680). No images.
The previous matching pair proves only that pair's repeatability. Next trace
native pad processing at8006B208/8006B514 and actual update entry across early
input edges, using raw memory reads. Sparse held/pressed snapshots cannot prove
identical input history. Archive-anchor parity or input phase is a candidate,
not a proven cause; matching game counters do not prove matching simulation.
Do not use GameTrace.TriggerPass/LogicFrames as an update hook: that historical
comment was wrong (observed level-load calls); Rates uses800B4F38 instead.

**Newest control baseline:** `chase-relative-control-a/b` use
`--snapshot-offsets 590,890,1190,1490,1690,1890,1990,2090` with identical inputs.
3250ticks,61.719s/61.812s,exit0,no images. Both anchors1088; all eight relative
ticks resolve exactly. Player positions, native button records, Venom state and
game counters match exactly at all eight checkpoints. Comparison retained in
chase-relative-control-comparison.json. This establishes repeatability for this
sampled interval/configuration, not deterministic physics for all runs or the
cause of the earlier39tick discrepancy. New SM1 diagnostic build SHA66087986...
adds archive-relative checkpoints; it changes no clock/input/gameplay behavior.
Next extend this identical-control schedule through the disputed ledge before
varying the next jump. Keep snapshot offsets identical in A/B runs so capture
overhead/phase is not an uncontrolled variable.

Control evidence: `chase-control-a/b` use identical buttons and eight absolute
snapshot ticks,3250tick runs,60.219s/60.453s,exit0,no images. Their level
anchors are1088/1090, so absolute snapshot pairs differ by two relative ticks;
do not call their position differences nondeterministic at a matched checkpoint.
All paired button snapshots match. After aligning presentation indices by level
anchor,801 samples atrelative500..2100 have a constant game-counter offset of
minus1 for B versus A, not a growing rate difference. Earlier disputed edge/jump
runs have zero differing game counters across presentation indices550..1570.
These observations do not establish the cause of their39tick trigger difference.
Next add level-relative diagnostic snapshots (existing ones are absolute) or
trace native update entry, then compare matching input phases. Preserve the
clock; a broad rate defect is not demonstrated. Decoder now includes raw
game_counter800B4F38; no game RAM writes or runtime changes were made.

**Control-run gate before further timing tuning:** next-roof-edge and
next-roof-jump have the same executable SHA7337E933...,same level anchor1088,
and identical applied inputs until R2 release atrelative2430. Yet native
trigger800E48D8 occurs atabsolute3133 versus3094 (relative2045 versus2006),
well before that changed input, and before either run's first snapshot/capture.
Thus earlier route divergence exists; do not treat one changed input and one
outcome as a controlled causal result. Next repeat identical input/snapshot
schedules, locate the first divergence and inspect console-tick versus native
update/input scheduling. Do not assume a clock defect or fix positions to hide it.

`chase-next-roof-jump` releases R2 at2430, Cross2450:12,R2+2480.
5000ticks,88.765s,exit0,eight snapshots,no images. At2472 player
(5185,541,10107),then Lost-Venom triggers at2520/2524 and failure controls
the player by2572. This does not isolate whether the new jump works because
the preceding route already differed as described above. Retain both runs.

`chase-next-roof-edge` repeats earlier-swing,3670ticks/66.453s,exit0.
One native image atrelative2480 inspected: player airborne over the gap,
city/HUD intact, Venom visible on roof ahead. Positions2422/2442/2462
advance atY578/578/577;2482Y1007,2502Y1945,2522Y3150,2542Y4495,
then native FallFog control. The drop starts between2462 and2482; no decoded
grounded flag is claimed. Next release R2 at2430, Cross2450:12,R2+2480.

`chase-ledge-earlier-swing` changes only R2 initiation from2400 to2370,
keeping Right2325:70/Cross2340.4600ticks,81.625s,exit0,eight snapshots,
no images.2372(4486,253,9534),2412(4992,578,9903),2462
(5394,577,10201),yaw2656. This passes the prior fixed-X/Z wall position and
reaches a level-height moving interval on the next roof. Atrelative2551 the
native failure triggers include the string `FallFog` (script800E4DFC), followed
by player-script control/teleport and Venom removal. Do not mistake that teleport
for completion or a forced fixture: it is the game's failure sequence.
Next inspect/bracket the roof edge after2462 and jump before the failure;
release/repress R2 only after the known roof arrival. This is useful route
progress, but no new image establishes the exact roof outline or fall animation.

`chase-ledge-heading-only` changed only Right2325 from35 to70 ticks relative
to late-right, preserving Cross2340 and R2+2400.4500ticks,80.156s,exit0,
eight snapshots,no images.2371(4455,228,9512),yaw2662;2411
(4930,1170,9782),yaw0;2461(4930,988,9781),then failure by2561.
The earlier jump avoids the long-right run's deep initial drop, but this heading
still meets another wall and loses Venom. It does not improve on late-right's
script31/32. Next distinguish swing initiation from the intervening wall:
keep Cross2340 while varying only R2 initiation, or inspect that crossing;
do not repeat the already disproved delayed Cross2380. No speed change justified.

`chase-ledge-late-right` preserves R2 until2320, Right2325:35, Cross2340:12,
R2 resumed2400.4500ticks/80.078s,exit0,eight snapshots,no images. At2342
the player remains at(4003,577,9280),yaw2504,Right held: unlike the previous
comparison this turn registered on the ledge.2362(4118,214,9363),yaw2672;
2382(4275,318,9687),yaw2176;2512(4717,-236,12065),Venom script31/32,
distance7911. Failure owns the player by2712. It clears the obstruction and
advances one waypoint but still loses Venom. Next comparison extends Right
to70ticks and delays Cross to2380/R2 to2440 to direct travel farther right.

`chase-ledge-long-right` tried that comparison (4600ticks,81.969s,exit0,
eight snapshots,no images). It is worse:2369(4422,1338,9451),2399
(4899,3022,9749),2439(4928,3189,9782),then vertical movement at that X/Z.
The delayed Cross2380 comes after descent has already begun; this is not a
valid comparison of two jumps from the ledge. Keep the earlier Cross2340 and
R2+2400 for the next heading experiment, changing only Right duration. Best
latest progress is late-right, script31/32; full completion remains unverified.

`chase-fourth-roof-inspection` repeats the obstacle-jump route for3550ticks,
64.610s,exit0. Its sole native image frame3441(relative2350) was inspected:
the player runs against a wall on a narrow ledge, with city/HUD visible and
direction arrow pointing right. Relative2239 reaches(4003,578,8580),2279
(4003,577,8960),then2319..2439 stays(4003,577,9280),yaw2048. This is an
onward-route obstruction, not an established timing failure. Next comparison
releases R2 at2220, turns Right+2250:35, jumps+2280:12, swings+2340:2000.

That comparison, `chase-fourth-roof-right-jump`, finished4800ticks/90.312s,
exit0,eight snapshots,no images. It did not complete: relative2271 is still
climbing at(4038,790,8449),yaw0, rather than on the previously observed roof;
2331 reaches(4146,458,9255),and2461(3589,318,9817),yaw1540,
Venom script30/31,distance7447. Failure owns the player by2661. Releasing R2
at2220 changed the climb timing, so this is not a clean ground-turn comparison.
Next retain R2 until the established stopped roof position (about2319), then
turn/jump there; verify native buttons/heading instead of assuming the prior
turn occurred on the roof. Keep the failed run; no physics/speed change follows.

Latest inspection: `chase-left-roof-inspection` (3300 ticks, 60.547s,
exit0) shows the Left+1620:8 route stopped against a rooftop antenna, rather
than an inferred roof edge. Its sole native capture, frame3021 (relative1930),
was inspected: player running into the pole, roof, city, HUD and distant Venom
visible. Relative1859 through2109 retain exactly (5347.251,1596,4931.046)
with Up/R2 held and no player script. Earlier relative1759/1809 show the
wall climb to this roof. This is route obstruction evidence, not a clock defect.
`chase-left-roof-obstacle-jump` releases R2 at1840, jumps at1860 and resumes
R2 at1930. It finished4200 ticks in74.859s,exit0,eight snapshots,no images.
Relative1912 player(4944,1596,5949) is beyond the obstruction;2012 reaches
(4196,1914,8013),2112/2212 climb near(4003,8448),and2412 reaches
(4003,577,9280),Venom script30/31. Lost-Venom fires atrelative2460;
failure owns the player by2612. It clears the previous obstruction but still
fails ordinary completion. Next bracket the roof near X4003/Z9280 before
relative2460 and inspect its onward route; do not assume a timed jump proves
an animation or a decoded grounded flag. Both runs are terminal.

The best advancing attempt is `chase-third-roof-jump`: Venom naturally reaches
waypoint31 before loss. Retain Up after+908, R2 +980:220, Left +1200:60,
Cross +1260:12, Right +1280:15, R2 +1320:230, Cross +1600:12 and
R2 +1660:3000, following the first-roof buttons below. All offsets are relative
to l5a1_t.trg. The stable second-roof checkpoint below remains useful for
isolating changes; it is no longer the farthest progressing attempt.

`sm1-diagnostic-log-integration/frame_03400.png` was inspected and shows
Spider-Man wall climbing on a tall building after the third-roof jump, with
Chase HUD and surrounding city present. The repeated near-fixed X/Z at the
later snapshots therefore has direct visual support for this specific pose.
Next investigate the approach to the building near X6277/Z6009 and the later
wall near X7090/Z7422; continuing to hold Up/R2 loses Venom. No forced trigger,
physics or clock change is justified by a failed route-control attempt.

## Confirmed checkpoint

`proof_render/performance-repairs/chase-edge-swing-route` is the current confirmed
route to the second roof. It finished 3,800 console ticks without a loss/fall
script. The player remained under ordinary control, approximately
(2206,906,-1772), while Venom waited at script `0x800E23D2`, words
`0,11,16,0,11,18`, roughly 4,056 native units away. This is a checkpoint, not a
completed chase or a scene-wide visual pass.

All offsets below are relative to the first `l5a1_t.trg` load. They refer to
console ticks (60 Hz), not presented frames (30 FPS). The runner supplies title
and introduction confirmation presses automatically for `--level l5a1`.

| Offset | Buttons | Duration | Purpose |
|---:|---|---:|---|
| 575 | Up + Cross | 12 | First forward jump after the introduction |
| 590 | Up + R2 | 290 | First swing and roof approach |
| 880 | Up + Left | 28 | Move around the roof obstruction |
| 908 | Up | 2100 | Continue forward |
| 920 | Cross | 12 | Second jump |
| 980 | R2 | 2100 | Start swing after reaching the gap |

Do not move the last R2 press back to +945: that comparison fell and reached
Game Over. Delaying it by 35 ticks reached the second roof and remained in the
chase. Pressing while still over the roof is a plausible explanation; the native
swing eligibility condition has not been traced, so do not claim that precise
engine condition as proved.

Reproduce with a fresh run name:

```powershell
python tools/performance_native_run.py sm1 UNIQUE_NAME --level l5a1 --frames 3800 --extra-script 'l5a1_t.trg+575:up+cross:12;l5a1_t.trg+590:up+r2:290;l5a1_t.trg+880:up+left:28;l5a1_t.trg+908:up:2100;l5a1_t.trg+920:cross:12;l5a1_t.trg+980:r2:2100' --snapshots 2100,2200,2300,2400,2500,2700,3000,3500
python tools/chase_route_state.py proof_render/performance-repairs/UNIQUE_NAME
```

## What earlier failures establish

- Constant Up/R2 reaches an obstruction near (995,539,-5617). A native capture
  shows the player running toward rooftop antenna scenery.
- Jumping before landing does not test a grounded jump. A later grounded jump
  moved beyond the obstruction but fell on the next crossing.
- A 65-tick left turn went toward a tall structure. The position moved almost
  vertically at fixed X/Z, consistent with wall climbing; that animation was not
  visually verified. A later R1 pulse did not materially change the trajectory.
- Shortening Left to 28 ticks with R2 at +945 fell. Its capture shows Game Over.
- Shortening Left to 28 ticks with R2 at +980 reached the second roof. This is
  route-control evidence; no change to chase speed, simulation or collision code
  was required for that crossing.

## Next gate

The second-roof checkpoint is now visually inspected in
`chase-second-roof-inspection/frame_02500.png`: a large building face blocks
forward motion, with a narrow route on its left. Walking Up+Left for 90 ticks
from +1200 falls off that edge.

A better extension is verified in `chase-second-roof-edge-jump`:
shorten the +980 R2 hold to **220**, then Left +1200:60, Cross +1260:12,
R2 +1320:2600, with Up continuing throughout. This naturally advances Venom
through waypoints 18,22,23. The player then hits another wall near
(1518,1233,3093), moves vertically and loses Venom. It is progress, not completion.

Adding Right +1350:60 during that existing swing did not materially alter the
trajectory (`chase-third-swing-right`). Inspect/test heading **before** initiating
the +1320 swing next; no native rule about mid-swing steering has been proved.
Keep a snapshot within any turn interval to verify the button state directly.

`chase-before-swing-right` tested Right +1280:35 before R2 +1320, with
the same preceding route. It exited normally after 3800 ticks (68.313 s),
but the chase failed. At relative1295, Right was held and R2 was not;
player yaw changed from1937 at1270 to2459 at1295 and2691 at1320.
The player reached (5299,875,1731) by1570, then moved vertically at
nearly fixed X/Z. This overshot toward a different wall, so the input
was effective but the route was wrong. No screenshots were generated.
Next compare a shorter pre-swing Right hold (12â€“18 ticks) and snapshot
during that hold. This is route tuning, not evidence of a physics fix.

`chase-third-roof-route` tried repeating the turn/jump pattern at +1200/+1240,
with R2 at +1300. It fell: at relative1286 the player was (2207,436,-1443),
and by1436 (2680,5271,800); fall scripts fired at absolute2594. Its one capture
at3000 shows Game Over. The second roof's shape/edge needs inspection before
another jump sequence; do not assume the first roof's timings transfer to it.
The +1300 swing may still be too early over roof geometry, but that is unproved.

Extend the confirmed route past the second roof, then through the remaining
waypoints and authored building sequence. Retain failed attempts. Inspect actual
game output at consequential checkpoints and the final completion; exit zero,
rendered frames, a nearby actor, or a forced trigger is not completion evidence.
Use relative ticks from `route-state.json` when comparing runs, because boot and
level-load absolute ticks vary. Full performance/audio and cross-hardware
acceptance remain separate requirements in the performance handoff.


### Shorter pre-swing turn comparison

`chase-before-swing-right-short` changed only Right +1280 from35 to15 ticks
and ran4200 ticks (74.985s, exit0). Eight RAM snapshots, no images. At
relative1289 Right was held, yaw2408, player(2038,513,-1258); by1319 yaw2404,
player(2273,1436,-754). At1419 the player was(3415,1591,1052) and Venom had
naturally reached script waypoint18. At1569 player(4021,1367,2102), yaw2389,
Venom waypoint22, distance5281. By1719 a failure script owned the player and
Venom was gone. This is another failed ordinary route, not completion evidence.

The shorter turn avoids the previous far-right wall position near(5299,1731)
and advances Venom naturally. It does not establish a physics defect. Next
inspect the player/collision state around relative1450â€“1600 before choosing the
next jump/release; the later position alone does not prove a grounded landing.
All these route tests include RAM capture overhead and are not performance
acceptance. The package verification and this diagnostic have exited; owned
runtime/executable copies were cleaned up. No diagnostic game remains running.


### Third roof: observed running, edge bracket and next jump

`chase-third-crossing-inspection` (2900 ticks,53.484s,exit0) repeats the
15-tick pre-swing Right route. Its only image, frame2700, was inspected: the
player is running on a narrow roof beside a tall building, with city/HUD intact.
At relative1497..1607 Y stays1367 while X/Z advance from(3658,1527) to
(4207,2479). By1627 Y1376 and1647 Y1873 indicate leaving that roof edge.
This supports timing the next jump before the drop; no native grounded flag
has been identified, so the position is not advertised as a decoded flag.

`chase-third-roof-jump` shortens R2 +1320 to230 ticks, adds Cross +1600:12
and a fresh R2 +1660:3000, retaining earlier buttons. 4200ticks,76.047s,
exit0; eight snapshots, no images. It naturally advances Venom through
waypoints23,24,26,28,30,31, farther than the previous route. Atrelative1801
player(6276,2107,6007),1951(6277,1403,6009),2151(7090,3091,7422),
2451(7090,1137,7422); later Lost-Venom and actor removal. Vertical movement
at fixed X/Z still suggests intervening walls; inspect the next consequential
position before adding more turns. This is failed-route progress, not completion.
Neither run changes game physics, trigger regions, or actor positions.


### Pre-third-swing Left comparison

`chase-third-swing-left` adds Left +1620:8 to the prior third-roof jump
(R2 released+1550, Cross+1600, R2+1660).4800ticks,85.469s,exit0,
eight snapshots,no images. Atrelative1631 player(4376,991,2874),yaw2122;
1731(5410,2116,4661),yaw0;1931(4998,1596,5801),yaw1822,
Venom waypoint24/26. By2131 failure owns the player and Venom is gone.
The short turn changes the approach and reaches a different roof/edge, but loses
Venom earlier than the straight route. The first snapshot is after the8-tick
hold: heading changed, but no held-button sample is claimed. Next inspect or
bracket the roof aroundrelative1900â€“2000 before timing another jump. Do not
interpret a single Y1596 sample as a decoded grounded flag. Best advancing
attempt remains chase-third-roof-jump (waypoint31); this comparison is retained.


Native schedule semantics: its update number is the counter observed during
pre-pad VSyncInputEvent, not a promise that the decoder consumes it under that
same counter value. First native-update-a trace consumes the requested98 action
at99; at98 it still has0 while retained phase-a requestedUp+Cross. This one-update
pipeline delay must be corrected in the fixture (start97 for desired consumed98),
not by writing pad buffers or advancing game counters. Await paired repeatability
before treating a shifted recipe as controlled. Comparison tool --native-start
allows native-counter alignment without requiring an archive-tick log anchor.


Aligned recipe verification: chase-native-aligned-a,3800ticks,68.391s,exit0.
Against retained chase-input-phase-a fromnative98:2044matching native decode
keys,0differences in script/controller/nativeheld/pressed/playerptr/XYZ/yaw.
The -1preparation shift reproduces the known ordinary route exactly for these
samples, rather than merely producing another stable but different path.
A second aligned run remains pending. Next single route variant, prepared in
native-chase-next-roof-jump-script.txt, leaves inputs through1019 unchanged:
1007:up+r2:12;1019:up:10;1029:up+cross:6;1035:up:8;1043:up+r2:3000.
This targets a jump consumed at1030 before the recorded1040roofedge. Previous
baseline coordinates:1020(5072.8,578,9962.4),1030(5233.4,578,10081.5),
1040(5394,577,10200.7),1050(5625.9,1006.8,10370.6),thenfall/failure.
No variant has yet been tested; do not infer it will complete the gap.


Aligned-b finished3800ticks67.922s,exit0. Aligned pair2044matching keys,
0unmatched,0differences,contiguous traces/cap not reached. The jump variant
chase-native-roof-jump is now a4100tick run with identical earlier inputs,
no screenshots. Compare against aligned-a before drawing route conclusions.


## Controlled next-roof jump result

`chase-native-roof-jump`:4100ticks,73.438s,exit0. First input difference versus
aligned baseline is native1020 (R2release),first pose difference1031(afterCross
consumed1030). No earlier difference: this is a controlled route change.
At1040 player(5498.9,192.3,10277.3),1050(5695.2,254.7,10436.6),
1080(6804.3,263.7,11621.2),1110(8167.8,328.2,13071.2). Unlike baseline,
no FallFog fall immediately after1040. By1120 position(8246.9,289.1,13100.2),
yaw0,then roughlystationary XZ whileY decreases. Atcounter1134/frame3731,
script800E4CAC contains UTF16LE words decoding `You Lost Venom`;1135teleports
to(312,1097,-1793),a failure sequence,not success. No screenshot this run,
so obstruction type/next-roof landing itself is not visually verified.
Milestones saved in run/native-route-milestones.json;rawtrace/comparison retained.

NEXT: use the same native-chase-next-roof-jump-script.txt with one native capture
near native1100–1120 (VSync approximately3663–3703 for the observed run; prefer
archive-relative shot after checking actual anchor), plus bounded RAM checkpoints
if needed. Inspect obstruction and Venom destination before changing direction.
Keep earlier recipe fixed; choose the next local turn/jump from evidence.
Do not revert to console-tick control for the route or broaden the authored
building-cadence fix. Current progress is route traversal,not a new production
performance fix or fullChase acceptance. All three runs terminal,copies/caches
cleaned,zeroimages. P0/P1 open,canonical binaries and ZIP unchanged this turn.


## Next obstruction inspected; right-turn trial

chase-native-next-obstruction:3850ticks,69.032s,exit0. One native screenshot
frame03686 inspected: Spider-Man swinging beside a tall building corner, with
Chase HUD/arrow and surrounding textured buildings/sky visible. He has not
landed on an open roof at this point. Four snapshots at native1080/1100/1110/
1120 give Venom(12044,824,10937),(13951,1324,10758),(14266,1375,10758),
(14908,1403,10760),while player drifts toward(8247,289,13100). Thus route heads
too far+Z while Venom advances mostly+X; this motivates turning right before
the next swing instead of merely extending the existing heading.

Control comparison with prior jump run:2190matched native keys,0differences;
144unmatched keys only in the longer original run. Snapshot/capture overhead
has not changed the compared input/pose records. This is a scene/route check,
not timing acceptance. Exactly one image generated and inspected.

Prepared native-chase-next-roof-right-script.txt changes only
1019:up:10 ->1019:up+right:10 and
1029:up+cross:6 ->1029:up+right+cross:6.
Jump consumed1030 and R2resume1044 stay fixed. Trial chase-native-next-roof-right
uses4400ticks,PadTrace,twoRAMcheckpoints,zeroadditionalimages. Do not infer
success from a farther coordinate; inspect native failure/region scripts.


Right-turn trial: chase-native-next-roof-right,4400ticks,78.266s,exit0.
First input/pose differences native1020,exactly where Right was added.
Player1030(5264,578,9998),yaw3187;1040(5584,192,9951),yaw2847;
1070(7396,1006,10709),1071Y898.03,1072onwardY898.0.
By1110(8075,898,11037),Venom(14266,1375,10758). Player advances onlyabout
2X/3.5Z units perupdate late here,consistent with another collision; no image
of this later surface yet,so do not call obstacle geometry verified.
Script800E4CAC again triggers You Lost Venom atframe3762;failure sequence
teleports to(312,1097,-1793). This is an improved heading,not completion.

Next isolated trial native-chase-lower-roof-jump-script.txt keeps priorinputs
through1077: replace terminal1043:R2hold with
1043:up+r2:34;1077:up:6;1083:up+cross:6;1089:up:6;1095:up+r2:3000.
Normal Cross consumed1084 after measured landing1071; run
chase-native-lower-roof-jump,4400ticks,PadTrace/twoRAMsnapshots,noimages.
Do not combine this with an untested additional turn when assessing the result.


Lower-roof jump result: chase-native-lower-roof-jump,4400ticks,78.265s,exit0.
First input difference1078(R2release),first pose difference1085(afterCross1084).
No earlier difference from right-turn baseline.1100(8370,579,11120),
1120(8902,595,11280),1140(9320,898,11451),1160(9562,898,11532),
1180(9735,898,11612);XZstops there through1200. Script800E4CAC again says
You Lost Venom atframe3878;by1210teleportedfailure. So secondjump advances
past earlier obstruction but does not yet solve next collision or chase pacing.
All comparisons retain exact rawpad state;no position-following or forcedregions.

NEXT BASELINE: native-chase-lower-roof-jump-script.txt. Inspect next collision
around native1160–1190 with one native game capture and bounded RAM state.
Prior run counter1160 corresponds roughlyVSync3788, archiveoffset2700;
confirm anchor and nativecounter in actual capture. Venom at1110 remains
(14266,1375,10758)whileplayer(8736,595,11210);heading still drifts+Z. Use
observed next geometry to decide whether to turn more toward+X or jump.
Do not declare traversed roofs/rendering correct from coordinate distance alone.
All three runs terminal,ownedcopies/caches cleaned,oneimage generated/inspected
this turn. No binary or ZIP changes. P0/P1 and cross-hardware acceptance open.


## Third obstruction native inspection

chase-native-third-obstruction:4100ticks,73.422s,exit0. Native capture03786
atcounter1160 inspected: Spider-Man running on a roof between utility structures,
a thin antenna directly ahead,Chase HUD/directionarrow,skyline/textures present.
No confirmed corruption in this frame; do not remove authored antenna geometry.
Snapshots1140/1160/1180 retain player(9320,898,11451),(9562,898,11532),
(9735,898,11612),Venom(15567,1366,10761),(16377,1106,10178),
(16832,1316,9851). Heading continues to drift away+Z. Antenna-jump is a local
traversal trial,not evidence that heading/overallpacing is already correct.
Control against prior lower-roof run:2440matched native keys,0differences,
36unmatched only in longer priorrun. Single screenshot inspected.

Next trial native-chase-antenna-jump-script.txt retains earlier inputs,
replaces terminal1095R2hold with1095:up+r2:44;1139:up:10;
1149:up+cross:6;1155:up:8;1163:up+r2:3000.
Normal jump consumed1150 before the knownlatecollision; no extra turn added.
chase-native-antenna-jump runs4500ticks,PadTrace/twoRAMsnapshots,noimages.


Antenna-jump trial:chase-native-antenna-jump,4500ticks,79.906s,exit0.
Firstinputdifference1140(R2release),firstposedifference1151(afterCross1150).
No earlier route changes.1160(9682,512,11582),1180(10456,621,11909),
1200(11439,531,12326),1220(12332,584,12701),thenXZstuckaround
(12638,12913)whileYdecreases andyaw0,consistent withwallclimbing.
Venom1200(17096,1157,10035),1270(19209,1145,10077),so player is still
headingtoo far+Z. LostVenomscript800E4CAC atframe4032;failureteleport by1300.
This clears prior localized obstacle but does not establish correct fullroute.

Nexttrial native-chase-antenna-right-script.txt addsRight to1139Up10 and
1149Up+Cross6,keepingjump/swingtiming andall earlierinputs unchanged.
chase-native-antenna-right:4700ticks,PadTrace/twoRAMsnapshots,noimages.
This is grounded pre-jump steering based on measured Venom direction;
no actorwrites,forcedregions,or changes togame-clock/cadence.


Antenna-right result:chase-native-antenna-right,4700ticks,83.656s,exit0.
First input/pose difference1140,matchingaddedRight.1160(9782,512,11303),
yaw2999;1180(10475,683,11405),1200(10955,907,11485),
1220(11625,2377,11599),1240(12365,4955,11725),thenFallFogscript800E4DFC
atframe3948 andfailureteleport(27502,1981,-24056). This turn does not yield
a usable route: it avoids the old wall heading but falls in a gap earlier.
Do not promote this trial as improved,or confuse its FallFog failure withthe
prior LostVenom failure. Exactinputs/rawtrace/RAMstates/comparison retained.

BEST CURRENT BASELINE remains native-chase-antenna-jump-script.txt (NO added
Right at1139/1149),which passes the antenna and reaches(12638,*,12913)before
wallclimb/LostVenom. NEXT inspect that wall aroundnative1220–1260 using one
native capture and state: choose an earlier direction change or swingrelease/
reattach based on actualgeometry. The 10updategroundRight addition tested here
is rejected. No successful fullChase or authoredbuilding traversal established.
Allthree runs terminal,ownedcopies/cachescleaned,oneimage generated/inspected
thisturn. No productionbinary/ZIPchange;allP0/P1scope remains open.


## Late wall native inspection and wall-jump trial

chase-native-late-wall:4200ticks,74.953s,exit0. Capture03946 atnative1240
inspected: Spider-Man clinging to tallbuildingface,Chase HUD/arrow,window
textures and sky present. This confirms wallclinging rather than only inferring
it fromstaticXZ/yaw0. A thin intermittent horizontal line is visible across
thewallaroundimageY330; preserve this potential geometry/raster seam for
separate original-asset/render-stage investigation. Cause not established;
do not call allgeometry correct or remove authored scenery from this image.
Snapshot1220player(12332,584,12701),Venom(17801,1207,10526);
1240player(12639,511,12913),Venom(17959,1316,10636);
1260player(12638,277,12913),Venom(18699,1071,10305).
Control againstantenna-jump:2540matchingnativekeys,0differences;90extra keys
only in longerpriorrun. Screenshot/checkpoints didnot altercomparedroute.

Next isolated trial native-chase-wall-jump-script.txt retains earlierroute,
replaces terminal1163R2hold with1163:up+r2:66;1229:right:2;
1231:right+cross:6;1237:right:6;1243:up+r2:3000.
This attempts a sidewayswalljumpaftercontact followedbyreswing,notanearlier
turn into the gap that failed previously. Trialchase-native-wall-jump uses
4700ticks,PadTrace,twoRAMcheckpoints,noadditionalimages.


Wall-jump trial: chase-native-wall-jump,4700ticks,83.235s,exit0.
First input difference1230,first pose difference1232. Player1240
(12639,775,12913),yaw4021;1250(12571,1078,12795),yaw0;1270
(12571,973,12795),1280(12571,843,12795),thenLostVenom by1290.
It detaches/repositions slightly but catches the same wall; do not adopt it.
Earlier route remained unchanged. No additionalimages.

Next controlled trial native-chase-corner-release-script.txt retains the
antenna-jumpbaseline and replaces1163R2hold with1163:up+r2:36;
1199:up+right:12;1211:up+r2:3000. This releases the swing before contact,
steersright whileairborne,thenresumesR2. It does not change the earlierground
turn or successfulantenna jump. chase-native-corner-release,4700ticks,
PadTrace/twoRAMsnapshots,noimages. Result remains pending.


Corner-release result:chase-native-corner-release,4700ticks,82.906s,exit0.
First input difference1200 as intended; NO player-position/yaw difference in
any matched native record versus antenna-jump baseline. Direction/R2 masks
are demonstrably consumed, yet the same swing trajectory and wallclimb occur,
followed byLostVenom. This rules out this specific airborne release/Right
combination, rather than proving the harness failed to send input.
Captured late-wall player.script_active is0 at1220/1240/1260, so do not blame
the authored building sequence for locking these movements.

NEXT: inspect native swing/input-state handling before further timing variants.
Track pad decoding fromfunc8006B514 throughfunc80017FC8/playerinput translation
into the large player updateptr8003EC34; resolve held/pressed/state gating for
releasing swing or changing direction. Do not mistake globals at800B4E64 for
pad records at800A4E64: immediate offsets alone are insufficient (an initial
text search also hit unrelated8002BD5C globalreads). A Cross-assisted release
is only a hypothesis,not a confirmed control mapping or pending acceptedfix.
Best current fixture remainsnative-chase-antenna-jump-script.txt; neither
wall-jump norcorner-release is promoted. Allthree runs terminal,copies/caches
cleaned,one nativeimage generated/inspectedthisturn. Potentialwallseam remains
unattributed. No productionbinary/ZIPchange; P0/P1/fullhardwareacceptance open.


## Airborne source audit and R1 trial (September 18)

Best remains native-chase-antenna-jump-script.txt; full completion unverified.
chase-native-corner-r1:4700ticks,83.344seconds,exit0,muted,no images.
Terminal hold replaced with 1163:up+r2:36;1199:up+r1:12;1211:up+r2:3000.
Against antenna-jump:2630 matched native keys,0unmatched,25input differences
starting1200,zero XYZ/yaw differences. Both traces contiguous and uncapped.
Reject this R1 interval as route improvement. See run route-state.json and
chase-native-corner-r1-comparison.json under proof_render/performance-repairs.

Generated-source audit (never hand-edit generated code):
- Initializer func_80047DF8_Impl stores pad800A4DF4 at player+F28.
  ptr_8003EC34 dispatches state+F3C==0x400 to L800435D8. Saved counters1200/1220
  state400,1240 state1,1260/1270 state10. Script+1A8 zero: not the special
  authored through-building sequence.
- CORRECTION: related-object +F84 is nonzero airborne: antenna1200=80129800,
  late-wall1220=80129DFC; zero at wall1240. Do not assume this pointer absent.
- State400 calls func_8004EA6C then func_8004DD4C. First returns early on EE8=0
  (saved state). Second requires900/F68/548 zero, pad+60(R1) held, successful
  raycast/normal checks. Success sets state40000. R1 trial1210 remains400 with
  all three initial gates zero; exact later geometric rejection uninstrumented.
- func_8006B514_Impl decodes16 physical then4 configurable face-action records.
  Type73 uses pad+150 masks; ordinary other types use+140. Both saved maps
  [40,10,80,20] mean records16/17/18/19=Cross/Triangle/Square/Circle here.
  +101/+111/+121/+131 are pressed bytes. PadTrace logs only16; RAM retains20.
- L80043D8C onward checks these face-action presses and can halve horizontal
  velocity, transitioning to state4. Check global/state gates before another
  Cross trial. No evidence supports a production movement/cadence patch.

Next establish the relevant state/action gate or improve the approach before
further timing variations. Wall seam in late-wall/frame_03946.png remains
unattributed. This trial is neither visual nor performance acceptance.


### Next ordinary-input gate established from saved RAM

At antenna-jump ram_03869.bin counter1200, global0x800B4F64=0,
player+1A8=0, state+F3C=0x400, +FB0=1. In L80043D8C onward, this global
value allows the configurable Cross pressed record+0x101 to take L80043E30,
which halves horizontal velocity and enters state4. R2 release/Right and R1
failed to alter trajectory; a fresh Cross edge at this point is a source-backed
next test, followed by ordinary steering/re-swing. This is a proposed button
trial, not evidence it clears the wall. Do not alter native state or cadence.


## Cross gate verified, route still fails (September 18)

chase-native-corner-cross:4700ticks,83.734s,exit0,muted,no images. Terminal
recipe1163:up+r2:36;1199:up+right+cross:6;1205:up+right:6;1211:up+r2:3000.
Against antenna-jump:2624matched keys,6unmatched baseline-only tail,169changed
records;first yaw difference at counter1200 (2812->3016). No earlier divergence.
Snapshot counter1209 state4 confirms the source-predicted transition; XYZ
(11745.86,890.03,12263.30),yaw3321. By1239 state10,wall position
(12171.53,979.56,12105.73). Counter1280 remains near(12173.26,458.62,12104.73).
LostVenom script800E4CAC atframe4027;counter1300 has failure teleport
(312,1097,-1793). Cross action works, but this approach still fails. Do not
promote to best route or change production movement. Best antenna-jump retained.
Evidence:chase-native-corner-cross-comparison.json and run RAM/route-state.json.
Next route reasoning should address the approach geometry rather than treating
button delivery or global clock speed as an established fault.


## Stock late-wall geometry audit (September 18)

Read-only source/RAM report:chase-late-wall-source.json. Mesh75 in original
l5a1_g.psx has32vertices; the entire256byte vertex block matches uniquely in
both late-wall counter1240 and corner-cross counter1239 snapshots, at common
asset load base800E956C. Object translation inferred as(13032,2805,12031),
packed rotation zero. Candidate near face edge in XZ is(12202,11964) to
(13121,13557),extending vertically fromY-2633 to5965. Face unit normal
(-0.866195,0,0.499707) matches native player surface normal(-3547,-1,2048)/4096.
Original path stops95.821units from that plane;Cross variant stops97.219units
from it. Thus both stops fit the same rotated stock wall and about96unit body
clearance, rather than two unrelated blockers. Render vertices are not corrupted
at these captures. Saved native collision-polygon pointerCA4 is zero, so this
is a plane/normal/proximity correlation, not a direct collision-polygon join.
It does not clear all geometry/material/seam defects or establish full route.

Near corner(12202,11964) suggests turning earlier to its lower-Z side; after
rounding it, the adjacent face slopes toward(13389,11279), so a straight +X
path can hit that face too. AABB-only steering would miss this rotated footprint.
Native-near-corner trial will break the swing/turn at1179, then jump again1199
before resumingR2 at1213. Ordinary button inputs only; no production physics edit.
Outcome must be inspected before promoting the recipe.


### Near-corner trial and roof polygon check

chase-native-near-corner:4800ticks,84.875s,exit0,muted,no images. Against best
antenna baseline2626matched keys,4baseline-only unmatched,209differences;first
yaw change1180,none earlier. Counter1189 state4 at(10762.65,1000.42,11846.21),
1209 stillstate4 at(11466.82,3106.26,11920.70). Thus proposed1199secondjump
occurred while falling,not grounded. By1239 state1 at(12312.62,3741.30,12349.42),
LostVenom script800E4CAC atframe4026. Reject recipe;no full-route progress.

chase-near-corner-horizontal-surfaces.json contains38 candidate horizontal
render quads near the approach, inferred from source object translations and
zero rotations;type3/35 records of28/32bytes only. Quad triangles012/123 tested
inXZ. Grounded positive-control(9562,898,11532) lies overmesh94 face93 atY995,
97units below actor origin. Candidate approach(11000,898,11000) similarly lies
onmesh94 face101. Falling1189/1209samples have no surface in this selected set.
This supports an open-gap explanation rather than the earlier AABB-based roof
assumption. It is a render-surface subset,not an exhaustive native collision map.

Next isolated recipe native-chase-roofward-script.txt starts from the earlier
antenna-right recipe and changes only1155:up:8 to1155:up+right:8, retaining the
turn through the initial jump toward lowerZ roof surfaces. Compare against
chase-native-antenna-right to locate first difference. Do not promote before
results; no state writes, clock changes, or gameplay patches.


## Roofward turn + grounded jump: new route candidate

chase-native-roofward:4900ticks,87.015s,exit0. ExtendedRight only during1155..1162
against antenna-right:2358matched,224new-only keys,first yaw difference1156.
At1200 grounded(10608.35,898,10810.49),1240 stops(10794.74,898,10736.75),
then LostVenom frame3983. No images. This reaches the intended lowerZ roof,
unlike antenna-right fall; it does not itself complete the route.

Native contact+CA4 at1240=800F7E7C maps exactly to mesh39 face0 start at asset
base800E956C. Face vertices:(11176,995,10528),(10843,995,10720),
(11176,772,10528),(10843,772,10720). It is a short original rooftop side face,
not the tall mesh75 wall. Contact at1200 maps to mesh39face13,possibly retained
from earlier contact; do not assume CA4 always names the currently blocking
face. Report:chase-roofward-contact-faces.json. Ground normalA4=(0,-4096,0),
BC4=0 at1200/1240; the observed Y898 matches roof995 minus97.

chase-native-roofward-jump adds release1197Up6,jump1203Up+Cross6,1209Up6,
1215Up+R2. Run5200ticks,92.187s,exit0,muted,no images. Against roofward:
2582matched,821new-only keys,first position difference1205. Trace is contiguous
but reaches4096record CAP atnativecounter1799/tick5064; later route is not pad
traced. Do not call the unmatched extension a baseline mismatch.
Checkpoints:
-1211:(10883.18,616.75,10696.60),state2,script0.
-1261:(13158.00,568.08,9753.18),state400,script0;Venom script advancing33->35.
-1331:(14702.94,257.00,9150.41),state10,script0;Venom waiting at
 (19592,1316.44,9906),script words[0,11,35,0,11,36].
-1381:(15219.03,1332.00,10035.58),state80000,script0;sameVenom wait.
-1500:(15557.09,928.95,10624.30);1600:(15825.05,593.42,11090.27);
 1750:(16429.54,593.88,12141.27). Ground/wall/swing cycles remain to diagnose.
-FallFog script800E4DFC atframe5136,after trace cap. No LostVenom marker in run.

Promote roofward-jump as BEST ROUTE CANDIDATE, not successful completion. It
clears old tall-wall approach and reaches a later authored wait, but its later
fall and entry into the actual through-building player script remain unresolved.
No production game/clock/scenery edits. User ZIP unchanged by button-fixture work.
Next inspect contact/geometry aroundcounter1300 and route to the waiting Venom
at19592,9906; preserve successful earlier inputs rather than restarting the path.
Use bounded checkpoint snapshots (or later-start trace support) if needed; don't
silently rely on records beyond4096. All three trial processes terminal/cleaned.


## Next narrow roof identified from native contact (September 18)

roofward-jump counter1331:BC4=0,surface normal(0,-4096,0),CA4=80110C80.
That pointer maps exactly to original mesh98face62 at base800E956C, horizontal
quadY354 withXZ(14565,9189),(14678,9386),(15198,8824),(15312,9020).
PlayerY257 is97 above it: this checkpoint is grounded on a narrow roof,not wall
attached. Prior player remained near(14637,453,9036) through1330,then the native
climb/transition completed and1335player=(14733,257,9202),yaw2394;1340position
(14784,257,9289),sameyaw. ContinuingUp carries across the roof's short edge.
At1381CA4=80111344 maps mesh98face117,vertical betweenY~542 and1429 near
XZ(15348,10032)..(15125,10160). Exact report:chase-next-roof-contact-faces.json.

One new recipe preserves all earlier inputs:1215Up+R2:118,1333Up+Right:4,
1337Up+Right+Cross:6,1343Up:8,1351Up+R2:3000. Goal:jump while still on the
narrow roof toward Venom's waiting position rather than stepping over the edge.
No actor/region writes; outcome pending in chase-native-narrow-roof-jump.
Read-only old GameTrace diagnostic mapping associates wait35 with original
region34, but region position/activation semantics are not established here.
Never pulse it to certify the ordinary route.


### Narrow-roof jump result; held-turn comparison pending

chase-native-narrow-roof-jump:5600ticks,98.125s,exit0,muted,no images. Against
roofward-jump3403matched keys,zero unmatched within the capped traces,first yaw
difference1334 (2394->2496);931changedrecords. Both hit4096record cap.
1339player(14794.13,257,9267.04);1399(16528.56,-2.51,11348.26);
1499(17534.45,-838.91,12689.01);1599(17534.44,-2139.71,12688.97).
All four saved player scripts zero. Venom remains at19592,1316.44,9906 with
script35/36,so fartherX does not establish route/region progress. LostVenom
atframe5409. Reject as successful traversal; path still goes too far+higherZ
then climbs another tall wall. Compare records only through the cap,not fullrun.

Single-change candidate narrow-roof-turn retainsRight during1343..1350
instead of releasing toUp,then resumes1351Up+R2. Same approach as the earlier
roofward turn correction,keeping all preceding successful buttons intact.
Result pending; no production physics or native state modification.


## Held narrow-roof turn: latest best candidate

chase-native-narrow-roof-turn:5600ticks,98.047s,exit0,muted,no images. Against
narrow-roof-jump3403matched keys,zero unmatched within capped traces,911changed
records;first yaw difference1344(2741->2945),none earlier. Both cap4096records.
Only input change is1343Up+Right:8 instead ofUp:8,then1351Up+R2:3000.
1399player(17390.01,-59.58,9742.39),state400,BC4=0.
1499(18050.16,-177.75,10101.84),state10,BC4=1,normal(3547,22,-2048).
1599(18048.39,91.00,10102.75),same state/normal.
1699(18305.79,18.72,9969.00),state10,BC4=1,normal(-3547,-10,2048).
All saved player scripts0 and CA4=0. Venom remains(19592,1316.44,9906),
script[0,11,35,0,11,36]. No LostVenom/FallFog marker before bounded exit;
this is not a completion marker. Pad trace does not cover the entire run.
Reports:chase-native-narrow-roof-turn-comparison.json,
chase-near-venom-contact-faces.json (no face join:CA4zero),run route-state.json.

Promote native-chase-narrow-roof-turn-script.txt as best current candidate:
closer to the wait point and avoids the prior highZ wall detour. Remaining issue
is wall attachment nearX18050/Z10102 then opposite normal near18306/Z9969.
Inspect surrounding original surfaces and required ordinary entry into region34
rather than forcing it. Never equate no failure during a bounded run with success.
Two runs this turn terminal/owned caches cleaned. No shipping code/ZIP change.


### Chase entry trigger source audit and swing release (September 18)

Read-only saved RAM/original asset audit: `proof_render/performance-repairs/chase-entry-trigger-source.json`. Correct native GP is 0x800B47F4 (generated startup); trigger table 0x800E1C18 has 344 records. Trigger 34 at 0x800E2670 is type 6, registered hash 0x6C28CB52, NOT an arbitrary coordinate trigger. Native hash table 0x8011DF74 maps mesh 0 to this hash. `func_800806AC` writes object+0x16 mesh index to 0x800B58AC = GP+0x10B8; `func_800733D0` uses it to look up the hash and call `func_8005AAB0`. Original mesh 0 has four vertical trigger quads at Y=489..2185, rotated XZ footprint (19679,9487), (19043,9855), (19374,10429), (20011,10062). Object rotation is zero; translation and vertex bytes match saved RAM. This identifies the authored volume; it does not prove ordinary activation. No forced triggers, RAM writes, or production geometry changes.

`chase-native-entry-drop` (5600 ticks, 101.781s, exit 0, muted, no images) changes only the prior narrow-roof-turn tail: 1351 up+r2 for 48; 1399 up+cross for 6; 1405 up for 54; 1459 up+r2. Compared with narrow-roof-turn, 3403 native keys match, 799 differ, first pose difference at counter 1401. It clears the blocking wall: counter 1419 (17958,257,9840), 1439 (18298,257,9906), 1499 (18955,257,10091), 1599 (21904,443,10182). Player script remains 0 and Venom waits at script35/36: player crosses ABOVE the entry volume. Keep this as a useful branch, not completed Chase acceptance. Native pad logs cap at 4096; later snapshots are separate read-only evidence. Run was initially produced under a duplicated relative output prefix and moved, after exit, into the normal evidence directory; launch metadata may retain that original path.


### Side-entry trial result (September 18)

`chase-native-entry-side` completed its 5400-tick bound in 94.313s, exit0, muted, no images; all owned game processes exited. Relative to entry-drop, only tail after1405 changes to up+left20, up20, up+right20, then up+r2 at1465. 3403 comparable keys,787 differences, first pose difference counter1406. At1499 player(19127.46,257,10039.72),1549(20124.73,257,10111.60),1599(22982.32,594.81,9714.30): again above volume while crossing it, then below its top only after leaving its footprint. Player script0; Venom remains35/36. This trial did NOT solve entry; do not replace the entry-drop comparison branch with it or count exit0/no failure as completion.

Next investigation: map original supporting roof edges and the side/window access around mesh0. Both release and modest left/right steering stay on the continuous Y354 roof (player centerY257). Determine an actual route off that roof and into the Y489..2185 trigger sides before another input trial; do not keep making small heading changes above the volume. The trigger dimensions/hash source are now established, while the playable entrance path and full scripted traversal remain open. P0 long stalls/audio interruptions and both-game acceptance are unchanged by these route-only diagnostics.


### Lower entrance floor source map (September 18)

`chase-entry-roof-surfaces.json` records the original nearby horizontal faces and point-in-triangle checks. These are render geometry with native vertex coordinates, not a complete collision map. At XZ(14733,9202), mesh98 face62 is the Y354 ledge. At(15000,9400) and(15500,9600), only mesh124 face10 atY1429 covers those points in this selected horizontal set: there is a roof gap before the covered building. Its corners are (14901,9257),(15348,10032),(15311,9020),(15758,9795). At(17500,10000),(18500,10000),(19500,10000), the Y354 roof and Y1429 interior floor overlap in XZ. Thus approach below the roof from the earlier gap; descending only near waiting Venom crosses the volume too high. The native-chase-interior-floor candidate removes the earlier jump: after unchanged inputs through1332, 1333 up+right12 then1345 up. It remains normal input only; its result must establish whether the inferred lower path is actually traversable.


### Lower-floor route results (September 18)

`chase-native-interior-floor` (5700ticks,112.860s,exit0,muted,no images) naturally lands on the lower floor:1359(15270,1332,9670),1399(15842,1332,10254),1459(16397,1332,10783). This establishes the lower approach with ordinary input, not trigger activation. By1759 the route leaves the corridor and wall-attaches at(18199,2457,12305). Script remains0; Venom waits35/36. 3403 matched keys versus narrow-roof-turn,923differences, first pose change1339.

`chase-native-interior-east` (6100ticks,106.750s,exit0,muted,no images) holds up+right from1359. It remains on lower floor at1400(15958,1332,9610), but held right keeps rotating (yaw3024 at1370,3157 at1380,3435 at1400,3713 at1420). At1500 it falls near(15838,1960,7943), then FallFog script800E4DFC atframe4523. Rejected as an overlong turn. Later snapshots have no player; do not treat the counter reset as completion. Next candidate limits this right input to12updates then uses up. `chase-interior-turn-witness` also enables full P0 witnesses; do not claim route snapshots/pad logging are an uncontaminated performance baseline.


### Instrumented lower-floor turn outcome (September 18)

`chase-interior-turn-witness`:5700ticks,106.219s,exit0,trace0,all3witness helpers0. Input1359 up+right12 then1371 up. Snapshot1400(15793,1332,9975),1500(17354,1332,11199),1600(17798,1332,11371); FallFog script800E4DFC atframe4824, failure teleport by1700, then player removed/counter reset. Player script0 at pre-failure snapshots and Venom35/36. Compared with interior-floor:3230matched keys,174base-only due failure,706differences,firstpose1360. Neither capped logs nor failure resets count as route completion. Next inspect input/camera mapping for correct corridor navigation; geometric lower-entry hypothesis is already demonstrated.


### Native camera-relative steering source (September 18)

Read-only source/snapshot report: chase-camera-heading-source.json. In ptr_8003EC34, labels80044EDC..80044F90, camera pointer800B5834+23A supplies the base angle. When player8F8=0 (all examined floor snapshots), playerF52 is added before func80053898; func80053A54 supplies smoothed player yaw. Native input processing8004AD68..8004ADD4 computes F52 from pad axes. Saved Up givesF52=0; Up+Right=512 (45deg in4096-turn units). Lower-floor base1359 yaw2553/camera2552; held-right branch1400 yaw3435/camera2994; short-right branch1400 yaw2639/camera2631. Thus holding right keeps moving the camera reference, and returning to Up preserves that reference instead of the transient player yaw.

Candidate native-chase-camera-aligned-script holds1359 up+right42 then1401 up, estimating release camera~3008 toward waiting Venom. Native source/saved states motivate this timing; it is not a physics/camera patch or waypoint follower. Horizontal source-face checks along an estimated line show floor coverage except nearXZ(18400,9788), so watch that crossing; render-face omission alone is not collision proof. The bounded witness run must establish actual motion/trigger activation before calling this successful.


### First normal trigger34 activation (September 18)

`chase-camera-aligned-witness` / native-chase-camera-aligned-script: holds1359 up+right42,1401 up. Lower-floor checkpoints1400(15958,1332,9610),1500(16969,1332,9658),1600(18578,1332,9606). Original trigger script800E2680 fires atframe4773/nativecounter1653, alongside800E2790 and800E287C. Ordinary native input only; this is direct activation evidence, not inferred from proximity. At1700 player(20371,1417,9630), Venom(18836,1111,8892),script37/38. FallFog4963 after overshooting. Entry heading remains near3059; return path is now required, because Venom turns back west through building toward waypoint37(16291,1372,8992),38(15571,1359,9453), and next trigger39(mesh1). Next candidate1401up254 then1655down, preserving exact inputs through the verified activation. No player forced-script acceptance yet; player script0 at the sampled checkpoints. P0 witnesses captured scheduling-related stalls in this run; see repair handoff.


### Second normal interior trigger (September 18)

`chase-return-corridor-witness`:6100ticks,111.375s,game/trace/all3helpers exit0; runtime19721events,lost0. native-chase-return-corridor-script adds1655down after preserving1401up254 and all prior successful inputs. Trigger34 againframe4773/native1653; trigger39 script800E26D8 nowframe5235/native1884. Checkpoints1670(19332,1332,9630),1700(18732,1332,9630),1750(17919,1332,9608),1800(16919,1332,9608),1850(16293,1332,9658),1900(15499,1332,9634). All saved player script flags0. Venom progresses36/37,37/38,38/wait,wait/41,41/42,42/wait. Camera3051 and player yaw1011 remain stable throughout Down, inputF52=2048: reverse movement does not continuously rotate the camera like held diagonal right. FallFog at5445 after missing next turnaround.

Max positive-counter interval43.7914ms,103audio observations,0stopped after5s. Only >100ms intervalstartup present63 at248.565ms; full recurrence review remains separate from this clean gameplay repeat. No new native images, no visual completion claim. All task-owned processes exited.


### Retained wall seam reinspection (September 18)

Reinspected existing chase-native-late-wall/frame_03946.png (no new image generated): a thin broken yellow/dark horizontal seam is visible around imageY328..341 across the facade. Its run uses older SHA8fa3009f444a6c95130d2cc8f53a41d2904d801c75f9140bf808ec46fffdfb1d, so reproduce on current canonical build before attributing current behavior. Original mesh75 wall bands use shared indexed vertices at localY-5438,-5177,-3772,-2361,-955,448,1860,3160; this does not itself join the visible seam to a primitive or establish its cause. Keep texture sampling and geometry projection as hypotheses, not confirmed fixes. Exact old ordinary-input schedule is extracted into native-chase-wall-seam-script.txt for a bounded same-pose current-renderer comparison. No source geometry/shader edit justified yet.


### Northern corridor and stock wall (September 18)

`chase-north-corridor-witness` / native-chase-north-corridor-script:1655down231,1886up+right24,1910up.6500ticks117.25s,game/trace/allhelpers0,19912events lost0. Reaches1909(16018,1332,9466),1949(16722,1332,9109),1999(17594,1332,8759),2049(17647,1332,8775),2299(17733,1332,8933). Venom waits at(17437,1318,7897); current task words[15,3,0,0]. Advanced script pointer[0,12,50,...] is NOT evidence of triggering44; no script800E2754 activation logged. Player script0 in all snapshots.

2049 contactCA4=80112370 joins original mesh99 face58, vertical Y542..1429, XZ end(17677,8689) to(17790,8885), near stopped player. Ground state10,BC4=0,normal(0,-4096,0),yaw3319,camera3311. The older1999 pointer80110EE0 is mesh98face81 behind the player, so stale CA4 must not be treated as immediate collision proof. Next native-chase-north-wall-end script changes only1990right18 then2008up to get west/north around the short wall end; it must be tested, not assumed. Max positive-counter44.291ms,109audio records,0stoppedafter5s. No new images for this route.


### Normal trigger44 and authored script entry (September 18)

`chase-north-wall-end-witness` / native-chase-north-wall-end-script changes1990right18 then2008up.6800ticks122.172s,game/trace/allhelpers0,20393events lost0. At1999(17535,1332,8698),2019(17417,1332,8335), then2039(17291,1004,7898) with player script1. Native trigger44 script800E2754 fires atframes5503 and5517. Player then follows the authored movement:2059(17418,793,7429),2099(18007,891,5273),2198(18135,13,4491),2265(18041,-153,4046),2398(18181,1076,3502). All later snapshots script1. This proves ordinary-input entry into the authored section, not completion.

Player1B4 cursor progresses from800E4A98 to800E4AA4, then800E2B3E (exact guarded prefix0,17,30,3,10,60,520,1,54,20,3,16,2),800E2B4A,800E2B60. Script commands advance; final saved position alone is not proof of a stall. Measured native counter1800..1990:190presentations/6.3338775s,33.3362ms perupdate. Counter2220..2390:255presentations/8.5004827s,50.0028ms perupdate. Thus guarded20Hz authored-script cadence activates through the real initializer while presentation remains30FPS.

Max positive-counter frame45.0093ms,114audio observations,0stoppedafter5s; onlystartup173.7612ms >100. No images in short run. Longer same-input witness chase-authored-sequence-long is pending with2native captures to verify continuation/end; do not mark P1 complete before inspecting its outcome.


### Authored sequence continuation and ordinary-control failure (September 18)

`chase-authored-sequence-long`:same ordinary north-wall-end fixture,10000ticks181.344s,game/trace/allhelpers0,20587events lost0. Checkpoints2199(18135,13,4491),2332(18029,-193,4079),2465(18181,935,3502),2599(18181,1076,3502),2732(18186,1074,4516),all player script1. By2909 player(19207,257,8833),script0,Venom(17099,251,10968),scriptpointer[0,12,216,0,11,217]. LostVenom800E4CAC firesframe7996; next snapshots show removed player/counter reset. Last duplicated positive-counter presentation belongs to2778; afterward30Hz update cadence resumes. This brackets successful authored script exit, not a completed chase.

Inspected both generated native captures in order:frame06285 shows letterboxed exterior building/window cutscene, facade/sky/broken windows and flying triangular debris, no visible player in this individual frame. frame08685 showsGameOver/Retry/Quit andYouLostVenom overcity. visual-review.json retains limits; no claim everyframe/material/geometry is correct and no audible review (muted). One earlier retained seam image was also reinspected this turn; no further images generated.

2732 snapshot:playerYaw2059,camera1026,inputF52=1024,state10,scriptcursor800E2BCA.2909:yaw2389,camera2389,F52=0,state10,cursor800E2BE6. Use closer handoff snapshots before another steering guess; an up tail from2008 remains active after script exit. Gameplay positive-counter max60.7531ms;179audio observations,14underruns after5s, with later game-over-phase shared-host stalls retained. See performance repair handoff.


### Exact normal-control handoff (September 18)

`chase-control-handoff-witness` truncates2008up at2780, then no input.8400ticks155.766s,allgame/trace/helpers0;20295events lost0. At2766 script1,position(18153,683,7006),yaw0,camera1300. At2780 script0,position(18153.52,598.48,7007.03),camera2275,state10,BC4=1,normal(-2079,-37,-3529). At2800..2960 it settles at(18153.39,574.82,7006.81),state1,stillwallattached,camera2366. Thus neutral stays on the wall; authored script has ended, not hung. LostVenomframe8058.

Do not overinterpret prior Up-only failure as wrong direction from the very first post-script update: Venom first goes toward waypoint212(18907,225,9492), then214(16233,237,11701), then west toward216(10936,953,10226). Up initially faces roughly toward the first target; walking is too slow and the later west turn is missing. New trial native-chase-post-building-swing adds2780up+r2. Tight checkpoints must establish how it leaves the wall and where to turn afterward. Post-building records210..239 are decoded in chase-post-building-trigger-records.json with absolute4-byte alignment after linkcount; record214 starts ataddressmod4=2 and needs padding.

Read-only tools/chase_route_state.py now includes playerstate,wall flag,normal,lastcontact,yaw,inputangle,scriptcursor/words and camera input-reference yaw. Known2780 fields were checked against prior direct RAM decoding. CA4 is explicitly lastcontact, not automatically current blocker. No shipping executable change. Gameplay max43.8143ms;17audio underrunsafter5s occurred in a run with early menu stalls. The >1s intervals atpresent181/202/239 have covered traces,noGC,hostCPU82-100%,queues33-56/47/40 and independentheartbeat delays. All are retained; no new images.


### Post-building R2-only outcome (September 18)

`chase-post-building-swing-witness`:9500ticks167.5s,game/trace/allhelpers0;20266events lost0. Same prefix,2780up+r2.2790(18155,472,7009) wallstate10/yaw0;2830(18418,257,7465),2870(18818,257,8158),2920(19317,257,9024),2970(19816,257,9890),allgroundstate10/yaw2389/camera2389. R2 alone never starts an airborne swing; ground displacement~20units/update. LostVenom7998. Prefix/script exit still valid; next jump then swing. No new images. Review-summary.json records bounded performance/audio/trace scope.


### First post-script jump/swing (September 18)

`chase-post-building-jump-witness`:9500ticks173.266s,game/trace/allhelpers0,19983events lost0. Input2780up+r2for35,2815up+cross6,2821up+r2.2829(18456,-136,7530),state400,distance2052;2869(19266,257,8935),groundstate10,distance1853;2919(19765,257,9801),ground,distance3179;2969(20369,1307,10849),fallstate4,distance8022. LostVenom7990. Both yaw/camera2389 through these samples. The first jump and swing work but land early; next trial needs southwest relaunch around the first landing, not simply longerUp. No new images. Gameplaymax45.8162ms,167audio samples9underrunsafter5s; later menu stalls including1385/1485/1356ms are preserved with full witnesses in review-summary/stall-correlation.


### Post-building attachment gate, source audit (September 18)

`chase-post-building-attachment-source.json` preserves the generated-source
references and read-only saved fields. Atcounter2920 in west-wall-jump, R1=0,
R2=1,gates900/F68/548/1A8=0,state40000,animation270,target EDC=(72470207,
-718229,41457786) fixed4096,normal(-2048,-1,-3547). This is an acquired vertical
surface. func8004E01C accepts R2 (pad+70), performs geometric checks and stores
that target/normal before entering40000. Thus the sampled detached state is not
proof that a plain wall jump remained in free flight. Candidate
native-chase-wall-jump-delay-swing-script.txt delays the2916R2 press until2940,
keeping ordinaryUp between; no native state/actor/timing writes. Check its actual
result before promoting. func8004DD4C is the separate R1 acquisition path;
40000 alone does not identify which entry was taken.
