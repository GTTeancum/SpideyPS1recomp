# Right-stick orbit — November 16, 2026

Horizontal right-stick input rotates the normal follow camera around Spider-Man
and rotates the costume viewer. Both games use a proportional dead zone and a
maximum gameplay speed of 180 degrees per second, with the costume viewer at
90 degrees per second (updated November 20). Timing follows console vblanks, so
interpolated presentations do not advance the orbit. Releasing the stick hands
heading control back to the game; there is no added automatic recenter.

Gameplay changes the native desired yaw before its offset and camera geometry
are calculated. The native collision and movement-relative camera code still
runs. Only active mode-3 cameras following the player accept input. Player
scripts, other targets, paused and special cameras are excluded. While held,
the accumulated manual yaw takes priority over native auto-follow requests.
The viewer gives the stick priority over native rotation while held, retaining
existing controls at neutral and keeping the zoom radius.

The hooks are registered in each game's configuration and applied to the local
generated sources (which are ignored by Git):

| Context | Spider-Man | Spider-Man 2 |
| --- | --- | --- |
| Follow-camera offset | `80071864` | `8007D9B4` |
| Viewer rotation branch | `80262568` → `802625B4` | `80249B90` → `80249BDC` |

The shared input poll now clears player-one axes when its controller disconnects,
matching the existing player-two behavior.

## Validation

Run from the repository after loading `.local/environment.ps1`:

```powershell
dotnet run --project tools/RecompOne/tests/OrbitRegression -c Release -- .
dotnet run --project tools/RecompOne/tests/Sm2OrbitRegression -c Release -- .
```

The optional repository argument loads the private extracted retail EXE's tables
and executes the original follow-camera geometry routine in memory. Tests cover
dead zone, proportional movement, both directions, full-circle wrapping,
duplicate presentation, pause/resume, ownership changes, native auto-follow,
camera/script guards, viewer branch/zoom behavior and process-local input.
Native geometry checks verify four opposing positions at a fixed 300-unit radius
around an offset target and restoration of the CPU stack.

Logs: `.local/orbit/sm1-tests.log` and `.local/orbit/sm2-tests.log`.
Both Release builds completed and all 60 checks passed for each game.
These tests initialize no game window, audio device or operating-system input.
They do not establish visual gameplay correctness, wall collision behavior or
controller feel by themselves. See the later user acceptance below.

## SM1 deployment — November 16, 2026

At the user's request after the successful Venom chase playthrough, the updated
portable Windows x64 `Spider-Man/SpiderMan.exe` was published and deployed at
21:27 Eastern. Its SHA-256 is
`a73bd43d49943b570a0dfcadcfb93f85ce4e30d98c8868bc21af73998ae2df7d`.
All 1,807 other staged files matched their pre-deployment hashes, including
settings, saves and suits. No game was launched. The SM2 staged EXE is unchanged.

Deployment evidence is `.local/orbit/deploy-sm1.json`; the prior EXE is retained
at `.local/orbit/SpiderMan.before-orbit.exe`. Automatic approval review rejected
the combined deployment-and-cleanup command without a detailed reason. Separate
EXE replacement succeeded; temporary publish files remain in
`.local/orbit/publish-sm1/`.

## User acceptance — November 16, 2026

After testing the staged SM1 EXE, the user reported: "Right stick rotation works
perfect." SM1 rotation is accepted and removed from TODO. This feedback does not
establish a separate camera/wall-collision test. SM2 deployment and visual testing
remained outstanding at that time. See the November 20 update below for current status.

## Speed update — November 20, 2026

Gameplay rotation is twice as fast at full deflection; viewer speed is unchanged.
Both games pass 63 orbit checks, including speed independence at 20, 30 and 60
updates per second. Native visual checks and staged deployment are documented in
[menu, orbit and Big Time repair evidence](menu-orbit-bigtime.md).
Both staged EXEs were updated on November 21 after hidden, silent native tests.
SM2 gameplay and costume-viewer rotation are now visually verified; its completed
deployment item has been removed from TODO.

## Bank rendering report

The bank NPC occlusion and shadow reports remain open and are tracked separately
in [bank rendering notes](bank-rendering.md). The initial report was in `l2a1`;
the later screenshots and saved log identify `l1a4_t` with the Surveillance suit.
