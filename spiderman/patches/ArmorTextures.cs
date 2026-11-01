using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Issue #10: Spider-Armor crashed with converted and custom player models.
///
/// Turning armor on or off (func_80046EEC) walks every registered player texture,
/// saves its pixels and palette into fixed 8192- and 1024-byte buffers, then swaps
/// them with the armor version in sparmour.psx, found through a 13-pair table of retail
/// texture hashes (0x80099658). Each armor record holds exactly one retail-sized page,
/// and the swap writes the player's page back into that record so armor-off can restore
/// it. Two things go wrong for converted actors (the port's default Spider-Man among
/// them):
///   - a hash missing from the table leaves the swap's destination pointers unset, and
///     the routine copies the saved texture to wherever they point, so the game crashed
///     a frame later while drawing the player;
///   - a texture that keeps a retail hash but is larger, or of another colour depth,
///     overruns its armor record into the next one, and the next lookup reads garbage.
///
/// Instruction hook at the top of the per-texture loop (0x80046FD4): a texture is only
/// swapped when its armor record exists and has the same depth and page size. Anything
/// else is skipped and keeps its own look. Retail textures match exactly and swap as
/// before, in both directions. Armor protection is set elsewhere (func_800489D4) and is
/// unaffected.
/// </summary>
public static class ArmorTextures
{
    const uint MappingTable = 0x80099658u;
    const int MappingPairs = 13;

    public static bool SkipUnmapped(CpuContext c, IMemory m)
    {
        uint descriptor = c.S0;
        uint hash = m.ReadU32(descriptor + 0x14u);
        uint armorHash = 0;
        for (uint i = 0; i < MappingPairs && armorHash == 0; i++)
            if (m.ReadU32(MappingTable + i * 8u) == hash) armorHash = m.ReadU32(MappingTable + i * 8u + 4u);

        // Same width conversion the routine applies before StoreImage: 4-bit pages
        // hold four texels per 16-bit word, 8-bit pages two.
        bool fourBit = (m.ReadU16(descriptor + 0x6u) >> 7 & 3u) == 0;
        int width = (short)m.ReadU16(c.S2);
        int words = fourBit ? (width + 3) >> 2 : (width + 1) >> 1;
        int bytes = words * 2 * (short)m.ReadU16(c.S4);

        string reason;
        if (armorHash == 0) reason = "no armor version";
        else if (!FindRecord(c, m, armorHash, out bool armorFourBit, out int armorBytes)) reason = "armor record missing";
        else if (armorFourBit != fourBit || armorBytes != bytes) reason = $"{bytes} bytes, armor page {armorBytes}";
        else return false;

        Console.WriteLine($"[armor] texture {hash:X8} kept ({reason})");
        return true; // resume at 0x800471B0, the next registered texture
    }

    // Walks the armor records the way the lookup (func_80046980) does. The routine keeps
    // the bank's hash list at sp+0x28 and its record section at sp+0x2C: a count, an
    // offset table, then records of { ?, colours, hash, hash index, h<<16|w, pixels }.
    static bool FindRecord(CpuContext c, IMemory m, uint armorHash, out bool fourBit, out int bytes)
    {
        fourBit = false; bytes = 0;
        uint hashes = m.ReadU32(c.SP + 0x28u);
        uint section = m.ReadU32(c.SP + 0x2Cu);
        int count = (int)m.ReadU32(section);
        if (count <= 0 || count > 64) return false;
        uint record = section + (uint)count * 4u + 4u;
        for (uint i = 0; i < count; i++)
        {
            if (m.ReadU32(hashes + m.ReadU32(record + 0xCu) * 4u + 4u) == armorHash)
            {
                uint size = m.ReadU32(record + 0x10u);
                int texels = (int)(size & 0xFFFF) * (int)(size >> 16);
                fourBit = m.ReadU32(record + 0x4u) == 0x10u;
                bytes = fourBit ? texels / 2 : texels;
                return true;
            }
            record += m.ReadU32(section + 8u + i * 4u) - m.ReadU32(section + 4u + i * 4u);
        }
        return false;
    }
}
