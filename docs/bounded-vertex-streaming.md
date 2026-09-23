# Bounded modern-renderer vertex uploads

Implemented September 17, 2026 in the shared runtime used by both games.
`GlVertexStream` allocates one 36 MiB buffer: three regions of 262,144 vertices
at 48 bytes per vertex. Submissions append within a region. This bounds the
application's vertex storage, not driver-internal allocations or all memory.

Both modern backends use GL 3.3-compatible range mapping with write,
invalidate-range, and unsynchronized flags. This implementation does not use
persistent mapping. A fence follows all draws using a completed region. Before
that region is reused, its fence must signal. Uploads never overwrite a range
still owned by outstanding draws. A failed fence wait or five-second timeout
raises an explicit error. Main, transparency, coverage, and world-copy passes
all use the upload's first-vertex offset. The legacy reference backend retains
its previous upload path.

Run `dotnet run --project tools/RecompOne/tests/PerformanceRegression -c Release -- --gl`.
The hidden, process-local GL test wraps tiny regions many times, renders 96
colored points into its own framebuffer, and checks every pixel's expected RGB
values. It passed on the development machine. It sends no desktop input.

The earlier version of this document described persistent mapping and soak
results that did not match the source inspected on September 17. Those claims
are not acceptance evidence for this implementation. Current implementation,
measurements, and remaining coverage are tracked in
[performance repairs](performance-repairs-2026-09-17.md).
