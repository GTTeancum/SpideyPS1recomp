using System;
using System.IO;
using System.Numerics;
using RecompOne.Runtime.Assets.Suits;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>SM1-only pose-ready hook at 80077418, before native lighting and vertex transforms.</summary>
public static class SuitRetargeting
{
    static readonly float[] Driver = new float[18 * 12];
    static readonly uint[] Mesh = new uint[18];
    static readonly bool Trace = Environment.GetEnvironmentVariable("SPIDEY_RETARGET_TRACE") == "1";
    static NativeRetargetRig _reported;
    static bool Ram(uint p, uint n) => n <= 0x800000 && p >= 0x80000000 && p <= 0x80800000 - n;
    static short Quantize(float x) => checked((short)MathF.Round(x));
    static Vector3 Point(float[] v, int i) => new(v[i * 6], v[i * 6 + 1], v[i * 6 + 2]);
    static void Normal(IMemory m, uint p, Vector3 n)
    {
        n = n.LengthSquared() > 1e-12f ? Vector3.Normalize(n) : new Vector3(0, -1, 0);
        m.WriteU16(p, unchecked((ushort)Quantize(n.X * 4096)));
        m.WriteU16(p + 2, unchecked((ushort)Quantize(n.Y * 4096)));
        m.WriteU16(p + 4, unchecked((ushort)Quantize(n.Z * 4096)));
    }
    public static void ApplyPose(CpuContext c, IMemory m)
    {
        // Do not key on title/level slot numbers. Those addresses are reused by NPCs.
        int selected = Costume.LoadedCostume;
        if (selected != SuitMods.Active || !SuitMods.IsMod(selected)) return;
        NativeRetargetRig rig = SuitMods.At(selected).CustomModel?.RetargetRig;
        if (rig == null) return; // Existing non-retargeted actors retain their original path.
        uint actor = c.S2, pose = c.S3;
        if (!Ram(actor, 0x198) || !Ram(pose, 18 * 24)) return;
        uint slot = m.ReadU8(actor + 0x1B);
        if (slot >= 40) return;
        uint entry = 0x800A0904 + slot * 64;
        if (m.ReadU32(entry) != 0x64697073 || m.ReadU16(entry + 4) != 0x7965 || m.ReadU8(entry + 6) != 0) return;
        uint table = m.ReadU32(entry + 0x10);
        if (!Ram(table - 4, 4 + 18 * 4) || m.ReadU32(table - 4) != 18) return;
        for (int i = 0; i < 18; i++)
        {
            uint p = pose + (uint)i * 24;
            for (int r = 0; r < 3; r++)
            {
                for (int k = 0; k < 3; k++)
                    Driver[i * 12 + r * 4 + k] = unchecked((short)m.ReadU16(p + (uint)(r * 3 + k) * 2)) / 4096f;
                Driver[i * 12 + r * 4 + 3] = unchecked((short)m.ReadU16(p + 18 + (uint)r * 2));
            }
            Mesh[i] = m.ReadU32(table + (uint)i * 4);
            if (!Ram(Mesh[i], 28) || m.ReadU16(Mesh[i] + 2) != rig.Packets[i].Length)
                throw new InvalidDataException("active retarget rig does not match resident native mesh");
            uint nv = m.ReadU16(Mesh[i] + 2), nn = m.ReadU16(Mesh[i] + 4), nf = m.ReadU16(Mesh[i] + 6);
            if (nn != nv + nf || !Ram(Mesh[i], 28 + nv * 8 + nn * 8 + nf * 36))
                throw new InvalidDataException("invalid resident retarget mesh bounds");
            uint faceStart = Mesh[i] + 28 + (nv + nn) * 8;
            for (uint f = 0; f < nf; f++)
                if (m.ReadU8(faceStart + f * 36 + 4) >= nv || m.ReadU8(faceStart + f * 36 + 5) >= nv || m.ReadU8(faceStart + f * 36 + 6) >= nv)
                    throw new InvalidDataException("invalid resident retarget face before skinning");
        }
        rig.Evaluate(Driver); // Fists by default. All validation/evaluation precedes guest writes.
        for (int i = 0; i < 18; i++)
        {
            float[] v = rig.LocalVertices[i]; uint mesh = Mesh[i]; int count = rig.Packets[i].Length;
            uint start = mesh + 28, normals = start + (uint)count * 8;
            int nf = m.ReadU16(mesh + 6); uint faces = normals + (uint)(count + nf) * 8;
            float radius = 0; Vector3 min = new(float.MaxValue), max = new(float.MinValue);
            for (int j = 0; j < count; j++)
            {
                Vector3 p = Point(v, j); radius = MathF.Max(radius, p.Length()); min = Vector3.Min(min, p); max = Vector3.Max(max, p);
                uint address = start + (uint)j * 8;
                m.WriteU16(address, unchecked((ushort)Quantize(p.X)));
                m.WriteU16(address + 2, unchecked((ushort)Quantize(p.Y)));
                m.WriteU16(address + 4, unchecked((ushort)Quantize(p.Z)));
                Normal(m, normals + (uint)j * 8, new Vector3(v[j * 6 + 3], v[j * 6 + 4], v[j * 6 + 5]));
            }
            for (int j = 0; j < nf; j++)
            {
                uint f = faces + (uint)j * 36;
                // Native on-disk winding is (a,c,b); vertex bytes are retained by ParsePSX.
                int a = m.ReadU8(f + 4), b = m.ReadU8(f + 6), d = m.ReadU8(f + 5);
                if (a >= count || b >= count || d >= count) throw new InvalidDataException("invalid resident retarget face");
                Normal(m, normals + (uint)(count + j) * 8, Vector3.Cross(Point(v, b) - Point(v, a), Point(v, d) - Point(v, a)));
            }
            // Update culling bounds because transport-packet ownership is not skin ownership.
            m.WriteU32(mesh + 8, checked((uint)MathF.Ceiling(radius) * 256));
            if (count > 0)
            {
                float[] mn = [min.X, min.Y, min.Z], mx = [max.X, max.Y, max.Z];
                for (int k = 0; k < 3; k++)
                {
                    m.WriteU16(mesh + 12 + (uint)k * 4, unchecked((ushort)(short)MathF.Ceiling(mx[k] / 16)));
                    m.WriteU16(mesh + 14 + (uint)k * 4, unchecked((ushort)(short)MathF.Floor(mn[k] / 16)));
                }
            }
        }
        if (Trace && !ReferenceEquals(_reported, rig))
        {
            _reported = rig;
            Console.WriteLine($"[retarget] live pose @ {pose:X8}: {rig.BoneCount} preserved bones, {rig.VertexCount} weighted vertices; fists; actor {actor:X8}");
        }
    }
}
