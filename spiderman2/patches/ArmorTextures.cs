using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Issue #10: Spider-Armor crashed with converted and custom player models.
///
/// Turning armor on or off (func_8004E848) walks every registered player texture,
/// saves its pixels and palette into fixed 8192- and 1024-byte buffers, then swaps
/// them with the armor version in sparmour.psx, found through a 14-pair table of retail
/// texture hashes (0x800A5F38). Each armor record holds exactly one retail-sized page,
/// and the swap writes the player's page back into that record so armor-off can restore
/// it. Two things go wrong for converted actors:
///   - a hash missing from the table leaves the swap's destination pointers unset, and
///     the routine copies the saved texture to wherever they point;
///   - a texture that keeps a retail hash but is larger, or of another colour depth,
///     overruns its armor record into the next one, and the next lookup reads garbage
///     (unmapped address in func_8004E2FC). The port's default Spider-Man does this.
///
/// Suit mods never take the armor art: their pages are drawn from the mod's own images,
/// so a retail armor page swapped underneath would only show on whatever pieces happen to
/// match retail sizes (Captain Spider's cape and head). Every texture is skipped while a
/// suit mod is active; armor protection still applies.
///
/// Instruction hook at the top of the per-texture loop (0x8004E930): a texture is only
/// swapped when its armor record exists and has the same depth and page size. Anything
/// else is skipped and keeps its own look. Retail textures match exactly and swap as
/// before. Armor protection is set elsewhere (func_80050494) and is unaffected.
/// </summary>
public static class ArmorTextures
{
    const uint MappingTable = 0x800A5F38u;
    const int MappingPairs = 14;

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
        if (SuitMods.Active >= 0) reason = "suit mod keeps its own look";
        else if (armorHash == 0) reason = "no armor version";
        else if (!FindRecord(c, m, armorHash, out bool armorFourBit, out int armorBytes)) reason = "armor record missing";
        else if (armorFourBit != fourBit || armorBytes != bytes) reason = $"{bytes} bytes, armor page {armorBytes}";
        else return false;

        Console.WriteLine($"[armor] texture {hash:X8} kept ({reason})");
        return true; // resume at 0x8004EB0C, the next registered texture
    }

    // Walks the armor records the way the lookup (func_8004E2FC) does. The routine keeps
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

/// <summary>
/// Test switch for issue #10: switch Spider-Armor on or off without reaching a pickup.
///
///     SPIDEY_TEST_ARMOR=4600          armor on at frame 4600
///     SPIDEY_TEST_ARMOR=4600,5200     on at 4600, off at 5200
///
/// Pre-hook on the HUD draw (func_80072B88), which reads the player from 0x800C21F4;
/// calls the pickup's own routine, func_80050494(player, on/off). Off unless the
/// variable is set. The armor texture bank (sparmour.psx) is only loaded at the start
/// of a level with an armor pickup or carried-over armor (func_8004E268, from
/// func_80064C2C): E3M0, E5M5 and E5M6. The switch loads it the same way first if the
/// level has not.
/// </summary>
public static class ArmorTest
{
    static readonly long[] Frames = Parse(Environment.GetEnvironmentVariable("SPIDEY_TEST_ARMOR"));
    static int _next;

    static long[] Parse(string spec)
    {
        if (string.IsNullOrWhiteSpace(spec)) return Array.Empty<long>();
        return Array.ConvertAll(spec.Split(','), s => long.Parse(s.Trim()));
    }

    public static void HudDraw(CpuContext c, IMemory m)
    {
        if (_next >= Frames.Length || Diag.Frame < Frames[_next]) return;
        uint player = m.ReadU32(0x800C21F4u);
        if ((player & 0xFF000000u) != 0x80000000u) return;
        uint on = _next % 2 == 0 ? 1u : 0u;
        _next++;
        var saved = c.Snapshot();
        if (m.ReadU32(c.GP + 0xAA4u) == 0)
        {
            Console.WriteLine("[armor-test] loading the armor texture bank");
            SpiderMan2.func_8004E268(c, m);
        }
        c.A0 = player;
        c.A1 = on;
        Console.WriteLine($"[armor-test] frame {Diag.Frame}: armor {(on == 1 ? "on" : "off")} for player 0x{player:X8}");
        SpiderMan2.func_80050494(c, m);
        Console.WriteLine($"[armor-test] returned; player+0x640 = {m.ReadU32(player + 0x640)}");
        c.Restore(saved);
    }
}
