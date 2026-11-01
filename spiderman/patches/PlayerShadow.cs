using System;
using System.Collections.Generic;
using System.Numerics;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Hle;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Issue #9: N64-style player shadow. The console version draws a round blob under
/// Spider-Man; the N64 version casts his silhouette onto the ground. This replaces the
/// player's blob with that silhouette.
///
/// Capture: the per-part mesh draw (func_80077C08 / func_80077CC4, mesh in A0, actor in
/// the caller's S2) runs with the GTE already holding that part's camera-space RT/TR, so
/// every player triangle is recorded in camera space exactly as the game projects it.
///
/// Draw: the blob (tpage 76, CLUT 33, subtractive) is drawn on the floor under the player.
/// Its screen centre fixes the floor height: the point straight below the player whose
/// projection lands there. The recorded triangles are flattened onto that floor along a
/// fixed world light and drawn in place of the blob, in the blob's ordering-table slot,
/// darkening each pixel once (PrimFlags.Shadow).
///
///     SPIDEY_SHADOW=off      keep the retail blob
///     SPIDEY_SHADOW=debug    draw the captured triangles unflattened (alignment check)
///     SPIDEY_SHADOW_LIGHT=x,z  world light slant (default 0.45,0.55; Y is down)
/// </summary>
public static class PlayerShadow
{
    const uint PlayerPointer = 0x800B5268;
    const uint CameraRotation = 0x800A57CC;
    const uint OutputBuffer = 0x800B58F0;
    const int BlobTexPage = 76, BlobClut = 33;

    static readonly string Mode = Environment.GetEnvironmentVariable("SPIDEY_SHADOW")?.Trim().ToLowerInvariant() ?? "on";
    static readonly Vector3 Light = ParseLight(Environment.GetEnvironmentVariable("SPIDEY_SHADOW_LIGHT"));
    static readonly List<Vector3> Triangles = new();
    static readonly List<Vector3> _current = new();
    static readonly Dictionary<uint, Vector3> _byAddress = new();
    static long _frame = -1;
    static float _h, _ofx, _ofy;
    static readonly short[] Camera = new short[9];

    static Vector3 ParseLight(string spec)
    {
        float x = 0.45f, z = 0.55f;
        var p = spec?.Split(',');
        if (p?.Length == 2 && float.TryParse(p[0], out float px) && float.TryParse(p[1], out float pz)) { x = px; z = pz; }
        return Vector3.Normalize(new Vector3(x, 1f, z));
    }

    public static void Install()
    {
        if (Mode == "off") return;
        Event.AddListener<RenderPrimEvent>(OnPrim);
        Console.WriteLine($"[shadow] player silhouette shadow {Mode}, light {Light.X:F2},{Light.Y:F2},{Light.Z:F2}");
    }

    public static void MeshDraw(CpuContext c, IMemory m)
    {
        if (Mode == "off" || c.S2 == 0 || c.S2 != m.ReadU32(PlayerPointer)) return;
        if (Diag.Frame != _frame)
        {
            Triangles.Clear();
            _byAddress.Clear();
            _frame = Diag.Frame;
            _h = (short)Gte.ReadControl(26);
            _ofx = (int)Gte.ReadControl(24) / 65536f;
            _ofy = (int)Gte.ReadControl(25) / 65536f;
            for (uint i = 0; i < 9; i++) Camera[i] = unchecked((short)m.ReadU16(CameraRotation + i * 2));
        }

        uint mesh = c.A0;
        if (mesh < 0x80000000u || mesh >= 0x807FF000u) return;
        int nv = m.ReadU16(mesh + 2), nn = m.ReadU16(mesh + 4), nf = m.ReadU16(mesh + 6);
        if (nv == 0 || nv > 512 || nf > 1024 || nn != nv + nf) return;

        Span<float> rt = stackalloc float[9];
        for (int i = 0; i < 4; i++)
        {
            uint w = Gte.ReadControl(i);
            rt[i * 2] = unchecked((short)w) / 4096f;
            rt[i * 2 + 1] = unchecked((short)(w >> 16)) / 4096f;
        }
        rt[8] = unchecked((short)Gte.ReadControl(4)) / 4096f;
        var tr = new Vector3((int)Gte.ReadControl(5), (int)Gte.ReadControl(6), (int)Gte.ReadControl(7));

        // Vertices flagged 2 in their pad word are joint vertices, already transformed
        // for an earlier part. The transform (func_8007B798/B9CC) reads them from
        // (output buffer + 0x1F38 - x), where it stores 8 bytes per vertex, so record
        // every vertex by the address the game writes it to and resolve joints the same way.
        uint output = m.ReadU32(OutputBuffer);
        _current.Clear();
        for (int i = 0; i < nv; i++)
        {
            uint v = mesh + 28 + (uint)i * 8;
            short x = unchecked((short)m.ReadU16(v)), y = unchecked((short)m.ReadU16(v + 2)), z = unchecked((short)m.ReadU16(v + 4));
            Vector3 p;
            if ((m.ReadU16(v + 6) & 2) != 0)
                p = _byAddress.TryGetValue(output + 0x1F38u - (uint)(ushort)x, out var joint) ? joint : new Vector3(float.NaN);
            else
                p = tr + new Vector3(rt[0] * x + rt[1] * y + rt[2] * z, rt[3] * x + rt[4] * y + rt[5] * z, rt[6] * x + rt[7] * y + rt[8] * z);
            _current.Add(p);
            _byAddress[output + (uint)i * 8] = p;
        }
        // Faces are variable length (u16 size at +2); vertex indices at +4..+7, the
        // fourth repeating the third for a triangle.
        uint face = mesh + 28 + (uint)(nv + nn) * 8;
        Span<int> idx = stackalloc int[4];
        for (int f = 0; f < nf; f++)
        {
            uint size = m.ReadU16(face + 2);
            if (size < 12 || size > 64) return;
            for (int k = 0; k < 4; k++) idx[k] = m.ReadU8(face + 4 + (uint)k);
            if (idx[0] < nv && idx[1] < nv && idx[2] < nv)
            {
                AddTriangle(idx[0], idx[1], idx[2]);
                if (idx[3] < nv && idx[3] != idx[2]) AddTriangle(idx[1], idx[3], idx[2]);
            }
            face += size;
        }
    }

    static void AddTriangle(int a, int b, int c)
    {
        Vector3 pa = _current[a], pb = _current[b], pc = _current[c];
        if (float.IsNaN(pa.X) || float.IsNaN(pb.X) || float.IsNaN(pc.X)) return;
        Triangles.Add(pa); Triangles.Add(pb); Triangles.Add(pc);
    }

    static void OnPrim(RenderPrimEvent e)
    {
        if (e.Skip || e.Count != 4 || !e.Textured || !e.SemiTransparent ||
            e.TexPage != BlobTexPage || e.Clut != BlobClut || Triangles.Count == 0) return;
        if (GpuHle.Backend is not { Ready: true } backend) return;

        // Where the player is on screen, to tell his blob from enemies' and pickups'.
        Vector3 centre = Vector3.Zero;
        float minX = float.MaxValue, maxX = float.MinValue, maxY = float.MinValue;
        foreach (var p in Triangles)
        {
            centre += p;
            if (p.Z <= 1) continue;
            var s = Project(p);
            minX = MathF.Min(minX, s.X); maxX = MathF.Max(maxX, s.X); maxY = MathF.Max(maxY, s.Y);
        }
        centre /= Triangles.Count;
        float bx = 0, by = 0;
        for (int i = 0; i < 4; i++) { bx += e.X[i] - e.DrawOffsetX; by += e.Y[i] - e.DrawOffsetY; }
        bx /= 4; by /= 4;
        float width = maxX - minX;
        if (bx < minX - width || bx > maxX + width || by < maxY - 2 * width - 40) return;

        Vector3 down = Vector3.Normalize(new Vector3(Camera[1], Camera[4], Camera[7]));
        Vector3 light = Vector3.Normalize(new Vector3(
            Camera[0] * Light.X + Camera[1] * Light.Y + Camera[2] * Light.Z,
            Camera[3] * Light.X + Camera[4] * Light.Y + Camera[5] * Light.Z,
            Camera[6] * Light.X + Camera[7] * Light.Y + Camera[8] * Light.Z));

        // Floor: the point centre + s*down that projects onto the blob's centre.
        float k = by - _ofy;
        float den = k * down.Z - _h * down.Y;
        if (MathF.Abs(den) < 1e-3f) return;
        float s0 = (_h * centre.Y - k * centre.Z) / den;
        Vector3 floor = centre + s0 * down;
        // The floor point is solved from the blob's height on screen; it must also land
        // on the blob horizontally, or the blob belongs to something else (pickups).
        if (s0 <= 0 || floor.Z <= 16 || MathF.Abs(Project(floor).X - bx) > 0.25f * width + 6) return;
        float along = Vector3.Dot(light, down);
        if (along < 0.2f) return;

        // Debug draws opaque magenta under the player: only misalignment shows.
        bool debug = Mode == "debug";
        var flags = new PrimFlags { Shadow = !debug, SemiTrans = !debug, TPage = 0 };
        byte tint = debug ? (byte)255 : (byte)0;
        Span<HleVertex> tri = stackalloc HleVertex[3];
        for (int t = 0; t + 2 < Triangles.Count; t += 3)
        {
            bool ok = true;
            for (int j = 0; j < 3; j++)
            {
                Vector3 p = Triangles[t + j];
                if (!debug) p += Vector3.Dot(floor - p, down) / along * light;
                if (p.Z <= 16) { ok = false; break; }
                var s = Project(p);
                tri[j] = new HleVertex { X = s.X + e.DrawOffsetX, Y = s.Y + e.DrawOffsetY, Z = 1, R = tint, B = tint };
            }
            if (ok) backend.DrawTri(tri[0], tri[1], tri[2], flags);
        }
        e.Skip = true;
    }

    static Vector2 Project(Vector3 p) => new(
        _h * p.X / p.Z * GpuHle.FovNum / GpuHle.FovDen + _ofx,
        _h * p.Y / p.Z + _ofy);
}
