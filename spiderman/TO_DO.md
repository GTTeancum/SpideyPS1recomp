# Spider-Man port — open work

Current progress and verification evidence live in [the project checkpoint](../CHECKPOINT.md).

- Retest the reported `L5A3` sewer crash with the animation-part repair, then
  verify progression through the level. The old converted Lizard-man layout
  overruns animation scratch into collision data; isolated released-code
  reproduction is distinct from a successful full level run.
- Reproduce the reported costume-pack texture corruption with the failing pack
  and selection sequence. The custom texture-layout parser defect is repaired,
  but issue #8 has not been attributed to that defect.
- Complete campaign, transition, boss, death/retry, and save/load verification
  using private retail inputs. Historical targeted runs are not full coverage.
