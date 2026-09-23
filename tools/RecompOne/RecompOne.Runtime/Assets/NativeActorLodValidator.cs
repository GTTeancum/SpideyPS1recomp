using System.Buffers.Binary;

namespace RecompOne.Runtime.Assets;

/// <summary>
/// Checks the issue #5 native animation-stride invariant BEFORE loading a loose
/// actor. The native loader counts terminal LOD meshes as bones; extra terminals
/// used to overwrite adjacent collision memory. No asset bytes are modified.
/// </summary>
public static class NativeActorLodValidator
{
    public readonly record struct Layout(uint Objects, uint Meshes, uint TerminalLods, bool Animated);

    public static Layout Validate(ReadOnlySpan<byte> data, string name, int maxUnpackedAnimationBones = 0)
    {
        if (maxUnpackedAnimationBones < 0) throw new ArgumentOutOfRangeException(nameof(maxUnpackedAnimationBones));
        Need(data, 0, 12, name);
        uint objects = U32(data, 8, name);
        if (objects == 0) return new(0, 0, 0, false); // texture-only companion
        long table = 12L + objects * 36L;
        Need(data, 12, objects * 36L, name);
        long metadata = U32(data, 4, name);
        if (metadata < table + 4)
            throw Bad(name, "metadata overlaps the native object table");
        Need(data, metadata, 4, name);
        long cursor = metadata;
        bool animated = false, unpacked = false;
        int blocks = 0;
        for (;;)
        {
            uint tag = U32(data, cursor, name);
            cursor += 4;
            if (tag == uint.MaxValue) break;
            if (++blocks > 4096) throw Bad(name, "too many native metadata blocks");
            uint size = U32(data, cursor, name);
            cursor += 4;
            Need(data, cursor, size, name);
            animated |= tag is 0x2A or 0x2C;
            unpacked |= tag == 0x2A;
            cursor += size;
        }
        if (!animated) return new(objects, 0, 0, false);
        uint count = U32(data, table, name);
        if (count > (metadata - table - 4) / 4)
            throw Bad(name, "mesh pointer table overlaps metadata");
        Need(data, table + 4, count * 4L, name);
        long firstMesh = table + 4 + count * 4L;
        uint terminal = 0;
        for (uint i = 0; i < count; ++i)
        {
            long mesh = U32(data, table + 4 + i * 4L, name);
            if (mesh < firstMesh || mesh > metadata - 28)
                throw Bad(name, "native mesh header is outside its section");
            if (BinaryPrimitives.ReadUInt16LittleEndian(data.Slice((int)mesh + 26, 2)) == ushort.MaxValue)
                ++terminal;
        }
        // Do not require equality: stock one-mesh props can carry an animation
        // block but no terminal link. Only the proven excess-terminal case fails.
        if (terminal > objects)
            throw Bad(name, $"{terminal} terminal LODs for {objects} animation bones; refusing an unsafe animation stride");
        // SM1 0x2A writes 24 bytes per terminal into 30 fixed slots. Packed 0x2C
        // uses another path (retail Doc Ock has 46 bones), so it MUST NOT be capped.
        if (maxUnpackedAnimationBones > 0 && unpacked && terminal > maxUnpackedAnimationBones)
            throw Bad(name, $"{terminal} unpacked animation bones exceed the game profile capacity {maxUnpackedAnimationBones}");
        return new(objects, count, terminal, true);
    }

    static uint U32(ReadOnlySpan<byte> data, long offset, string name)
    {
        Need(data, offset, 4, name);
        return BinaryPrimitives.ReadUInt32LittleEndian(data.Slice((int)offset, 4));
    }

    static void Need(ReadOnlySpan<byte> data, long offset, long size, string name)
    {
        if (offset < 0 || size < 0 || offset > data.Length || size > data.Length - offset)
            throw Bad(name, $"native actor record is out of bounds ({offset}+{size}/{data.Length})");
    }

    static InvalidDataException Bad(string name, string problem) => new($"{name}: {problem}");
}
