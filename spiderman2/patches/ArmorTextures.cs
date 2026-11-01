using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Issue #10: Spider-Armor crashed with converted and custom player models.
///
/// Turning armor on or off (func_8004E848) walks every registered player texture,
/// saves its pixels and palette into fixed 8192- and 1024-byte buffers, then swaps
/// them with the armor version found through a 14-pair table of retail texture hashes
/// (0x800A5F38). A hash missing from that table leaves the swap's two destination
/// pointers unset, and the routine copies the saved texture to wherever they point.
/// Converted actors use other hashes and can have pages larger than the buffer.
///
/// Instruction hook at the top of the per-texture loop (0x8004E930): a texture with no
/// armor mapping, or one too large for the retail buffers, is skipped and keeps its
/// own look. Retail textures that do have a mapping swap exactly as before.
/// Armor protection is set elsewhere (func_80050494) and is unaffected.
/// </summary>
public static class ArmorTextures
{
    const uint MappingTable = 0x800A5F38u;
    const int MappingPairs = 14;
    const int PixelBuffer = 0x2000;

    public static bool SkipUnmapped(CpuContext c, IMemory m)
    {
        uint descriptor = c.S0;
        uint hash = m.ReadU32(descriptor + 0x14u);
        bool mapped = false;
        for (uint i = 0; i < MappingPairs; i++)
            if (m.ReadU32(MappingTable + i * 8u) == hash) { mapped = true; break; }

        // Same width conversion the routine applies before StoreImage: 4-bit pages
        // hold four texels per 16-bit word, 8-bit pages two.
        bool fourBit = (m.ReadU16(descriptor + 0x6u) >> 7 & 3u) == 0;
        int width = (short)m.ReadU16(c.S2);
        int words = fourBit ? (width + 3) >> 2 : (width + 1) >> 1;
        int bytes = words * 2 * (short)m.ReadU16(c.S4);
        if (mapped && bytes > 0 && bytes <= PixelBuffer) return false;

        Console.WriteLine($"[armor] texture {hash:X8} kept ({(mapped ? $"{bytes} bytes exceeds the armor buffer" : "no armor version")})");
        return true; // resume at 0x8004EB0C, the next registered texture
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
/// variable is set. Only meaningful in a level that has loaded the armor texture bank
/// (sparmour.psx): E1M0 does not, and switching armor off there reads garbage even
/// for the stock suit.
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
        c.A0 = player;
        c.A1 = on;
        Console.WriteLine($"[armor-test] frame {Diag.Frame}: armor {(on == 1 ? "on" : "off")} for player 0x{player:X8}");
        SpiderMan2.func_80050494(c, m);
        Console.WriteLine($"[armor-test] returned; player+0x640 = {m.ReadU32(player + 0x640)}");
        c.Restore(saved);
    }
}
