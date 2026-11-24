# Main menu, orbit speed and Big Time repairs — November 20, 2026

Both games now keep only the game name in the window title, use a maximum
gameplay right-stick orbit speed of 180 degrees/second (previously 90), and
offer EXIT GAME above Spider-Man on the main menu. Costume-viewer rotation
remains at 90 degrees/second.

## Main menu and title

`HostWindow.ApplyTitle` assigns the game name without a frame-rate suffix. The
frame counter and its per-frame call have been removed from the shared runtime.
Native framebuffer captures omit the Windows title bar; this change was verified
in source and the successful release builds, without operating the desktop.

`NativeMainMenu` calls each game's own text and animated highlight functions.
EXIT GAME is centered at native coordinates (256, 12), above the character's
head (raised from Y=22 following the November 21 overlap report).
Up from Continue/Training, or Down from Options/Gallery, selects it. Cross or
Start performs normal runtime shutdown and exits the process. Triangle returns
to the ordinary choices. The original eight-entry model/preview table stays
intact; Exit uses separate focus rather than an invalid ninth table index.

The menu hooks are scoped to SM1 function `802562B4` and SM2 function `80241E30`.
The recompiler now respects an optional `function` on instruction/branch hooks,
avoiding duplicate patches in overlapping recovered SM2 functions. Unscoped
hooks retain their existing behavior.

## Big Time head seams

The head geometry was already closed. The converted, quantized face UVs sampled
black pixels outside the head's atlas island at the crown and rear seam. The
repair insets those boundary coordinates for both `smu-bigtime` (green/Stealth)
and `smu-bigtime-red` (red/Sonic).

Each actor changes exactly 84 bytes across 70 face corners. Every non-UV byte,
including geometry, normals, rig, animation data, materials and embedded fallback
textures, is preserved. External texture artwork is unchanged. Both games use
these shared actors.

`tools/retarget/repair_bigtime_head_uv.py --apply` reproduces the repair from the
known original actor hashes, verifies the expected resulting hashes, and updates
the conversion reports. It refuses unknown inputs. Without `--apply`, it audits
the actors without writing them.

| Actor | Repaired SHA-256 |
| --- | --- |
| Green | `e76bfb409e1e35408113c33e4976de8a12d557cdc92c3f799a8c86da11c96118` |
| Red | `c2c0bb5b483ad1a7a438e92a78c8c1a03f5d8529aa44fc148a3af92cc13e635f` |

## Verification

All artifacts are under `.local/menu-suits/`. Tests used hidden game windows,
muted audio, isolated settings, disabled memory cards and process-local input.
They never sent input through Windows or modified the staged saves/settings.
Native captures were inspected individually for the expected menu, model,
background, HUD and camera behavior; a successful process exit alone was not
treated as visual proof. Audio was deliberately muted and was not evaluated.

The focused executable regressions passed 167 checks:

- 63 per game for orbit timing, guards, directions, wrapping and native camera
  geometry, including 180-degree/second timing at 20, 30 and 60 updates/second.
- 32 for menu focus, navigation/release handling, drawing, register preservation
  and both games' bindings.
- 9 for emitted instruction/branch patches, including function-scoped overlap.

Logs are `orbit-sm1.log`, `orbit-sm2.log`, `menu-regression.log` and
`instruction-regression.log`. The actor byte audit is `uv-repair.jsonl`.

SM1's `final-viewer-sm1` and `final-viewer-red-sm1` each contain four inspected
native frames covering the repaired head in the animated viewer. The four
`final-gameplay-sm1` frames show the camera moving around the player in `l1a1_t`
and returning near its starting angle after a full orbit. These used build
`79eafda2081a43496c9e46743e1270da8fee8ebc57d4251ca56fdc6d1ba892df`;
the subsequent SM1 change only raised the Exit label from Y=46 to Y=22.

The final SM1 `release-exit-sm1` run verifies that corrected layout, navigation
back to the retail choices, reselecting Exit from Training, and an actual
`EXIT GAME confirmed` shutdown. All three captured frames were inspected.

SM2's initial `final-*` runs established rendering of the repaired suits and Exit,
but did not establish right-stick motion: its new replay listener lacked an
archive-load trigger. The trigger was connected in both archive lookup paths.
Those initial SM2 runs are retained as diagnostic evidence, not orbit acceptance.

The final SM2 runs use build
`8ac12d84eebfcf6fa94391721699aac6a51ff72e5b654395057a14b6d6e368b1`.
All 15 captures across `release-exit-sm2`, `release-viewer-sm2`,
`release-viewer-red-sm2` and `release-gameplay-sm2` were inspected individually.
They show the final Exit layout and shutdown, both repaired heads during viewer
rotation, and the camera orbiting around the player in `e1m0_t` before returning
near its original angle. The gameplay log records the replay starting at frame
2843; captures 2838, 2873, 2908 and 2978 show before, during and after that input.

These short sequences verify the requested changes in the tested flows. They
are not a complete playthrough or an exhaustive camera/wall-collision test.

Selected native proof frames, relative to `.local/menu-suits/`:

| Check | Image |
| --- | --- |
| SM1 selected Exit | `release-exit-sm1/frame_00853.png` |
| SM2 selected Exit | `release-exit-sm2/frame_01077.png` |
| SM1 green crown/back | `final-viewer-sm1/frame_02213.png` |
| SM1 red crown/back | `final-viewer-red-sm1/frame_02213.png` |
| SM2 green crown | `release-viewer-sm2/frame_02187.png` |
| SM2 red crown | `release-viewer-red-sm2/frame_02186.png` |
| SM1 opposite side of player | `final-gameplay-sm1/frame_03285.png` |
| SM2 opposite side of player | `release-gameplay-sm2/frame_02908.png` |

## Staged deployment — November 21, 2026

Both existing portable installs in the repository root were updated:

| Executable | SHA-256 |
| --- | --- |
| `Spider-Man/SpiderMan.exe` | `1ba44918328029fb98b92a593f4909604418e1ee58112c0be8f7c09fad3e67a1` |
| `Spider-Man 2/SpiderMan2.exe` | `8ac12d84eebfcf6fa94391721699aac6a51ff72e5b654395057a14b6d6e368b1` |

Each EXE matches its verified final native Exit run. Deployment changed five
files per install: the EXE, two Big Time actors and their conversion reports.
Hashes verified that the other 1,803 SM1 files and 1,182 SM2 files stayed intact,
including settings, saves, other suits and disc assets. Evidence is
`.local/menu-suits/deployment.json` with complete before/after inventories beside
it. The completed SM2 orbit/deployment item was removed from TODO.

Automatic approval review rejected the recursive cleanup of the task's temporary
`baseline-sm1`, `sm1` and `sm2` directories with "blocked by policy" and no
detailed reason. Those copies remain under `.local/menu-suits/` (about 601 MiB
combined); the deployed installs and retained proof files are unaffected.

## Exit head-clearance correction — November 21

The shared label anchor is now Y=12 instead of Y=22. This is a fixed menu
coordinate; it does not follow the animated character. The 32 menu regression
checks pass with both games' bindings.

SM1's `.local/n64-shadow/menu-clearance` run captured 15 frames across the
animated preview, selected and unselected Exit, and confirmed native shutdown.
Every capture was inspected individually. The label stays above the head and
its glyphs remain on screen; the Surveillance suit's unusually tall back
attachments can cross the label in some poses. This correction does not resize
or alter costume geometry.

That correction is deployed with SM1's completed N64 shadow release, SHA-256
`eb5a3ebc728b4d53f0220a7c2137e1d55fa3c0a90946dce969a0cfd09d6d507b`.
Only the staged EXE changed; all 1,807 other files are unchanged.
[Selected Exit proof](../.local/n64-shadow/menu-clearance/frame_01890.png),
[menu run record](../.local/n64-shadow/menu-clearance/result.json),
[deployment record](../.local/n64-shadow/visibility-deployment.json).

SM2's `.local/menu-suits/clearance-sm2` run captured 13 frames spanning the Big
Time preview animation and both Exit states. All 13 were inspected in sequence:
the head stays below the label, the glyphs remain on screen, the native menu
background and choices remain visible, and Cross confirms normal shutdown.
The run was hidden and silent with process-local input and isolated settings.

Deployed SM2 SHA-256
`911a743de7196863e15ce7a8390d30f845072a4868db4988b0dc47da356cd75e`
to `Spider-Man 2/SpiderMan2.exe`. Only the EXE changed; all **1,186 other staged
files** are unchanged. The existing `.local/menu-suits/sm2/SpiderMan2.exe` is
the previous release and remains available as rollback.
[SM2 selected Exit proof](../.local/menu-suits/clearance-sm2/frame_01837.png),
[run record](../.local/menu-suits/clearance-sm2/result.json),
[deployment record](../.local/menu-suits/clearance-deployment-sm2.json).
