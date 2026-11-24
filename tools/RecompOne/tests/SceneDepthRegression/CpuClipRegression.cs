using System.Numerics;
using Recompiled;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hle;
using RecompOne.Runtime.Memory;

static class CpuClipRegression
{
    public static void Run(Action<bool, string> check)
    {
        foreach (bool wide in new[] { false, true })
        {
            GpuHle.FovNum = wide ? 3 : 1;
            GpuHle.FovDen = wide ? 4 : 1;
            TestPolygon(wide, check);
        }
        GpuHle.FovNum = GpuHle.FovDen = 1;
    }

    static void TestPolygon(bool wide, Action<bool, string> check)
    {
        var mem = new PSMemory(0x800000);
        var cpu = new CpuContext { GP = 0x800B47F4, SP = 0x807EF000 };
        const uint projected = 0x80300000, camera = 0x80310000, view = 0x80320000;
        mem.WriteU32(cpu.GP + 0x10FC, projected);
        mem.WriteU32(cpu.GP + 0x1124, view);
        mem.WriteU16(view + 14, 256);
        mem.WriteU16(view + 8, 150);
        float fx = wide ? 192 : 256;
        Vector3[] points = [new(230.5f,-10,200), new(12.25f,-4,250), new(23.5f,28,100)];
        for (int i = 0; i < 3; i++)
        {
            var v = points[i];
            cpu.T0 = (uint)i;
            cpu.A0 = projected + (uint)i * 8;
            cpu.A1 = camera + (uint)i * 8;
            mem.WriteU32(cpu.A0, 0);
            mem.WriteU16(cpu.A1, (ushort)(short)v.X);
            mem.WriteU16(cpu.A1 + 2, (ushort)(short)v.Y);
            mem.WriteU16(cpu.A0 + 4, (ushort)v.Z);
            // The vertex leaving the frustum exercises the native camera fallback.
            if (i != 2)
                mem.TagGteVertex(cpu.A0, 0, new(v.Z, 256 + v.X * fx / v.Z, 120 + v.Y * 256 / v.Z, true));
            CpuWorldProjection.SourceVertex(cpu, mem);
        }
        points[2].X = 23;
        cpu.S4 = 3;
        cpu.S5 = 1;
        CpuWorldProjection.ClassifyPlane(cpu, mem);
        check(cpu.S3 == (wide ? 0u : 1u), $"Horizontal clip bounds follow projected FOV, wide={wide}");
        cpu.S5 = 0;
        CpuWorldProjection.ClassifyPlane(cpu, mem);
        check(cpu.S3 == 4, "Near plane rejects only corner 2");

        // The native polygon is [0, 1, cut(1,2), cut(0,2)]. Check geometry and
        // the guest's UV interpolation weight against independent intersections.
        cpu.A0 = 2; cpu.A1 = 1; cpu.A2 = 2;
        CpuWorldProjection.Intersect(cpu, mem);
        CpuWorldProjection.NearWeight(cpu, mem);
        check(cpu.T2 == 2731, "Near-plane UVs follow the geometric intersection");
        cpu.A0 = 3; cpu.A1 = 0; cpu.A2 = 2;
        CpuWorldProjection.Intersect(cpu, mem);
        CpuWorldProjection.FinishPlane(cpu, mem);
        cpu.S4 = 4;
        cpu.S5 = 3;
        CpuWorldProjection.ClassifyPlane(cpu, mem);
        CpuWorldProjection.FinishPlane(cpu, mem);
        Vector3 cut12 = Vector3.Lerp(points[1], points[2], 2f / 3f);
        Vector3 cut02 = Vector3.Lerp(points[0], points[2], 0.5f);
        uint start = projected + 220 * 8;
        void Fill(int count)
        {
            for (int i = 0; i <= count; i++) mem.WriteU32(start + (uint)i * 8, 0x00780100);
        }
        bool Matches(int i, Vector3 v)
        {
            uint ptr = start + (uint)i * 8;
            uint word = mem.ReadU32(ptr);
            float x = v.X * fx / v.Z, y = v.Y * 256 / v.Z;
            return (short)word == (wide ? 256 + (int)x : 256) &&
                mem.TryGetGteVertex(ptr, word, out var tag) && tag.HasSubpixel &&
                Math.Abs(tag.ScreenX - (256 + x)) < 0.001 &&
                Math.Abs(tag.ScreenY - (120 + y)) < 0.001 && Math.Abs(tag.Depth - v.Z) < 0.001;
        }
        mem.WriteU32(cpu.SP + 0x30C, 0);
        mem.WriteU32(cpu.SP + 0x364, 1);
        mem.WriteU32(cpu.SP + 0x368, 2);
        Fill(15);
        CpuWorldProjection.TriangleGrid(cpu, mem);
        int vertex = 0;
        for (int row = 0; row <= 4; row++)
        for (int col = 0; col <= row; col++)
        {
            Vector3 v = ((4-row)*points[0] + (row-col)*points[1] + col*cut12) / 4;
            if (!Matches(vertex++, v)) throw new Exception($"Triangle vertex {row},{col}, wide={wide}");
        }
        check(!mem.TryGetGteVertex(start + 15*8, 0x00780100, out _),
            $"CPU triangle has 15 precise vertices and preserves the following slot, wide={wide}");
        mem.WriteU32(cpu.SP + 0x398, 0);
        mem.WriteU32(cpu.SP + 0x39C, 1);
        mem.WriteU32(cpu.SP + 0x3A0, 3);
        mem.WriteU32(cpu.SP + 0x3A4, 2);
        Fill(25);
        CpuWorldProjection.QuadGrid(cpu, mem);
        vertex = 0;
        for (int row = 0; row <= 4; row++)
        for (int col = 0; col <= 4; col++)
        {
            Vector3 l = Vector3.Lerp(points[0],cut02,row/4f), r = Vector3.Lerp(points[1],cut12,row/4f);
            if (!Matches(vertex++, Vector3.Lerp(l,r,col/4f))) throw new Exception($"Quad vertex {row},{col}, wide={wide}");
        }
        check(!mem.TryGetGteVertex(start + 25*8, 0x00780100, out _),
            $"CPU quad has 25 precise vertices and preserves the following slot, wide={wide}");
        // Clip the same input against the top edge using a shallow, receding
        // segment. The precise edge must stay at y=0 after projection.
        points[0] = new(-20,-137,250);
        points[1] = new(10,18,188);
        points[2] = new(36,47,300);
        for (int i = 0; i < 3; i++)
        {
            Vector3 v = points[i];
            cpu.T0 = (uint)i; cpu.A0 = projected + (uint)i*8;
            mem.TagGteVertex(cpu.A0, 0, new(v.Z, 256+v.X*fx/v.Z, 120+v.Y*256/v.Z, true));
            CpuWorldProjection.SourceVertex(cpu, mem);
        }
        cpu.S5 = 4; cpu.S4 = 3;
        CpuWorldProjection.ClassifyPlane(cpu, mem);
        cpu.A0 = 0; cpu.A1 = 1; cpu.A2 = 0;
        CpuWorldProjection.Intersect(cpu, mem);
        CpuWorldProjection.SideWeight(cpu, mem);
        float di = 120*points[1].Z + 256*points[1].Y, dt = 120*points[0].Z + 256*points[0].Y;
        check(Gte.Read(8) == (uint)MathF.Round(4096*di/(di-dt)), "Side-plane UV weight matches exact edge");
        cpu.A0 = 3; cpu.A1 = 2; cpu.A2 = 0;
        CpuWorldProjection.Intersect(cpu, mem);
        CpuWorldProjection.FinishPlane(cpu, mem);
        Fill(15);
        CpuWorldProjection.TriangleGrid(cpu, mem);
        check(mem.TryGetGteVertex(start, mem.ReadU32(start), out var clippedTop) &&
            clippedTop.HasSubpixel && Math.Abs(clippedTop.ScreenY) < 0.001f,
            $"The clipped wall reaches the viewport edge at subpixel precision, wide={wide}");
    }
}
