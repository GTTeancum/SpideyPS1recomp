using System.Collections.Generic;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

public static partial class FramePackets
{
    static uint _pool;
    static readonly PacketRange QuitPrompt = new();

    // A native HUD draw can be submitted after the other buffer has started.
    sealed class PacketRange
    {
        readonly Dictionary<uint, (uint Start, uint End)> _byPool = new(2);
        uint _start;

        public void Begin(IMemory m) => _start = m.ReadU32(PacketCursor) & 0x7FFFFF;

        public void End(IMemory m)
        {
            uint end = m.ReadU32(PacketCursor) & 0x7FFFFF;
            if (_start != 0 && end > _start) _byPool[_pool] = (_start, end);
            _start = 0;
        }

        public bool Contains(uint address)
        {
            foreach (var range in _byPool.Values)
                if (address >= range.Start && address < range.End) return true;
            return false;
        }

        public void Reuse(uint pool)
        {
            _start = 0;
            _byPool.Remove(pool);
        }
    }

    public static void BeginQuitPrompt(CpuContext c, IMemory m) => QuitPrompt.Begin(m);
    public static void EndQuitPrompt(CpuContext c, IMemory m) => QuitPrompt.End(m);
    public static bool IsQuitPromptCommand(uint address) => QuitPrompt.Contains(address);

    static void ReusePacketPool(uint pool)
    {
        _pool = pool & 0x7FFFFF;
        QuitPrompt.Reuse(_pool);
    }
}
