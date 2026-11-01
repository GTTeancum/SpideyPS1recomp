using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Issue #10: Spider-Armor crashed with converted and custom player models.
///
/// Turning armor on or off (func_80046EEC) walks every registered player texture,
/// saves its pixels and palette into fixed 8192- and 1024-byte buffers, then swaps
/// them with the armor version found through a 13-pair table of retail texture hashes
/// (0x80099658). A hash missing from that table leaves the swap's two destination
/// pointers unset, and the routine copies the saved texture to wherever they point.
/// Converted actors (the port's default Spider-Man among them) use other hashes and
/// can have pages larger than the 8192-byte buffer, so armor corrupted memory and the
/// game crashed a frame later while drawing the player.
///
/// Instruction hook at the top of the per-texture loop (0x80046FD4): a texture with no
/// armor mapping, or one too large for the retail buffers, is skipped and keeps its
/// own look. Retail textures that do have a mapping swap exactly as before, in both
/// directions. Armor protection is set elsewhere (func_800489D4) and is unaffected.
/// </summary>
public static class ArmorTextures
{
    const uint MappingTable = 0x80099658u;
    const int MappingPairs = 13;
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
        return true; // resume at 0x800471B0, the next registered texture
    }
}
