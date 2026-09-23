using System.Buffers.Binary;
using System.Runtime.InteropServices;

namespace RecompOne.Runtime.Assets.Suits;

/// <summary>Full imported rig stored in RTG2 native metadata. No guest addresses in the file.</summary>
public sealed unsafe class NativeRetargetRig
{
    public const uint Tag = 0x32475452;
    readonly byte[] _data;
    readonly float[] _bonePose, _world;
    readonly float[][] _packetWorld, _packetLocal;
    public int BoneCount { get; }
    public int VertexCount { get; }
    public int[][] Packets { get; }
    public float[][] LocalVertices => _packetLocal;
    public ReadOnlyMemory<byte> PreservedData => _data;

    static class Core
    {
        [DllImport("OpenSpideyRetarget", CallingConvention = CallingConvention.Cdecl)] internal static extern int rtg_pose(byte* data, uint length, float* drivers, uint flags, float* bones, uint boneFloats);
        [DllImport("OpenSpideyRetarget", CallingConvention = CallingConvention.Cdecl)] internal static extern int rtg_web_target(byte* data, uint length, float* bones, uint boneFloats, uint part, short* actorR, int* actorT, int* actorPosition, uint mirror, int* output);
        [DllImport("OpenSpideyRetarget", CallingConvention = CallingConvention.Cdecl)] internal static extern int rtg_web_segment(int* start, int* end, int* tip, int* target, uint index, uint count, int* output);
        const string Library = "OpenSpideyRetarget";
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern uint rtg_abi();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int rtg_validate(byte* data, uint length);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int rtg_evaluate(byte* data, uint length, float* drivers, uint flags, float* bones, uint boneFloats, float* output, uint outputFloats);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int rtg_to_part(float* driver, float* world, uint count, float* local);
    }
    public NativeRetargetRig(byte[] data)
    {
        _data = (byte[])data.Clone();
        if (Core.rtg_abi() != 0x00020002) throw new InvalidDataException("retarget core ABI mismatch");
        fixed (byte* p = _data)
        {
            int e = Core.rtg_validate(p, (uint)_data.Length);
            if (e != 0) throw new InvalidDataException($"invalid native retarget rig (code {e})");
        }
        int U(int p) => checked((int)BinaryPrimitives.ReadUInt32LittleEndian(_data.AsSpan(p)));
        BoneCount = U(12); VertexCount = U(16);
        _bonePose = new float[BoneCount * 12]; _world = new float[VertexCount * 6];
        Packets = new int[18][]; _packetWorld = new float[18][]; _packetLocal = new float[18][];
        int table = U(40);
        for (int i = 0; i < 18; i++)
        {
            int count = U(table + i * 8), offset = U(table + i * 8 + 4);
            Packets[i] = new int[count];
            for (int v = 0; v < count; v++) Packets[i][v] = U(offset + v * 4);
            _packetWorld[i] = new float[count * 6]; _packetLocal[i] = new float[count * 6];
        }
    }
    /// <summary>Compute everything before writing any guest memory. No GPU, GTE or CPU-state mutation.</summary>
    public void Evaluate(float[] drivers, bool openHands = false)
    {
        if (drivers.Length != 18 * 12) throw new ArgumentException("18 driver matrices required");
        fixed (byte* d = _data)
        fixed (float* input = drivers, bones = _bonePose, world = _world)
        {
            int e = Core.rtg_evaluate(d, (uint)_data.Length, input, openHands ? 1u : 0u,
                bones, (uint)_bonePose.Length, world, (uint)_world.Length);
            if (e != 0) throw new InvalidDataException($"retarget evaluation failed ({e})");
            for (int i = 0; i < 18; i++)
            {
                for (int v = 0; v < Packets[i].Length; v++)
                    Array.Copy(_world, Packets[i][v] * 6, _packetWorld[i], v * 6, 6);
                if (Packets[i].Length == 0) continue;
                fixed (float* w = _packetWorld[i], l = _packetLocal[i])
                    e = Core.rtg_to_part(input + i * 12, w, (uint)Packets[i].Length, l);
                if (e != 0) throw new InvalidDataException($"retarget part transform failed ({i}:{e})");
                for (int v = 0; v < Packets[i].Length; v++)
                    for (int k = 0; k < 3; k++)
                    {
                        float x = _packetLocal[i][v * 6 + k];
                        if (!float.IsFinite(x) || MathF.Abs(x) > 32760)
                            throw new InvalidDataException("retarget vertex exceeds native signed-16 range");
                    }
            }
        }
    }
    /// <summary>Use current native pose; no skinning/vertex writes needed for a rope socket.</summary>
    public void EvaluatePose(float[] drivers)
    {
        if (drivers.Length != 216) throw new ArgumentException("18 driver matrices required");
        fixed (byte* d = _data)
        fixed (float* input = drivers, bones = _bonePose)
        {
            int e = Core.rtg_pose(d, (uint)_data.Length, input, 0, bones, (uint)_bonePose.Length);
            if (e != 0) throw new InvalidDataException($"retarget socket pose failed ({e})");
        }
    }
    /// <summary>Return preserved palm origin in original world Q12, using the native actor transform.</summary>
    public bool TryGetWebTarget(uint nativePart, short[] rotation, int[] translation, int[] actorPosition, bool mirror, int[] output)
    {
        if (rotation.Length != 9 || translation.Length != 3 || actorPosition.Length != 3 || output.Length != 3)
            throw new ArgumentException("invalid web transform buffer");
        fixed (byte* d = _data)
        fixed (float* bones = _bonePose)
        fixed (short* r = rotation)
        fixed (int* t = translation, p = actorPosition, o = output)
            return Core.rtg_web_target(d, (uint)_data.Length, bones, (uint)_bonePose.Length, nativePart, r, t, p, mirror ? 1u : 0u, o) == 0;
    }
    /// <summary>Render scratch only: bend native line onto the wrist, keeping its anchor unchanged.</summary>
    public static bool TryWarpWebSegment(int[] start, int[] end, int[] nativeTip, int[] target, uint index, uint count, int[] output)
    {
        if (start.Length != 3 || end.Length != 3 || nativeTip.Length != 3 || target.Length != 3 || output.Length != 6)
            throw new ArgumentException("invalid web segment buffer");
        fixed (int* a = start, b = end, tip = nativeTip, wrist = target, o = output)
            return Core.rtg_web_segment(a, b, tip, wrist, index, count, o) == 0;
    }
}
