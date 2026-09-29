using System.Text.Json;
using RecompOne.Runtime.Events;

namespace RecompOne.Runtime.Hle;

/// <summary>Opt-in submitted geometry evidence for native frame captures.</summary>
public static class GeometryTrace
{
    static readonly string? Path = Environment.GetEnvironmentVariable("RECOMP_GEOMETRY_DUMP");
    static readonly long Start = long.TryParse(Environment.GetEnvironmentVariable("RECOMP_GEOMETRY_START"), out var start) ? start : 0;
    static readonly long End = long.TryParse(Environment.GetEnvironmentVariable("RECOMP_GEOMETRY_END"), out var end) ? end : Start;
    static long _frame;
    static readonly bool HudOnly = Environment.GetEnvironmentVariable("RECOMP_GEOMETRY_HUD_ONLY") == "1";
    static StreamWriter? _writer;
    static StreamWriter? _textureWriter;
    static readonly HashSet<string> TextureSignatures = [];
    static long _stackFrame = -1;

    static GeometryTrace()
    {
        if (string.IsNullOrWhiteSpace(Path)) return;
        Event.AddListener<VSyncEvent>(e =>
        {
            _frame = e.Frame;
            if (_frame > End)
            {
                _writer?.Dispose(); _writer = null;
                _textureWriter?.Dispose(); _textureWriter = null;
            }
        });
    }

    public static void TextureResolution(PrimFlags flags, int u0, int v0, int u1, int v1,
        bool hit, Assets.Textures.ResolvedTexture resolved)
    {
        if (string.IsNullOrWhiteSpace(Path) || _frame < Start || _frame > End ||
            TextureSignatures.Count >= 256) return;
        string key = $"{flags.TPage}:{flags.Clut}:{u0}:{v0}:{u1}:{v1}:{hit}";
        if (!TextureSignatures.Add(key)) return;
        var rect = Assets.Textures.TextureTile.Describe(flags.TPage, flags.Clut,
            u0, v0, u1 - u0 + 1, v1 - v0 + 1);
        var palette = new ushort[rect.ClutCount];
        if (Runtime.Gpu is { } gpu)
            for (int i = 0; i < palette.Length; i++)
                palette[i] = gpu.Vram[rect.ClutY * 1024 + ((rect.ClutX + i) & 1023)];
        _textureWriter ??= new StreamWriter(Path + ".textures.jsonl") { AutoFlush = true };
        _textureWriter.WriteLine(JsonSerializer.Serialize(new {
            frame = _frame, flags.TPage, flags.Clut, u0, v0, u1, v1, hit,
            flags.BlendMode, flags.SemiTrans, flags.World, palette,
            replacement = resolved.Texture == null ? null : new {
                resolved.Texture.Width, resolved.Texture.Height, mode = resolved.Texture.Mode.ToString(),
                resolved.Rect.U0, resolved.Rect.V0, resolved.Rect.W, resolved.Rect.H,
            },
            replacementClut = resolved.Clut != null,
        }));
    }

    public static void Triangle(HleVertex a, HleVertex b, HleVertex c, HleDrawEnv env, PrimFlags flags,
        (int X, int Y) nativeA, (int X, int Y) nativeB, (int X, int Y) nativeC)
    {
        if (string.IsNullOrWhiteSpace(Path) || _frame < Start || _frame > End) return;
        if (HudOnly && (flags.World || !flags.Textured)) return;
        _writer ??= new StreamWriter(Path) { AutoFlush = true };
        if (_stackFrame != _frame)
        {
            _stackFrame = _frame;
            File.AppendAllText(Path + ".stacks.txt", $"frame={_frame}\n{Environment.StackTrace}\n");
        }
        object Vertex(HleVertex v, (int X, int Y) native) => new {
            v.X, v.Y, v.Z, v.U, v.V, v.R, v.G, v.B, v.HasGteZ, NativeX = native.X, NativeY = native.Y };
        _writer.WriteLine(JsonSerializer.Serialize(new
        {
            frame = _frame, vertices = new[] { Vertex(a, nativeA), Vertex(b, nativeB), Vertex(c, nativeC) },
            env.ClipX0, env.ClipY0, env.ClipX1, env.ClipY1,
            env.TwMaskX, env.TwMaskY, env.TwOffX, env.TwOffY, env.SetMask, env.CheckMask,
            flags.World, flags.Textured, flags.TPage, flags.Clut, flags.SemiTrans,
            flags.BlendMode, flags.RawTexture, flags.Gouraud, flags.Hud,
            flags.Background, flags.IgnoreCoverage,
        }));
    }

    public static void Rectangle(HleRect rect, HleDrawEnv env, PrimFlags flags)
    {
        if (string.IsNullOrWhiteSpace(Path) || _frame < Start || _frame > End) return;
        HleVertex Corner(int x, int y) => new() {
            X = rect.X + x, Y = rect.Y + y,
            U = (short)(rect.U + x), V = (short)(rect.V + y), R = rect.R, G = rect.G, B = rect.B,
        };
        var a = Corner(0, 0); var b = Corner(rect.W, 0);
        var c = Corner(0, rect.H); var d = Corner(rect.W, rect.H);
        // Match the backend's rectangle triangulation without changing submission.
        Triangle(a, b, c, env, flags, ((int)a.X, (int)a.Y), ((int)b.X, (int)b.Y), ((int)c.X, (int)c.Y));
        Triangle(b, d, c, env, flags, ((int)b.X, (int)b.Y), ((int)d.X, (int)d.Y), ((int)c.X, (int)c.Y));
    }
}
