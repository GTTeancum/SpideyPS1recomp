# Targeted Right-Hand Contact Corrections

Only three rotations changed: Arana Gymnast's RArmDigit32, and Batty Brant's
RArmDigit51/RArmDigit52. Exact named-bone checks and all 4196 animation frames
verify that left hands, thumbs, other fingers, body, source bind, geometry,
weights and all open poses remain unchanged. Both suits pass 115 web frames,
structural and managed tests. Ten finger-policy unit tests pass. The Windows
game build reports zero warnings and errors; Linux execution is not claimed.

Sampled right-hand palm-envelope clearances are positive: minima approximately
6.72 for Arana and 5.97 for Batty. This is not exhaustive collision testing.
Two native framebuffer images per suit were individually reviewed. Arana shows
a rear jump and front-facing movement, not a verified live web endpoint. Batty
shows a swing with attached web and a side-facing folded right glove. These
views complement rather than replace the offline contact diagnostics.

The initial isolation harness wrongly expected two changed rotations for both
suits. Binary inspection found one for Arana and two for Batty. The rerun
requires the exact named sets in isolation-expectations.json; the initial failed
run is retained. This is a narrower assertion, not permission for extra changes.

Safety branch: safety/pre-fists08-20260923. Previous suit directories and the
installed hash inventory are retained in the task's work/safety/fist-repair-batch08
backup. Only four actor/report files changed; 960 other installed files remain
identical, including names and texture bytes. No push was performed.

The two suits were already included in the 42-suit policy rollout count. This
correction does not increment that count. The remaining 191 rollout entries,
including Swiney Girl, are unfinished; final catalogue acceptance remains false.
