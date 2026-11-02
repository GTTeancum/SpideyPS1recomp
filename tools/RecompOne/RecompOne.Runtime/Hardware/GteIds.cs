using System.Collections.Generic;

namespace RecompOne.Runtime.Hardware;

/// <summary>
/// A name for every point the GTE projects that stays the same from one game frame to
/// the next, so frame interpolation can tell where each vertex was a frame ago.
///
/// The name comes from what was projected, not where it landed: the RAM address the
/// model-space vertex was loaded from (LWC2), plus its model coordinates when the
/// address is a reused scratch buffer or unknown, plus how many times that source has
/// already been projected since the last display swap (one mesh drawn for several
/// actors). Main-RAM sources are named by address alone so meshes the host rewrites
/// every frame (retargeted suits) keep their names.
///
/// The name reaches the GPU packet by the exact projected screen position, which the
/// existing provenance already carries to every validated packet vertex.
/// </summary>
public static class GteIds
{
    public static bool Enabled;
    public static long Named, NameMisses;

    static readonly Dictionary<uint, int> _occurrences = new(16384);
    static Dictionary<ulong, (float Z, uint Id, uint Alt)> _current = new(16384), _previous = new(16384);

    static ulong Key(float x, float y) =>
        ((ulong)BitConverter.SingleToUInt32Bits(x) << 32) | BitConverter.SingleToUInt32Bits(y);

    static uint Mix(uint h, uint v)
    {
        h ^= v * 0x9E3779B1u;
        h = (h << 13) | (h >> 19);
        return h * 5 + 0xE6546B64u;
    }

    /// <summary>
    /// Games that move the world around the camera on the CPU (vertex minus camera
    /// position, then RTPS with the camera rotation and no translation) set this to the
    /// RAM address of the camera position: three 32-bit words, X, Y, Z. Adding it back
    /// names world geometry by where it is in the world rather than by where the camera
    /// happens to be this frame.
    /// </summary>
    public static uint CameraPosition;

    /// <summary>RAM address of the world camera's 3x3 rotation (4.12 shorts), with CameraPosition.</summary>
    public static uint CameraRotation;

    /// <summary>The world camera as the current frame projected scenery with it.</summary>
    public struct Camera
    {
        public bool Valid;
        public double[] R, C;
        public double H, OfX, OfY, Squeeze;
    }

    public static Camera FrameCamera = new() { R = new double[9], C = new double[3] };
    static bool _cameraNoted;

    static void NoteCamera(RecompOne.Runtime.Memory.IMemory mem)
    {
        _cameraNoted = true;
        if (CameraRotation == 0) return;
        var cam = new Camera { Valid = true, R = new double[9], C = new double[3] };
        for (uint i = 0; i < 9; i++) cam.R[i] = (short)mem.ReadU16(CameraRotation + i * 2) / 4096.0;
        for (uint i = 0; i < 3; i++) cam.C[i] = (int)mem.ReadU32(CameraPosition + i * 4);
        cam.H = (ushort)RecompOne.Runtime.Gte.ReadControl(26);
        cam.OfX = (int)RecompOne.Runtime.Gte.ReadControl(24) / 65536.0;
        cam.OfY = (int)RecompOne.Runtime.Gte.ReadControl(25) / 65536.0;
        cam.Squeeze = (double)Hle.GpuHle.FovNum / Hle.GpuHle.FovDen;
        FrameCamera = cam;
    }

    /// <summary>
    /// An untranslated projection may be camera-relative scenery or something drawn
    /// around the camera on purpose (the sky). It gets both names, the world position
    /// first, and interpolation uses whichever one the previous frame also had.
    /// </summary>
    public static void Note(uint source, int vx, int vy, int vz, float x, float y, float z, bool translated)
    {
        uint raw = Name(source, vx, vy, vz), world = 0;
        if (!translated && CameraPosition != 0 && RecompOne.Runtime.Runtime.Mem is { } mem)
        {
            if (!_cameraNoted) NoteCamera(mem);
            world = Name(source, vx + (int)mem.ReadU32(CameraPosition),
                vy + (int)mem.ReadU32(CameraPosition + 4), vz + (int)mem.ReadU32(CameraPosition + 8));
        }
        _current[Key(x, y)] = world != 0 ? (z, world, raw) : (z, raw, 0u);
    }

    static uint Name(uint source, int vx, int vy, int vz)
    {
        bool mainRam = source != 0 && (source & 0x1FFFFFFFu) < 0x800000u;
        uint key = Mix(0x2545F491u, source);
        if (!mainRam) key = Mix(Mix(Mix(key, (uint)vx), (uint)vy), (uint)vz);
        _occurrences.TryGetValue(key, out int n);
        _occurrences[key] = n + 1;
        return (Mix(key, (uint)n) | 1u) & 0x7FFFFFFFu;
    }

    /// <summary>
    /// Name a point a game patch derived from named points (a subdivision vertex is
    /// its three corners and its weights), replacing the name its own RTPS was given.
    /// </summary>
    public static void Name(uint id, float x, float y, float z) { Named++; _current[Key(x, y)] = (z, id | 1u, 0u); }

    public static uint Combine(uint a, uint b, uint c, int wa, int wb, int wc) =>
        Mix(Mix(Mix(Mix(Mix(Mix(0x51ED270Bu, a), b), c), (uint)wa), (uint)wb), (uint)wc) | 0x80000001u;

    public static uint Find(float x, float y, float z) => Find(x, y, z, out _);

    public static uint Find(float x, float y, float z, out uint alt)
    {
        ulong key = Key(x, y);
        alt = 0;
        if (!_current.TryGetValue(key, out var hit) || hit.Z != z)
            if (!_previous.TryGetValue(key, out hit) || hit.Z != z) return 0;
        alt = hit.Alt;
        return hit.Id;
    }

    /// <summary>Called once per actual display-buffer swap, with GteScreen.Roll.</summary>
    public static void Roll()
    {
        (_previous, _current) = (_current, _previous);
        _current.Clear();
        _occurrences.Clear();
        _cameraNoted = false;
    }
}
