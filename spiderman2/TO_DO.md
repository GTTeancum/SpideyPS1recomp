# Spider-Man 2 port — open work

Shared performance work and GitHub priorities are tracked in the [main to-do list](../TODO.md#github-issues) and [performance assessment](../docs/performance-assessment-2026-09-17.md).

- [ ] **Full level completion.** Verify normal player traversal and completion; prior scripted rooftop/death coverage does not establish this. Assess recording/replay support for repeatable routes.
- [ ] **Memory cards.** Verify saving, loading, and persistence in SM2; shared runtime fixes alone do not establish game-specific behavior.
- [ ] **Four story-prefix failures.** Investigate the previously recorded model-initialization failures in `e3m1`, `e3m2`, `e4m1`, and `e5m2`, then include those levels in the visual audit.
- [ ] **Widescreen and geometry.** Review side bands and unresolved rooftop polygons throughout the story levels. Current default background-completion behavior and assessment limits are documented in the performance handoff.
- [ ] **Game-internals instrumentation.** Add verified game-specific addresses/hooks where needed for the performance and functional test plan; do not infer SM2 layouts from SM1.
