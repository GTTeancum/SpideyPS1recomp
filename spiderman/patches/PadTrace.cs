using System;
using System.Buffers.Binary;
using System.Text.Json;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;

namespace Recompiled;

// Read-only post-hook on the native pad decoder. Raw RAM avoids idle-breaker
// side effects; normal runs pay only the disabled branch. Diagnostic logs use
// the existing background console sink and are bounded to 4096 records.
public static class PadTrace
{
    static readonly bool Enabled = Environment.GetEnvironmentVariable("SPIDEY_TRACE_PAD") == "1";
    static int _records;
    public static void AfterRead(CpuContext c, IMemory m)
    {
        if (!Enabled || _records >= 4096 || m is not PSMemory ps) return;
        var ram = ps.Ram;
        uint player = BinaryPrimitives.ReadUInt32LittleEndian(ram.Slice(0xB5268, 4));
        uint held = 0, pressed = 0;
        for (int i = 0; i < 16; i++)
        {
            if (ram[0xA4DF4 + i * 16] != 0) held |= 1u << i;
            if (ram[0xA4DF5 + i * 16] != 0) pressed |= 1u << i;
        }
        int x = 0, y = 0, z = 0, yaw = 0;
        if (player >= 0x80000000 && player <= 0x801FFFE0)
        {
            int at = (int)(player - 0x80000000);
            x = BinaryPrimitives.ReadInt32LittleEndian(ram.Slice(at + 4, 4));
            y = BinaryPrimitives.ReadInt32LittleEndian(ram.Slice(at + 8, 4));
            z = BinaryPrimitives.ReadInt32LittleEndian(ram.Slice(at + 12, 4));
            yaw = BinaryPrimitives.ReadInt16LittleEndian(ram.Slice(at + 18, 2));
        }
        Console.WriteLine("[pad-trace] " + JsonSerializer.Serialize(new {
            record = ++_records, tick = Diag.Frame,
            gameCounter = BinaryPrimitives.ReadUInt32LittleEndian(ram.Slice(0xB4F38, 4)),
            irqCounter = BinaryPrimitives.ReadUInt32LittleEndian(ram.Slice(0xB5468, 4)),
            padClock = BinaryPrimitives.ReadUInt32LittleEndian(ram.Slice(0xB546C, 4)),
            script = Controller.ScriptHeld, controller = Controller.State,
            held, pressed, player, x, y, z, yaw, caller = c.RA
        }));
        if (_records == 4096) Console.WriteLine("[pad-trace] record limit reached");
    }
}
