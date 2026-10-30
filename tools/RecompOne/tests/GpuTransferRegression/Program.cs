using RecompOne.Runtime;
using RecompOne.Runtime.Hle;
using Silk.NET.Maths;
using Silk.NET.OpenGL;
using Silk.NET.Windowing;

// Real OpenGL transfers, with no retail executable or game assets. Linux may run
// this under xvfb-run with LIBGL_ALWAYS_SOFTWARE=1. Missing GL support is a failure,
// never a silently skipped successful test.
internal static class Program
{
    private const int Width = Gpu.VramWidth;
    private const int Height = Gpu.VramHeight;
    private static int checks, failures;

    private readonly record struct Transfer(string Name, int X, int Y, int W, int H);

    private static int Main(string[] args)
    {
        string selection = args.Length == 0 ? "all" : args[0];
        if (args.Length > 1 || selection is not ("all" or "gl33" or "gl45"))
        {
            Console.Error.WriteLine("Usage: GpuTransferRegression [all|gl33|gl45]");
            return 2;
        }

        foreach (string backend in selection == "all" ? new[] { "gl33", "gl45" } : new[] { selection })
        {
            try { RunBackend(backend); }
            catch (Exception ex)
            {
                checks++;
                failures++;
                Console.Error.WriteLine($"FAIL {backend}: {ex}");
            }
        }
        Console.WriteLine($"GPU transfers: {checks - failures}/{checks} checks passed; {failures} failure(s).");
        return failures == 0 ? 0 : 1;
    }

    private static void RunBackend(string name)
    {
        var options = WindowOptions.Default with
        {
            Size = new Vector2D<int>(64, 64),
            IsVisible = false,
            VSync = false,
            API = new GraphicsAPI(ContextAPI.OpenGL, ContextProfile.Core, ContextFlags.Default,
                name == "gl33" ? new APIVersion(3, 3) : new APIVersion(4, 5)),
        };
        using var window = Window.Create(options);
        window.Initialize();
        window.GLContext!.MakeCurrent();
        using var gl = GL.GetApi(window);
        Console.WriteLine($"{name}: GL={gl.GetStringS(StringName.Version)}; " +
                          $"vendor={gl.GetStringS(StringName.Vendor)}; renderer={gl.GetStringS(StringName.Renderer)}");

        int previousScale = GlVram.Scale;
        bool previousActive = GpuHle.Active;
        var previousBackend = GpuHle.Backend;
        bool previousCoverage = GpuHle.WideBackgroundCompletion;
        float previousWide = GpuHle.WideAspect;
        try
        {
            GpuHle.WideBackgroundCompletion = false;
            GpuHle.WideAspect = 0;
            foreach (int scale in new[] { 1, 4 })
            {
                GlVram.Scale = scale;
                IGlVram vram = name == "gl33" ? new Gl33Vram(gl) : new Gl45Vram(gl);
                var renderer = new GlCore(gl, vram);
                try
                {
                    renderer.InitGl();
                    Check(renderer.Ready && DrainErrors(gl).Length == 0, $"{name}/{scale}x initialization");
                    if (!renderer.Ready) throw new InvalidOperationException("Renderer initialization failed.");
                    GpuHle.Backend = renderer;
                    RunTransfers(gl, renderer, $"{name}/{scale}x");
                    RunDisplaySynchronization(gl, renderer, $"{name}/{scale}x");
                }
                finally
                {
                    GpuHle.Active = false;
                    GpuHle.Backend = null;
                    renderer.Dispose();
                }
            }
        }
        finally
        {
            GlVram.Scale = previousScale;
            GpuHle.Active = previousActive;
            GpuHle.Backend = previousBackend;
            GpuHle.WideBackgroundCompletion = previousCoverage;
            GpuHle.WideAspect = previousWide;
        }
    }

    private static void RunTransfers(GL gl, GlCore renderer, string label)
    {
        Transfer[] transfers =
        [
            new("ordinary odd width/height", 17, 19, 3, 5),
            new("exact right/bottom boundary", 1020, 510, 4, 2),
            new("right wrap with row stride", 1022, 21, 5, 3),
            new("bottom wrap", 29, 510, 3, 4),
            new("right and bottom wrap", 1022, 511, 5, 3),
            new("full width at nonzero X", 17, 99, Width, 2),
            new("full height at nonzero Y", 19, 5, 3, Height),
            new("full VRAM at nonzero origin", 997, 503, Width, Height),
        ];
        var empty = new ushort[Width * Height];
        var seeded = Pattern(empty.Length, 317);
        var gpuPixels = new ushort[empty.Length];

        foreach (var transfer in transfers)
        {
            var pixels = Pattern(transfer.W * transfer.H, 43);
            var unchanged = pixels.ToArray();
            var expected = new Gpu();
            GpuHle.Active = false;
            Upload(expected, transfer, pixels);

            renderer.WriteVram(0, 0, Width, Height, empty);
            DrainErrors(gl);
            var actual = new Gpu();
            GpuHle.Active = true;
            Upload(actual, transfer, pixels);
            string uploadErrors = DrainErrors(gl);
            renderer.ReadVram(0, 0, Width, Height, gpuPixels);
            string uploadReadErrors = DrainErrors(gl);
            bool uploadPass = expected.Vram.AsSpan().SequenceEqual(gpuPixels) &&
                              expected.Vram.AsSpan().SequenceEqual(actual.Vram) &&
                              pixels.AsSpan().SequenceEqual(unchanged) &&
                              uploadErrors.Length == 0 && uploadReadErrors.Length == 0;
            Check(uploadPass, $"{label} upload: {transfer.Name}",
                $"GL={uploadErrors}/{uploadReadErrors}; {Mismatch(expected.Vram, gpuPixels)}");

            // Seed through an ordinary full-size transfer so a broken wrapped upload
            // cannot conceal a separate readback defect.
            renderer.WriteVram(0, 0, Width, Height, seeded);
            string seedErrors = DrainErrors(gl);
            seeded.CopyTo(expected.Vram, 0);
            GpuHle.Active = false;
            ushort[] wanted = Download(expected, transfer);
            GpuHle.Active = true;
            ushort[] readback = Download(actual, transfer);
            string readErrors = DrainErrors(gl);
            Check(wanted.AsSpan().SequenceEqual(readback) && seedErrors.Length == 0 && readErrors.Length == 0,
                $"{label} readback: {transfer.Name}",
                $"GL={seedErrors}/{readErrors}; {Mismatch(wanted, readback)}");
        }
    }

    private static void RunDisplaySynchronization(GL gl, GlCore renderer, string label)
    {
        var empty = new ushort[Width * Height];
        renderer.WriteVram(0, 0, Width, Height, empty);
        GpuHle.NotifyDisplay(0, 0, 32, 32);
        renderer.SetDrawEnv(new HleDrawEnv { ClipX0 = 0, ClipY0 = 0, ClipX1 = 31, ClipY1 = 31 });
        renderer.DrawRect(new HleRect { X = 0, Y = 0, W = 8, H = 8, R = 255 }, default);
        var wrap = new ushort[8];
        renderer.ReadVram(1022, 511, 4, 2, wrap);
        ushort[] wanted = [0, 0, 0, 0, 0, 0, 0x001F, 0x001F];
        string readErrors = DrainErrors(gl);
        Check(wrap.AsSpan().SequenceEqual(wanted) && readErrors.Length == 0,
            $"{label} wrapped read flushes and writes back the dirty display target",
            $"GL={readErrors}; {Mismatch(wanted, wrap)}");

        // Dirty the target again, then upload across both VRAM edges. The wrapped
        // origin pixels must also update the cached display target, while red pixels
        // outside the upload survive its eventual writeback.
        renderer.DrawRect(new HleRect { X = 0, Y = 0, W = 8, H = 8, R = 255 }, default);
        ushort[] update = [0x0123, 0x0456, 0x0789, 0x0ABC, 0x1234, 0x4567, 0x7C00, 0x03E0];
        renderer.WriteVram(1022, 511, 4, 2, update);
        string writeErrors = DrainErrors(gl);
        var shown = new ushort[64];
        renderer.ReadVram(0, 0, 8, 8, shown);
        var expected = Enumerable.Repeat((ushort)0x001F, 64).ToArray();
        expected[0] = update[6];
        expected[1] = update[7];
        string syncErrors = DrainErrors(gl);
        Check(shown.AsSpan().SequenceEqual(expected) && writeErrors.Length == 0 && syncErrors.Length == 0,
            $"{label} wrapped upload synchronizes the display target without losing neighboring pixels",
            $"GL={writeErrors}/{syncErrors}; {Mismatch(expected, shown)}");
    }

    private static ushort[] Pattern(int length, int seed)
    {
        var data = new ushort[length];
        for (int i = 0; i < length; i++)
            data[i] = unchecked((ushort)(seed + i * 193 + (i / 7) * 11));
        return data;
    }

    private static void Upload(Gpu gpu, Transfer t, ushort[] pixels)
    {
        gpu.WriteGp0(0xA0000000);
        gpu.WriteGp0((uint)t.X | (uint)t.Y << 16);
        gpu.WriteGp0((uint)t.W | (uint)t.H << 16);
        for (int i = 0; i < pixels.Length; i += 2)
            gpu.WriteGp0(pixels[i] | (uint)(i + 1 < pixels.Length ? pixels[i + 1] : 0xA55A) << 16);
    }

    private static ushort[] Download(Gpu gpu, Transfer t)
    {
        gpu.WriteGp0(0xC0000000);
        gpu.WriteGp0((uint)t.X | (uint)t.Y << 16);
        gpu.WriteGp0((uint)t.W | (uint)t.H << 16);
        var pixels = new ushort[t.W * t.H];
        for (int i = 0; i < pixels.Length; i += 2)
        {
            uint word = gpu.ReadData();
            pixels[i] = (ushort)word;
            if (i + 1 < pixels.Length) pixels[i + 1] = (ushort)(word >> 16);
        }
        return pixels;
    }

    private static string DrainErrors(GL gl)
    {
        var errors = new List<string>();
        for (var error = gl.GetError(); error != GLEnum.NoError; error = gl.GetError())
            errors.Add(error.ToString());
        return string.Join(",", errors);
    }

    private static string Mismatch(ReadOnlySpan<ushort> expected, ReadOnlySpan<ushort> actual)
    {
        for (int i = 0; i < Math.Min(expected.Length, actual.Length); i++)
            if (expected[i] != actual[i]) return $"first mismatch [{i}]: expected=0x{expected[i]:X4} actual=0x{actual[i]:X4}";
        return expected.Length == actual.Length ? "pixels match" : "length mismatch";
    }

    private static void Check(bool pass, string name, string details = "")
    {
        checks++;
        if (!pass) failures++;
        Console.WriteLine($"{(pass ? "PASS" : "FAIL")} {name}{(!pass && details.Length != 0 ? ": " + details : "")}");
    }
}
