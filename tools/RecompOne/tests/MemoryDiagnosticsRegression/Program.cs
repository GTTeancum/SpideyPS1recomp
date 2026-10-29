using System.Reflection;
using RecompOne.Runtime;
using RecompOne.Runtime.Host;
using RecompOne.Runtime.Memory;

// These tests exercise real guest writes and diagnostic lifecycles without a
// window or game assets. Reflection checks backing storage because allocation
// removal, rather than a process-wide working-set estimate, is the requirement.
int failures = 0;
void Check(bool pass, string name)
{
    Console.WriteLine($"{(pass ? "PASS" : "FAIL")}: {name}");
    if (!pass) failures++;
}

object? Field(object instance, string name) => instance.GetType()
    .GetField(name, BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(instance);
long StampBytes(RamLogger logger) => new[] { "_readTimestamps", "_writeTimestamps" }
    .Sum(name => Field(logger, name) is uint[] data ? (long)data.Length * sizeof(uint) : 0L);

var log = new RamLogger();
Check(!log.Enabled && StampBytes(log) == 0, "access history has no backing storage before use");
log.RecordWrite(12, 4);
log.RecordRead(12, 4);
Check(log.HeatAt(12) == 0 && log.ReadHeatAt(12) == 0 && StampBytes(log) == 0,
    "disabled accesses neither allocate nor leave stale heat");

log.Enabled = true;
Check(StampBytes(log) == 16L * 1024 * 1024, "visible diagnostics allocate 16 MiB of access history");
log.RecordWrite(12, 4);
log.RecordRead(13, 2);
Check(log.HeatAt(12) == 1f && log.ReadHeatAt(13) == 1f && log.GetWriteStamp(16) == 0,
    "first-frame accesses and byte widths are recorded");
log.Enabled = true;
Check(log.HeatAt(12) == 1f, "repeated enable preserves history");
for (int frame = 0; frame < 90; frame++) log.Tick();
Check(MathF.Abs(log.HeatAt(12) - 0.5f) < 0.0001f, "heat decays over the configured half-life");
log.RecordWrite(0x1FFFFF, 2);
Check(log.GetWriteStamp(0x1FFFFF) == log.Cycle && log.GetWriteStamp(0) == log.Cycle,
    "access history retains the retail RAM mirror boundary");
Check(log.GetReadStamp(-1) == 0 && log.GetWriteStamp(0x200000) == 0,
    "out-of-range diagnostic queries are harmless");

log.Enabled = false;
Check(StampBytes(log) == 0 && log.HeatAt(12) == 0, "closing diagnostics releases access history");
var pixels = new byte[RamLogger.Width * RamLogger.Height * 4];
log.BuildTexture(ReadOnlySpan<byte>.Empty, pixels);
Check(pixels[0] == 63 && pixels[1] == 38 && pixels[2] == 38 && pixels[3] == 255,
    "queued texture conversion remains safe after logging is disabled");
log.Enabled = true;
Check(log.HeatAt(12) == 0 && log.ReadHeatAt(13) == 0, "reopening starts with fresh history");
log.RecordWrite(0, 1);
log.BuildTexture(ReadOnlySpan<byte>.Empty, pixels);
Check(pixels[0] > pixels[4] && pixels[1] < pixels[5] && pixels[3] == 255,
    "enabled texture conversion still displays write heat");
log.Enabled = false;

Runtime.RamLog.Enabled = false;
var memory = new PSMemory(0x800000);
const uint address = 0x80000100;
Check(Field(memory, "_frozen") == null, "8 MiB guest RAM starts without a freeze mask");
memory.Freeze(0, 0);
memory.Freeze(0, -1);
memory.Unfreeze(0, 10);
Check(Field(memory, "_frozen") == null, "empty and unused freeze operations do not allocate");
memory.WriteU32(address, 0x44332211);
Check(memory.ReadU32(0xA0000100) == 0x44332211 && StampBytes(Runtime.RamLog) == 0,
    "ordinary cached/uncached guest reads and writes work without diagnostics");
memory.TagGteDepth(address, 0x44332211, 123f);
Check(memory.TryGetGteDepth(address, 0x44332211, out float depth) && depth == 123f,
    "projection depth tags still attach while diagnostics are disabled");
memory.WriteU8(address, 0x22);
Check(!memory.TryGetGteDepth(address, 0x44332211, out _),
    "ordinary writes still invalidate projection depth tags");
memory.WriteU32(address, 0x44332211);

memory.Freeze(0x101, 2);
memory.Freeze(0x101, 2);
Check(Field(memory, "_frozen") is bool[] { Length: 0x800000 }, "first freeze lazily creates the mask");
memory.WriteU32(address, 0xDDCCBBAA);
Check(memory.ReadU32(address) == 0xDD3322AA, "word writes preserve only the frozen bytes");
memory.WriteU8(address + 1, 0xFF);
memory.WriteU16(address, 0xFFFF);
Check(memory.ReadU32(address) == 0xDD3322FF, "byte and halfword writes honor partial freezes");
memory.Unfreeze(0xA0000101, 1);
Check(!memory.IsFrozen(0x101) && memory.IsFrozen(0x102), "unfreeze uses guest address aliases");
memory.LoadBytes(address, [1, 2, 3, 4]);
Check(memory.ReadU32(address) == 0x04330201, "bulk loads preserve frozen bytes");
memory.ZeroRange(address, 4);
Check(memory.ReadU32(address) == 0x00330000, "zeroing preserves frozen bytes");
memory.Unfreeze(0x102, 1);
Check(Field(memory, "_frozen") == null, "last unfreeze releases mask despite duplicate freezes");
memory.WriteU32(address, 0x12345678);
Check(memory.ReadU32(address) == 0x12345678, "writes resume after the last unfreeze");

memory.WriteU8(0x807FFFFF, 0x12);
memory.WriteU8(0, 0x34);
memory.Freeze(0x7FFFFF, 2);
memory.WriteU8(0xA07FFFFF, 0xFE);
memory.WriteU8(0x80000000, 0xDC);
Check(memory.ReadU8(0x7FFFFF) == 0x12 && memory.ReadU8(0) == 0x34,
    "freeze ranges wrap over the actual guest RAM boundary");
memory.Poke(0, 0x56);
Check(memory.ReadU8(0) == 0x56, "debugger poke retains its explicit freeze override");
memory.ClearFreezes();
Check(!memory.IsFrozen(0) && !memory.IsFrozen(0x7FFFFF) && Field(memory, "_frozen") == null,
    "clear releases all freezes and backing storage");
memory.Freeze(0x103, 1);
Check(!memory.IsFrozen(0x102) && memory.IsFrozen(0x103), "new freeze mask contains no old freezes");
memory.ClearFreezes();

Runtime.RamLog.Enabled = true;
memory.WriteU16(address, 0xABCD);
memory.ReadU16(address);
Check(Runtime.RamLog.GetWriteStamp(0x100) != 0 && Runtime.RamLog.GetReadStamp(0x101) != 0,
    "memory editor can re-enable read and write tracking");
Runtime.RamLog.Enabled = false;

var hostType = typeof(HostWindow);
foreach (string name in new[] { "_ramFront", "_ramBack" })
{
    var buffer = (byte[])hostType.GetField(name, BindingFlags.NonPublic | BindingFlags.Static)!.GetValue(null)!;
    Check(buffer.Length == 0, $"host {name} does not reserve an unused 8 MiB image");
}
Console.WriteLine($"Memory diagnostics: {failures} failure(s). " +
    "Ordinary 8 MiB guest configuration avoids 40 MiB of diagnostic array payload.");
return failures == 0 ? 0 : 1;
