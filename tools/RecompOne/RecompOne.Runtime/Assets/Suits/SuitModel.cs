using System.Buffers.Binary;

namespace RecompOne.Runtime.Assets.Suits;

/// <summary>A bounded native, 18-part player actor. FBX conversion happens offline.</summary>
public sealed class SuitModel
{
    public const int ByteLimit = 4 * 1024 * 1024;
    public const int GuestByteLimit = 1024 * 1024;
    readonly byte[] _bytes;
    public ReadOnlyMemory<byte> Bytes => _bytes;
    public IReadOnlySet<uint> Materials { get; }
    public NativeRetargetRig? RetargetRig { get; }
    SuitModel(byte[] bytes, IReadOnlySet<uint> materials, NativeRetargetRig? rig = null) { _bytes = bytes; Materials = materials; RetargetRig = rig; }

    public static SuitModel Read(string path)
    {
        using var stream = File.OpenRead(path);
        if (stream.Length < 1024 || stream.Length > ByteLimit)
            throw new InvalidDataException("custom player model must be 1 KiB..4 MiB including preserved rig metadata");
        byte[] data = new byte[(int)stream.Length];
        stream.ReadExactly(data);
        return Parse(data);
    }

    public static SuitModel Parse(ReadOnlySpan<byte> source)
    {
        if (source.Length < 1024 || source.Length > ByteLimit)
            throw new InvalidDataException("custom player model exceeds its source file limit");
        byte[] b = source.ToArray();
        void Need(long offset, long size)
        {
            if (offset < 0 || size < 0 || offset + size > b.Length)
                throw new InvalidDataException("custom player model contains an out-of-bounds record");
        }
        uint U(int p) { Need(p, 4); return BinaryPrimitives.ReadUInt32LittleEndian(b.AsSpan(p)); }
        ushort H(int p) { Need(p, 2); return BinaryPrimitives.ReadUInt16LittleEndian(b.AsSpan(p)); }
        int Count(int p, int max) { uint n = U(p); if (n > max) throw new InvalidDataException("custom player model count exceeds its supported limit"); return (int)n; }
        if (b.Length < 1024 || b.Length > ByteLimit || H(0) != 4 || H(2) != 2 || U(8) != 18)
            throw new InvalidDataException("custom player model must be a native v4 18-part Spider-Man actor");
        const int table = 12 + 18 * 36;
        if (U(table) != 18) throw new InvalidDataException("custom player model must retain all 18 native meshes");
        int metadata = checked((int)U(4));
        Need(metadata, 4);
        int sources = 0;
        for (int i = 0; i < 18; i++)
        {
            int start = checked((int)U(table + 4 + i * 4));
            int end = i == 17 ? metadata : checked((int)U(table + 8 + i * 4));
            if (start < table + 4 + 18 * 4 || end < start + 28 || end > metadata)
                throw new InvalidDataException("invalid custom mesh pointer order");
            int vertices = H(start + 2), normals = H(start + 4), faces = H(start + 6);
            if (vertices > 256 || normals != vertices + faces || faces > 4096)
                throw new InvalidDataException("custom mesh exceeds native vertex/normal/face limits");
            int cursor = start + 28;
            Need(cursor, vertices * 8L + normals * 8L + faces * 36L);
            for (int v = 0; v < vertices; v++, cursor += 8)
            {
                int kind = H(cursor + 6);
                if (kind == 1) sources++;
                else if (kind == 2)
                {
                    if (H(cursor + 2) >= sources) throw new InvalidDataException("custom mesh has a forward or missing stitch source");
                }
                else if (kind != 0) throw new InvalidDataException("unsupported custom vertex record");
            }
            cursor += normals * 8;
            for (int f = 0; f < faces; f++)
            {
                int flags = H(cursor), length = H(cursor + 2);
                // Exact opaque native formats present in the bundled SM1 actors.
                // Keep extended triangles and quads intact when changing a rig.
                if (!((flags == 0x1f || flags == 0x0f) && length == 36 || flags == 0x3f && length == 40))
                    throw new InvalidDataException("unsupported custom face record");
                Need(cursor, length);
                if (cursor + length > end || b[cursor + 4] >= vertices || b[cursor + 5] >= vertices || b[cursor + 6] >= vertices ||
                    flags == 0x0f && b[cursor + 7] >= vertices || H(cursor + 12) >= normals)
                    throw new InvalidDataException("invalid custom face or normal reference");
                cursor += length;
            }
            if (cursor != end) throw new InvalidDataException("unexpected custom mesh trailing data");
        }
        int rigStart = -1, rigBlockSize = 0;
        NativeRetargetRig? rig = null;
        int p = metadata;
        for (int blocks = 0; ; blocks++)
        {
            uint tag = U(p); p += 4;
            if (tag == uint.MaxValue) break;
            if (blocks >= 64) throw new InvalidDataException("too many custom actor metadata blocks");
            int size = checked((int)U(p)); p += 4; Need(p, size);
            if (tag == NativeRetargetRig.Tag)
            {
                if (rigStart >= 0) throw new InvalidDataException("duplicate RTG2 rig");
                rigStart = p - 8; rigBlockSize = checked(size + 8);
                rig = new NativeRetargetRig(b.AsSpan(p, size).ToArray());
                for (int i = 0; i < 18; i++)
                    if (H(checked((int)U(table + 4 + i * 4)) + 2) != rig.Packets[i].Length)
                        throw new InvalidDataException("RTG2 packet mapping differs from native mesh");
            }
            p += size;
        }
        Need(p, 18 * 4); p += 18 * 4;
        int hashes = Count(p, 128); p += 4;
        var materials = new HashSet<uint>();
        for (int i = 0; i < hashes; i++, p += 4) materials.Add(U(p));
        if (materials.Count == 0) throw new InvalidDataException("custom actor has no materials");
        int palette4 = Count(p, 128); p += 4; Need(p, palette4 * 36); p += palette4 * 36;
        int palette8 = Count(p, 128); p += 4; Need(p, palette8 * 516); p += palette8 * 516;
        if (U(p) == uint.MaxValue)
        {
            p += 4; int details = Count(p, 128); p += 4; Need(p, details * 36); p += details * 36;
            int cubes = Count(p, 128); p += 4; Need(p, cubes * 36); p += cubes * 36;
        }
        int textures = Count(p, 128); p += 4;
        Need(p, textures * 4);
        for (int i = 0; i < textures; i++)
        {
            int t = checked((int)U(p + i * 4)); Need(t, 20);
            if (t < p + textures * 4 || H(t + 16) == 0 || H(t + 18) == 0 || H(t + 16) > 256 || H(t + 18) > 256 || U(t + 12) >= hashes)
                throw new InvalidDataException("invalid custom actor texture record");
            int bytes = checked(H(t + 16) * H(t + 18));
            // Native flags 0x100 select 8-bit indices; otherwise the donor uses 4-bit.
            if ((U(t + 4) & 0x100) == 0) bytes = (bytes + 1) / 2;
            Need(t + 20, bytes);
        }
        if (rigStart >= 0)
        {
            // Keep the complete rig host-side; give the original guest loader a
            // strictly legacy-native stream. No unknown-tag behavior is assumed.
            byte[] guest = new byte[b.Length - rigBlockSize];
            b.AsSpan(0, rigStart).CopyTo(guest);
            b.AsSpan(rigStart + rigBlockSize).CopyTo(guest.AsSpan(rigStart));
            for (int i = 0; i < textures; i++)
            {
                uint texture = U(p + i * 4);
                if (texture < rigStart + rigBlockSize) throw new InvalidDataException("overlapping RTG2 texture");
                BinaryPrimitives.WriteUInt32LittleEndian(guest.AsSpan(p - rigBlockSize + i * 4), texture - (uint)rigBlockSize);
            }
            b = guest;
        }
        if (b.Length > GuestByteLimit)
            throw new InvalidDataException("custom player geometry exceeds the 1 MiB guest limit after rig removal");
        NativeActorLodValidator.Validate(b, "custom player model", LooseWadOverrides.MaxUnpackedAnimationBones);
        return new SuitModel(b, materials, rig);
    }
}
