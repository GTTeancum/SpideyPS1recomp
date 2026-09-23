using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;
using RecompOne.Runtime.Sdk;

namespace Recompiled;

public static class Timing
{
    static readonly bool Trace = System.Environment.GetEnvironmentVariable("SPIDEY_TRACE_TIMING") == "1";
    static bool _chaseLevel;
    static uint _chasePlayer;

    // LoadTriggers pre-hook. A reload/retry must not inherit another scene's rate.
    public static void LoadTriggers(CpuContext c, IMemory m)
    {
        _chasePlayer = 0;
        const string name = "l5a1_t";
        _chaseLevel = c.A0 != 0;
        for (int i = 0; _chaseLevel && i <= name.Length; i++)
            _chaseLevel = m.ReadU8(c.A0 + (uint)i) == (i == name.Length ? 0 : name[i]);
    }

    // Native player-script initializer (80049D54): A0=player, A1=script.
    // Only the authored through-building Chase Venom sequence needs a
    // 20-update cadence. Its movement commands are not invariant to update size.
    // Bind to the actual script start, independent of the optional proof harness.
    public static void StartPlayerScript(CpuContext c, IMemory m)
    {
        _chasePlayer = 0;
        if (!_chaseLevel || c.A0 < 0x80000000u || c.A0 >= 0x80200000u ||
            c.A1 < 0x80000000u || c.A1 > 0x801FFFE6u) return;
        // Includes the initial (delay=0, command=17, argument=30) animation.
        System.ReadOnlySpan<ushort> prefix = [0, 17, 30, 3, 10, 60, 520, 1, 54, 20, 3, 16, 2];
        for (int i = 0; i < prefix.Length; i++)
            if (m.ReadU16(c.A1 + (uint)i * 2) != prefix[i]) return;
        _chasePlayer = c.A0;
        if (Trace) System.Console.WriteLine("[timing] Chase building cadence selected");
    }

    // Retail 8005E748 waits on the counter incremented by the game's vblank IRQ.
    public static void WaitVBlanks(CpuContext c, IMemory m)
    {
        // The main loop calls this twice with A0=1. Extend only its second
        // wait during this script, advancing three genuine IRQ edges per update.
        // Presentation remains capped at 30 FPS and the console clock stays 60 Hz.
        if (_chasePlayer != 0 && c.RA == 0x8002C294u && c.A0 == 1)
        {
            uint player = m.ReadU32(0x800B5268u);
            if (player == _chasePlayer && m.ReadU32(player + 0x1A8u) != 0)
                c.A0 = 2;
            else
            {
                if (Trace) System.Console.WriteLine($"[timing] normal cadence restored player={player:X8} bound={_chasePlayer:X8}");
                _chasePlayer = 0;
            }
        }
        LibEtc.WaitCounter(c, m, c.GP + 0xC74u);
    }
}
