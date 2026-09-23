# Decoder provenance

This is an **adaptation**, not an unmodified/pinned upstream PL_MPEG release.
MPEG-1 VLC trees, picture/reference organization and reconstruction were adapted
from Dominic Szablewski's MIT-licensed PL_MPEG, consulted through its raw source:

- https://raw.githubusercontent.com/phoboslab/pl_mpeg/master/pl_mpeg.h
- https://github.com/phoboslab/pl_mpeg

The upstream commit could not be retrieved. Do not invent a commit or claim that
our adapted source hash is an upstream hash. `build-provenance.json` identifies
our exact shipping source and compiled libraries. Preserve LICENSE-PL_MPEG.txt.

New code supplies the bounded MPEG-PS reader, single-handle callbacks, ADX decode,
C ABI, validation/error paths, fixed workspace, rational frame metadata, original
ADX sample count, full-picture coverage, edge-safe prediction, final reference
flush, magnitude-first inverse quantization and Q14/64-bit separable inverse DCT.
PL_MPEG's MP2 audio decoder, malloc-based I/O and approximate premultiplied IDCT
are NOT used. No FFmpeg code or binary is linked/shipped in this decoder.

ADX header/predictor details were cross-checked against the supplied streams and
these independently licensed **reference** implementations (not copied modules):
- https://raw.githubusercontent.com/FFmpeg/FFmpeg/n7.1/libavcodec/adx.c
- https://raw.githubusercontent.com/FFmpeg/FFmpeg/n7.1/libavcodec/adxdec.c
- https://raw.githubusercontent.com/FFmpeg/FFmpeg/n7.1/libavformat/mpeg.c

Test-only FFmpeg 7.1.5 supplies independently decoded frame/sample comparisons.
It is NOT required at installation or playback. `tiny.sfd` is a synthetic test
pattern plus authored silent ADX, not extracted game content; its generator is
included. Original Dreamcast SFDs are never rewritten or embedded in the overlay.

## Reference-decoder caveat

The supplied L2M2 frame 2 has full-pel B-picture motion and skipped macroblocks.
FFmpeg's n7.1 `mpeg12dec.c` expands full-pel vectors for coded macroblocks, but the
skip branch copies unexpanded `last_mv` values. Our prediction retains full-pel
scaling for both. This produces a localized difference against that reference;
we do not silently implement a reference quirk just to report identical pixels.
See the repair package's `evidence/reference-motion-difference.json` and the direct native unit test. Actual
Dreamcast presentation has not been captured for an independent tie-breaker.

https://raw.githubusercontent.com/FFmpeg/FFmpeg/n7.1/libavcodec/mpeg12dec.c
(coded motion around lines 538–552; skipped motion around 1526–1530).
