using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

public static class MenuPacketOrder
{
    // SM2's func_80075438 has the same single-DR_AREA alias as SM1's menu pass.
    public static bool SingleMetadataTail(CpuContext c, IMemory m)
    {
        const uint mask = 0xFFFFFF;
        uint anchor = c.T0 & mask, tail = c.T2 & mask;
        if (tail != (c.A3 & mask) || tail == anchor ||
            tail > 0x7FFFF4 || anchor > 0x7FFFFC || ((tail | anchor) & 3) != 0)
            return false;
        uint header = m.ReadU32(tail);
        if (header != 0x02FFFFFF || m.ReadU32(tail + 4) >> 24 != 0xE3 ||
            m.ReadU32(tail + 8) >> 24 != 0xE4)
            return false;

        // Retain clipping, backdrop insertion, and the foreground anchor once each.
        m.WriteU32(anchor, (m.ReadU32(anchor) & 0xFF000000) | mask);
        m.WriteU32(tail, (header & 0xFF000000) | anchor);
        uint rampAddress = c.GP + 0x1010;
        uint ramp = unchecked(m.ReadU32(rampAddress) + 0x60);
        m.WriteU32(rampAddress, (int)ramp > 0x300 ? 0x300u : ramp);
        return true;
    }
}
