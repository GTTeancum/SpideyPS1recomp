using System;
using RecompOne.Runtime.Assets;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Repairs the animation-part count in older converted actors whose extra LOD
/// meshes were incorrectly marked as additional skeleton parts.
/// </summary>
public static class Sm1AnimationBounds
{
    const uint ModelTable = 0x800A0904u;
    const uint ModelSlots = 40;
    const uint ModelStride = 64;
    public const uint AnimationScratch = 0x800A58E4u;
    public const uint CollisionGrid = 0x800A5BB4u;
    public const uint BytesPerPart = 24;
    public const uint MaxParts = (CollisionGrid - AnimationScratch) / BytesPerPart;

    /// <summary>
    /// Post-hook for func_80068BB0. It returns the loaded model-table entry in V0,
    /// after func_80074C98 has counted terminal meshes and the metadata walk has
    /// installed the animation-bank pointer. No guest register is changed here.
    /// </summary>
    public static void AfterModelLoad(CpuContext c, IMemory m)
    {
        uint entry = c.V0;
        if (entry < ModelTable || entry >= ModelTable + ModelSlots * ModelStride ||
            (entry - ModelTable) % ModelStride != 0)
            return;

        uint model = m.ReadU32(entry + 20u);
        if (!LooseWadOverrides.TryGetAllocationSize(model, out uint allocationSize) ||
            allocationSize < 12u)
            return;

        uint parts = m.ReadU32(model + 8u);
        ushort cachedParts = m.ReadU16(entry + 52u);
        // Only a duplicate-LOD overcount is repaired. Matching counts, retail
        // heap models, and formats not proved below retain their native behavior.
        if (cachedParts <= parts || parts == 0 || parts > MaxParts ||
            m.ReadU16(model) != 4u || m.ReadU16(model + 2u) != 2u)
            return;

        uint bank = m.ReadU32(entry + 24u);
        if (!TryFindRawBank(m, model, allocationSize, bank, out uint bankLength) ||
            !MatchesPartCount(m, bank, bankLength, parts))
            return;

        m.WriteU16(entry + 52u, (ushort)parts);
        Console.WriteLine($"[dc-model] corrected animation parts in slot {(entry - ModelTable) / ModelStride}: " +
            $"{cachedParts} -> {parts} (validated raw animation bank)");
    }

    static bool TryFindRawBank(IMemory m, uint model, uint allocationSize, uint bank, out uint length)
    {
        length = 0;
        uint offset = m.ReadU32(model + 4u);
        if (offset < 12u || offset > allocationSize - 4u)
            return false;

        // Walk the model's actual metadata chain. A plausible tag immediately
        // before an unrelated pointer is insufficient evidence to repair it.
        while (offset <= allocationSize - 4u)
        {
            uint tag = m.ReadU32(model + offset);
            if (tag == uint.MaxValue || allocationSize - offset < 8u)
                return false;
            uint chunkLength = m.ReadU32(model + offset + 4u);
            if (chunkLength > allocationSize - offset - 8u)
                return false;
            uint payload = model + offset + 8u;
            if (payload == bank)
            {
                if (tag != 0x2Au)
                    return false;
                length = chunkLength;
                return true;
            }
            offset += 8u + chunkLength;
        }
        return false;
    }

    static bool MatchesPartCount(IMemory m, uint bank, uint length, uint parts)
    {
        if (length < 12u)
            return false;
        uint clips = m.ReadU32(bank);
        if (clips == 0 || clips > (length - 4u) / 8u)
            return false;
        uint tableEnd = 4u + clips * 8u;
        if (m.ReadU32(bank + 4u) != tableEnd)
            return false;

        for (uint clip = 0; clip < clips; clip++)
        {
            uint record = bank + 4u + clip * 8u;
            uint start = m.ReadU32(record);
            uint end = clip + 1u < clips ? m.ReadU32(record + 8u) : length;
            uint frames = m.ReadU16(record + 4u);
            uint stride = (uint)m.ReadU16(record + 6u) + 1u;
            if (start < tableEnd || end < start || end > length || frames == 0)
                return false;

            // Tag 0x2A stores one 24-byte transform per part per sampled frame.
            // Requiring every clip, including the final one, to fit exactly
            // rejects compressed, padded, truncated, or mixed-layout banks.
            uint samples = (frames + stride - 1u) / stride;
            ulong expected = (ulong)samples * parts * BytesPerPart;
            if ((ulong)(end - start) != expected)
                return false;
        }
        return true;
    }
}
