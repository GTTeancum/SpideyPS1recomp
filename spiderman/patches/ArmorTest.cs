using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Test switch for issue #10: switch Spider-Armor on or off without reaching a pickup.
///
///     SPIDEY_TEST_ARMOR=4600          armor on at frame 4600
///     SPIDEY_TEST_ARMOR=4600,5200     on at 4600, off at 5200
///
/// Calls the same routine the pickup does (func_800489D4: player, on/off), from the
/// post-hook on the per-frame pad read, so the armor texture swap runs on the real
/// player with whatever costume or suit mod is selected. Off unless the variable is set.
/// </summary>
public static class ArmorTest
{
    static readonly long[] Frames = Parse(Environment.GetEnvironmentVariable("SPIDEY_TEST_ARMOR"));
    static int _next;
    public static uint Player;

    static long[] Parse(string spec)
    {
        if (string.IsNullOrWhiteSpace(spec)) return Array.Empty<long>();
        return Array.ConvertAll(spec.Split(','), s => long.Parse(s.Trim()));
    }

    public static void AfterPadRead(CpuContext c, IMemory m)
    {
        if (_next >= Frames.Length || Diag.Frame < Frames[_next] || Player == 0) return;
        uint on = _next % 2 == 0 ? 1u : 0u;
        _next++;
        var saved = c.Snapshot();
        c.A0 = Player;
        c.A1 = on;
        Console.WriteLine($"[armor-test] frame {Diag.Frame}: armor {(on == 1 ? "on" : "off")} for player 0x{Player:X8}");
        SpiderMan.func_800489D4(c, m);
        Console.WriteLine($"[armor-test] returned; player+0x5F0 = {m.ReadU32(Player + 0x5F0)}, gp+0xA7C = {m.ReadU32(saved.gpr[28] + 0xA7C)}");
        c.Restore(saved);
    }
}
