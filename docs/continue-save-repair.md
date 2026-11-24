# Continue and costume-change repair — November 16, 2026

Spider-Man now restores the most recently saved slot at startup. Loading another
slot manually replaces that startup selection. The reported crash after changing
costumes, loading slot 1, and selecting Continue was traced to stale head geometry.

## Cause and changes

The native head-expression routine (`800472C0`) uses two fixed 38-vertex PS1 target
tables and a head backup at `GP+AB4`. That backup is allocated only once by
`80047218`; changing costumes does not refresh it. The previous compatibility
post-hook restored non-retail heads from that stale backup. A coordinate from a
previous model could overwrite the new head's shared-vertex offset. The failing
first head vertex in the supplied log then referenced `0x79BA8194`.

The new instruction branch at `80047368` skips the incompatible geometry loop
before any target or backup reads. It retains native expression state updates,
the native epilogue, and the original behavior for 38-vertex heads. It does not
change suit assets, textures, powers, or the selected external suit identity.

`ContinueSave.Startup` runs once at the title-shell entry, before its model load.
It reads enabled memory cards, validates native slot checksums, restores the save
and score table, and applies the original settings routine. It does not flush or
rewrite the card. Returning to the menu cannot overwrite current progress or a
manually loaded slot.

PS1 cards contain eight 180-byte slots but no per-slot save timestamps. A portable
`carda.sav.continue.json` file beside the card records slot number, SHA-256 and UTC
save time. Both native save-copy paths are tracked; metadata changes only after
the native write reports success and the disk bytes match. Loading a slot does
not change save-time ordering. Disabled cards, invalid checksums, and stale or
malformed metadata are excluded. Cards without matching history use the last
occupied valid slot and explicitly log that legacy fallback.

For this staged card, the user confirmed slot 3 (`abc`, `L5A4_T`) was saved last.
That confirmation seeds the initial metadata; slot 1 (`a`, `L1A4_T`) was the slot
used in the crash reproduction.

## Verification

- In-memory regression executes the real recompiled checksum, head-expression,
  and shared-vertex transform routines. Replaying the old restore reproduces the
  exact `0x79BA8194` exception. The patch preserves every coordinate/joint byte of
  the 124-vertex staged head for all three expression modes, handles an unreadable
  stale backup, and retains the retail 38-vertex morph. It also checks save-order
  persistence, failed writes, disk mismatches, invalid metadata, disabled cards,
  manual selection, and preservation of card bytes. **34 checks pass.**
- Hidden, muted portable-EXE run: slot 3 autoloads, Continue displays **Sewer
  Cavern**, and selecting it reaches `l5a4_t` gameplay. Native captures show the
  character, environment, transition and HUD; exit code 0.
- Hidden, muted reported flow: start with Spider-Ham, select Steve Ditko in the
  costume viewer, manually load slot 1, acknowledge **LOAD SUCCESSFUL**, then
  Continue into **Stop the Bomb!** (`l1a4_t`). Native captures show the changed
  costume, selected save, updated Continue destination, introductory scene and
  gameplay HUD. No crash; exit code 0. The isolated card remains byte-identical.

Tests use copies of the user's card and process-local input only. No desktop
capture, host input, visible game window, or audible playback is used. Audio was
intentionally muted and is not acoustically validated. These checks cover the
reported loading paths, not a complete playthrough or every suit combination.

Private evidence stays under `.local/continue/`: `crash.log`, `tests.log`,
`auto-final/`, `manual-final/`, and deployment manifests. The earlier `manual/` observation
ended in the costume viewer and is not counted as completed flow validation.
Native screenshots were reviewed individually, including the transition captures.

The final portable EXE passed both flows: startup in 40.03 seconds and the
costume-change/manual-load flow in 71.03 seconds, both with exit code 0. The
captured slot-1 destination is visible in `manual-final/frame_02286.png` and its
gameplay/HUD in `manual-final/frame_02529.png` and `frame_03249.png`.

The recompiler emits both save-copy instruction hooks in 15 overlapping native
function bodies. Build validation now permits these ordinary instruction-hook
duplicates while still requiring a unique function for branch hooks.

## Deployment

User acceptance on November 16, 2026: loading a save, changing a costume, and
then loading into a level all worked in the staged SM1 install. This adds manual
confirmation of that sequence to the automated flows above. The completed work
stays out of TODO.

Replaced `Spider-Man/SpiderMan.exe` with the tested self-contained EXE and seeded
`Spider-Man/saves/carda.sav.continue.json` for the user-confirmed slot 3. All 1,806
other existing staged files were verified unchanged, including the original
memory card, settings and all suit files. Both resolved entries were removed from
TODO. The previous EXE is retained in `.local/continue/SpiderMan.before-continue.exe`.

EXE SHA-256: `b5d3f42bc6b0303447d0319582cc2a4bb088c1535f3c0d8d9fefcad0fbac299d`.

Preserved card SHA-256: `d713f1dee32029b21132053a3e6ded7ac39998bba0e7542a06428588d79beb27`.

The regression requires the local extracted disc and preserved private card:

```powershell
. .\.local\environment.ps1
dotnet run --project tools/RecompOne/tests/ContinueRegression -c Release -- C:\Programming\GitHub\SpideyPS1recomp C:\Programming\GitHub\SpideyPS1recomp\.local\continue\tests
```
