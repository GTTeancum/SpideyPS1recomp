using System;
using System.Numerics;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Hle;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>Carry the GTE projection through SM1's CPU frustum clipper and its four-step grids.</summary>
public static class CpuWorldProjection
{
    // Homogeneous screen offsets ((screenX - 256) * Z, (screenY - 120) * Z, Z)
    // interpolate linearly in camera space, including on edges crossing the near plane.
    readonly record struct Vertex(Vector3 Position, uint Id);

    // The game alternates two 16-entry polygon buffers for each face.
    static Vertex[] input = new Vertex[16], output = new Vertex[16];
    static int intersectionWeight;
    static Vector3 intersection;
    static float focal;

    public static void SourceVertex(CpuContext c, IMemory m)
    {
        // At 8007810C A0 names the original projected vertex, and A1 its camera XY.
        // Use the native camera coordinates for a vertex behind the eye where there
        // is no usable GTE projection. The native near plane will intersect its edge.
        if (c.T0 == 0) focal = m.ReadU16(m.ReadU32(c.GP + 0x1124) + 14);
        Vector3 position;
        uint id = 0;
        if (m is PSMemory ps && ps.TryGetGteVertex(c.A0, m.ReadU32(c.A0), out var tag) &&
            tag.HasSubpixel && tag.Depth > 0)
        {
            position = new((tag.ScreenX - 256) * tag.Depth, (tag.ScreenY - 120) * tag.Depth, tag.Depth);
            if (GteIds.Enabled) id = GteIds.Find(tag.ScreenX, tag.ScreenY, tag.Depth);
        }
        else
        {
            position = new((short)m.ReadU16(c.A1) * focal * GpuHle.FovNum / GpuHle.FovDen,
                (short)m.ReadU16(c.A1 + 2) * focal, (short)m.ReadU16(c.A0 + 4));
        }
        if (GteIds.Enabled && id == 0)
            id = GteIds.Combine(m.ReadU32(c.GP + 0x1184), c.T0, 0, 0, 0, 0);
        input[c.T0] = new(position, id);
    }

    // The retail clipper assumes a focal length of 276 and a 4:3 view.
    // Test the actual projected frustum before the retail trivial-reject branch.
    static float Distance(Vector3 p, uint plane, float near) => plane switch
    {
        0 => p.Z - near,
        1 => 256 * p.Z - p.X,
        2 => 256 * p.Z + p.X,
        3 => 120 * p.Z - p.Y,
        _ => 120 * p.Z + p.Y
    };

    public static void ClassifyPlane(CpuContext c, IMemory m)
    {
        float near = m.ReadU16(m.ReadU32(c.GP + 0x1124) + 8);
        uint outside = 0;
        for (int i = 0; i < c.S4; i++)
            if (Distance(input[i].Position, c.S5, near) < 0) outside |= 1u << i;
        c.S3 = outside;
    }

    // All three native edge-dispatch sites pass (output index, inside, outside).
    // Intersect in homogeneous coordinates before rounding. The original six-step
    // bisection could place the upper edge of a near wall inside the viewport.
    public static void Intersect(CpuContext c, IMemory m)
    {
        Vertex a = input[c.A1], b = input[c.A2];
        float near = m.ReadU16(m.ReadU32(c.GP + 0x1124) + 8);
        double da = Distance(a.Position, c.S5, near), db = Distance(b.Position, c.S5, near);
        float t = (float)(da / (da - db));
        intersection = Vector3.Lerp(a.Position, b.Position, t);
        intersectionWeight = (int)MathF.Round(t * 4096);
        uint id = GteIds.Enabled ? GteIds.Combine(Math.Min(a.Id, b.Id), Math.Max(a.Id, b.Id), c.S5, -1, 0, 0) : 0;
        output[c.A0] = new(intersection, id);
    }

    // Feed the same intersection to the game's native UV and color interpolation.
    public static void NearWeight(CpuContext c, IMemory m) => c.T2 = (uint)intersectionWeight;

    public static void SideWeight(CpuContext c, IMemory m)
    {
        Gte.Write(8, (uint)intersectionWeight);
        if (c.S5 <= 2)
        {
            c.T5 = (uint)(int)(intersection.X * GpuHle.FovDen / (focal * GpuHle.FovNum));
            c.T6 = (uint)(int)intersection.Z;
        }
        else
        {
            c.T6 = (uint)(int)(intersection.Y / focal);
            c.T5 = (uint)(int)intersection.Z;
        }
    }

    public static void FinishPlane(CpuContext c, IMemory m)
    {
        // Intersect already filled each crossing at its native destination index.
        // Copy the retained original vertices in the native polygon's order.
        int n = (int)c.S4, written = 0;
        for (int i = 0; i < n; i++)
        {
            bool outside = (c.S3 & (1u << i)) != 0;
            bool nextOutside = (c.S3 & (1u << ((i + 1) % n))) != 0;
            if (!outside) output[written++] = input[i];
            if (outside != nextOutside) written++;
        }
        (input, output) = (output, input);
    }

    public static void TriangleGrid(CpuContext c, IMemory m)
    {
        // The triangle builder saves 5 * A0 and the original A1 and A2.
        Vertex a = input[m.ReadU32(c.SP + 0x30C) / 5];
        Vertex b = input[m.ReadU32(c.SP + 0x364)], d = input[m.ReadU32(c.SP + 0x368)];
        uint cursor = m.ReadU32(c.GP + 0x10FC) + 220 * 8;
        for (int row = 0; row <= 4; row++)
        for (int col = 0; col <= row; col++, cursor += 8)
        {
            int wa = 4 - row, wb = row - col;
            Vector3 point = (wa * a.Position + wb * b.Position + col * d.Position) * 0.25f;
            uint id = 0;
            if (GteIds.Enabled)
                id = wa == 4 ? a.Id : wb == 4 ? b.Id : col == 4 ? d.Id :
                    GteIds.Combine(a.Id, b.Id, d.Id, wa, wb, col);
            Project(m, cursor, point, id);
        }
    }

    public static void QuadGrid(CpuContext c, IMemory m)
    {
        uint args = c.SP + 0x398;
        Vertex a = input[m.ReadU32(args)], b = input[m.ReadU32(args + 4)];
        Vertex d = input[m.ReadU32(args + 8)], e = input[m.ReadU32(args + 12)];
        uint cursor = m.ReadU32(c.GP + 0x10FC) + 220 * 8;
        for (int row = 0; row <= 4; row++)
        {
            Vertex left = Quarter(a, d, row), right = Quarter(b, e, row);
            for (int col = 0; col <= 4; col++, cursor += 8)
            {
                Vertex v = Quarter(left, right, col);
                Project(m, cursor, v.Position, v.Id);
            }
        }
    }

    static Vertex Quarter(Vertex a, Vertex b, int step)
    {
        if (step == 0) return a;
        if (step == 4) return b;
        uint id = GteIds.Enabled ? GteIds.Combine(a.Id, b.Id, 0, 4 - step, step, 0) : 0;
        return new(Vector3.Lerp(a.Position, b.Position, step * 0.25f), id);
    }

    static void Project(IMemory m, uint address, Vector3 p, uint id)
    {
        if (p.Z <= 0 || m is not PSMemory ps) return;
        float x = p.X / p.Z, y = p.Y / p.Z;
        // CPU projection truncates the offset before adding the screen centre.
        // Update X for native winding and bounds decisions in widescreen. Preserve
        // the retail integer coordinates at 4:3 and certify the precise projection.
        if (GpuHle.FovNum != GpuHle.FovDen)
            m.WriteU16(address, (ushort)(256 + (int)x));
        uint word = m.ReadU32(address);
        var tag = new GteScreen.VertexTag(p.Z, 256 + x, 120 + y, true)
            { NativeScreen = 0x80000000u | (word & 0x07FF07FFu) };
        ps.TagGteVertex(address, word, tag);
        if (id != 0) GteIds.Name(id, tag.ScreenX, tag.ScreenY, tag.Depth);
    }
}
