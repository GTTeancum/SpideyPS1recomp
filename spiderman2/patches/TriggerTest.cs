using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Test switch: run one of the level's trigger-script records on a chosen frame.
///
///     SPIDEY_TEST_TRIGGER=5600:113     at frame 5600 run record 113 of the loaded TRG
///
/// Issue #11 needs the ordinary E1M3 → E1M4 hand-off, which normally takes defeating
/// fifteen thugs. E1M3's record 113 is the exit script the last defeat starts (it ends
/// by naming E1M4_T); running it here goes through the game's own script interpreter
/// (func_80066D74, called as func_80064E44 does: record + 2, record index, 1), so the
/// fade, level-complete status and next-level load are all native.
///
/// Pre-hook on the per-frame script runner (func_80019848). The record table is the
/// relocated pointer array at gp+0xBFC with gp+0xC00 entries. Off unless the variable
/// is set.
/// </summary>
public static class TriggerTest
{
    static readonly (long Frame, uint Record)[] Steps = Parse(Environment.GetEnvironmentVariable("SPIDEY_TEST_TRIGGER"));
    static int _next;

    static (long, uint)[] Parse(string spec)
    {
        if (string.IsNullOrWhiteSpace(spec)) return Array.Empty<(long, uint)>();
        return Array.ConvertAll(spec.Split(','), s =>
        {
            var p = s.Split(':');
            return (long.Parse(p[0].Trim()), uint.Parse(p[1].Trim()));
        });
    }

    public static void ScriptRunner(CpuContext c, IMemory m)
    {
        if (_next >= Steps.Length || Diag.Frame < Steps[_next].Frame) return;
        uint index = Steps[_next++].Record;
        uint table = m.ReadU32(c.GP + 0xBFCu);
        uint count = m.ReadU32(c.GP + 0xC00u);
        if ((table & 0xFF000000u) != 0x80000000u || index >= count)
        {
            Console.WriteLine($"[trigger-test] frame {Diag.Frame}: record {index} not available ({count} records)");
            return;
        }
        uint record = m.ReadU32(table + index * 4u);
        Console.WriteLine($"[trigger-test] frame {Diag.Frame}: running record {index} (type {m.ReadU16(record)}) at 0x{record:X8}");
        var saved = c.Snapshot();
        c.A0 = record + 2u;
        c.A1 = index;
        c.A2 = 1u;
        SpiderMan2.func_80066D74(c, m);
        c.Restore(saved);
    }
}
