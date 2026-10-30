using RecompOne.Runtime;
using RecompOne.Runtime.Assets.Textures;
using RecompOne.Runtime.Hle;

// Drive the actual GP0 fill, VRAM copy and CPU image-load commands through Gpu with
// no renderer attached, then ask VramTracker what TextureResolver would ask it.
// VRAM wraps at 1024x512; every block a command wrote must get a new generation and
// lose any GPU-dirty mark, including the part that wrapped to the left or top edge.
internal static class Program
{
    private static int checks, failures;

    private readonly record struct Rect(int X, int Y, int W, int H);

    // Probe: a rectangle the command wrote after wrapping (or, for the controls, the
    // ordinary destination). Origin: the unwrapped corner of the destination.
    private sealed record Case(string Name, Action<Gpu> Run, Rect Probe, Rect Origin);

    private static int Main()
    {
        if (GpuHle.Active) throw new InvalidOperationException("HLE must be inactive for CPU-only checks.");

        Case[] cases =
        [
            new("fill in bounds (control)", g => Fill(g, 0x0000FF, 64, 64, 32, 16),
                new(80, 64, 16, 16), new(64, 64, 16, 16)),
            new("fill right wrap", g => Fill(g, 0x0000FF, 1008, 64, 32, 16),
                new(0, 64, 16, 16), new(1008, 64, 16, 16)),
            new("fill bottom wrap", g => Fill(g, 0x00FF00, 64, 504, 16, 16),
                new(64, 0, 16, 8), new(64, 504, 16, 8)),
            new("fill right and bottom wrap", g => Fill(g, 0xFF0000, 1008, 504, 32, 16),
                new(0, 0, 16, 8), new(1008, 504, 16, 8)),
            new("copy in bounds (control)", g => Copy(g, 100, 100, 600, 200, 16, 8),
                new(608, 200, 8, 8), new(600, 200, 8, 8)),
            new("copy right wrap", g => Copy(g, 100, 100, 1016, 200, 16, 8),
                new(0, 200, 8, 8), new(1016, 200, 8, 8)),
            new("copy bottom wrap", g => Copy(g, 100, 100, 300, 508, 8, 8),
                new(300, 0, 8, 4), new(300, 508, 8, 4)),
            new("upload in bounds (control)", g => Upload(g, 700, 40, 8, 2),
                new(704, 40, 4, 2), new(700, 40, 4, 2)),
            new("upload right wrap", g => Upload(g, 1020, 40, 8, 2),
                new(0, 40, 4, 2), new(1020, 40, 4, 2)),
            new("upload right and bottom wrap", g => Upload(g, 1020, 510, 8, 4),
                new(0, 0, 4, 2), new(1020, 510, 4, 2)),
        ];

        Rect far = new(512, 320, 16, 16);
        foreach (var c in cases)
        {
            VramTracker.Reset();
            var gpu = new Gpu();
            // Copy source pixels are seeded directly so the tracker sees only the copy.
            for (int y = 96; y < 112; y++)
                for (int x = 96; x < 128; x++)
                    gpu.Vram[y * Gpu.VramWidth + x] = (ushort)(0x1000 + x + y);

            // The renderer drew over both the wrapped and the unwrapped area first.
            VramTracker.MarkGpuWrite(c.Probe.X, c.Probe.Y, c.Probe.W, c.Probe.H);
            VramTracker.MarkGpuWrite(c.Origin.X, c.Origin.Y, c.Origin.W, c.Origin.H);
            int probeBefore = Gen(c.Probe), originBefore = Gen(c.Origin), farBefore = Gen(far);
            ushort pixelBefore = Pixel(gpu, c.Probe);

            c.Run(gpu);

            Check(Pixel(gpu, c.Probe) != pixelBefore, $"{c.Name}: command changed the probed VRAM pixels");
            Check(Gen(c.Origin) != originBefore, $"{c.Name}: unwrapped destination gets a new generation");
            Check(!Dirty(c.Origin), $"{c.Name}: unwrapped destination is no longer GPU-dirty");
            Check(Gen(c.Probe) != probeBefore, $"{c.Name}: probed destination gets a new generation");
            Check(!Dirty(c.Probe), $"{c.Name}: probed destination is no longer GPU-dirty");
            Check(Gen(far) == farBefore, $"{c.Name}: unrelated block keeps its generation");
        }

        // A fill ending exactly at the right/bottom edge must not reach the left/top.
        VramTracker.Reset();
        var edge = new Gpu();
        Rect left = new(0, 496, 16, 16), top = new(992, 0, 16, 16);
        int leftBefore = Gen(left), topBefore = Gen(top);
        Fill(edge, 0x00FFFF, 992, 496, 32, 16);
        Check(Gen(new(992, 496, 32, 16)) != 0, "exact edge fill: destination gets a new generation");
        Check(Gen(left) == leftBefore && Gen(top) == topBefore, "exact edge fill: nothing wraps");

        // A full-width copy from a nonzero origin covers every column exactly once.
        VramTracker.Reset();
        var full = new Gpu();
        VramTracker.MarkGpuWrite(0, 128, 1024, 8);
        Copy(full, 0, 300, 17, 128, 0, 8); // width 0 means 1024
        Check(!Dirty(new(0, 128, 1024, 8)), "full-width copy at nonzero X: every column is clean");
        Check(!Dirty(new(1008, 128, 16, 8)) && !Dirty(new(0, 128, 16, 8)), "full-width copy at nonzero X: both edge blocks are clean");

        Console.WriteLine($"VRAM tracker:{checks - failures}/{checks} checks passed; {failures} failure(s).");
        return failures == 0 ? 0 : 1;
    }

    private static void Fill(Gpu gpu, uint bgr, int x, int y, int w, int h)
    {
        gpu.WriteGp0(0x02000000u | bgr);
        gpu.WriteGp0((uint)(y << 16 | x));
        gpu.WriteGp0((uint)(h << 16 | w));
    }

    private static void Copy(Gpu gpu, int sx, int sy, int dx, int dy, int w, int h)
    {
        gpu.WriteGp0(0x80000000u);
        gpu.WriteGp0((uint)(sy << 16 | sx));
        gpu.WriteGp0((uint)(dy << 16 | dx));
        gpu.WriteGp0((uint)(h << 16 | w));
    }

    private static void Upload(Gpu gpu, int x, int y, int w, int h)
    {
        gpu.WriteGp0(0xA0000000u);
        gpu.WriteGp0((uint)(y << 16 | x));
        gpu.WriteGp0((uint)(h << 16 | w));
        for (int i = 0; i < w * h / 2; i++) gpu.WriteGp0(0x7C1F7C1Fu);
    }

    private static int Gen(Rect r) => VramTracker.Generation(r.X, r.Y, r.W, r.H);
    private static bool Dirty(Rect r) => VramTracker.IsGpuDirty(r.X, r.Y, r.W, r.H);
    private static ushort Pixel(Gpu gpu, Rect r) => gpu.Vram[r.Y * Gpu.VramWidth + r.X];

    private static void Check(bool condition, string message)
    {
        checks++;
        if (condition) Console.WriteLine($"PASS {message}");
        else
        {
            failures++;
            Console.WriteLine($"FAIL {message}");
        }
    }
}
