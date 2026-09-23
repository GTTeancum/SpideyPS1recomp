# Laptop test build

These are diagnostic candidates, not a declaration that P0/P1 are resolved.
Use the Intel/Iris Xe laptop and the Ryzen laptop, and discrete graphics where
available. Strong machines matter too. CPU, RAM, selected renderer, settings,
power status and executable hashes are recorded automatically.

The user's Gateway GWTN156 bundle is
`proof_render/OpenSpidey-Gateway-Chase-Diagnostics-2026-09-18.zip`. It contains both games and the
user's local game data, including movies/audio. This replaces the earlier
executable-only kit for this personal laptop test.

This package includes the audio queue recovery, movie-buffer ownership,
SM1 movie CD binding, publish-time precompilation and timing/logging fixes.
It also includes the shared visible-edge subdivision precision repair and
read-only process-priority metadata in both games' automatic performance logs. Only
both EXEs changed from the previous verified full package; launchers/data/assets
are byte-identical. Each game's rendering regression has49passing checks;
SM1 seam removal and SM2 rooftop output were inspected in native candidate runs.
Launchers clear inherited diagnostic input and capture settings before starting.
The archive contains 2,074 manifest-verified files (1,090,938,879 bytes).
See the latest repair handoff for isolated launch verification and its limits.
Long-stall resolution and full Chase completion remain open; the checklist below
is for later acceptance, not a request for immediate laptop testing.

Known logging limit in this ZIP: `audio-state.jsonl` stops after180seconds
from its first observation. Frame/process logs continue, but missing later audio
records are not evidence of uninterrupted sound. For later interruptions, retain
your time/action notes; sustained audio telemetry needs a bounded rotation fix
before it can support the full acceptance checklist. The ZIP itself is unchanged.

1. Extract the entire ZIP into a new writable folder on the laptop. Start on AC
   power using normal settings.
2. Open `1 - Play Chase Venom.cmd`. Startup advances into Chase Venom automatically;
   wait for the scene, then play with normal controls. There is no follower or
   forced trigger activation. Use `2 - Play Spider-Man 2.cmd` for SM2 or
   `3 - Play Spider-Man normally.cmd` for ordinary SM1 startup. No disc selection
   is needed. These fresh test copies have memory-card saves disabled.
3. In each game, play at least 15 minutes with movement and combat. Include a
   movie, skip/replay, a level transition, and any costume you normally use.
   In SM1, play Chase Venom normally through the building sequence and finish it.
4. Note the approximate elapsed time, level/action, and what happened whenever
   animation, input, geometry, dialogue or music misbehaves. An FPS counter alone
   does not establish correct game speed or smoothness. Note whether the whole
   laptop also appears to hesitate.
5. Exit normally. Preserve performance-system.json, performance-frames-*.jsonl,
   performance-process-*.jsonl, audio-state.jsonl and spidey.log from beside each executable, together
   with your notes. Copy these logs before starting another run: the next launch
   replaces the preceding session's files. Logs stay local; nothing is uploaded.

If an issue reproduces, repeat the same scene with one setting changed at a time.
Useful comparisons include lower render scale, another GPU when available, and
AC versus battery if power mode appears relevant. Record the change. Do not
replace a failing result with a passing one; retain both sets of logs.

The first kit is intended to establish whether the currently observed stalls and
Chase problems reproduce on independent hardware. Report what actually completes
and what fails; do not mark untested scenarios as passed.
