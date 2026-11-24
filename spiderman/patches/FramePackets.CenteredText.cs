using System.Collections.Generic;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

public static partial class FramePackets
{
    static uint _pool, _promptStart;
    // The two native draw buffers can be built and submitted on different frames.
    static readonly Dictionary<uint, (uint Start, uint End)> PromptByPool = new(2);

    public static void BeginQuitPrompt(CpuContext c, IMemory m) =>
        _promptStart = m.ReadU32(PacketCursor) & 0x7FFFFF;

    public static void EndQuitPrompt(CpuContext c, IMemory m)
    {
        uint end = m.ReadU32(PacketCursor) & 0x7FFFFF;
        if (_promptStart != 0 && end > _promptStart) PromptByPool[_pool] = (_promptStart, end);
        _promptStart = 0;
    }

    public static bool IsQuitPromptCommand(uint address)
    {
        foreach (var range in PromptByPool.Values)
            if (address >= range.Start && address < range.End) return true;
        return false;
    }

    static void ReusePacketPool(uint pool)
    {
        _pool = pool & 0x7FFFFF;
        _promptStart = 0;
        PromptByPool.Remove(_pool);
    }
}
