using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text.Json;
using Recompiled;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;

// No window, sound device, Runtime.Run, controller polling, or host input.
string root = Path.GetFullPath(args[0]);
string output = Path.GetFullPath(args[1]);
if (!output.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw new Exception("Output must stay in repository");
Directory.CreateDirectory(output);
int checks = 0;
void Check(bool ok, string name) { if (!ok) throw new Exception(name); checks++; Console.WriteLine("PASS: " + name); }
var cpu = new CpuContext { GP = 0x800B47F4, SP = 0x807EF000 };
PSMemory Memory()
{
    var memory = new PSMemory(0x800000);
    byte[] exe = File.ReadAllBytes(Path.Combine(root, "spiderman/extracted/SLUS_008.75"));
    memory.LoadBytes(BinaryPrimitives.ReadUInt32LittleEndian(exe.AsSpan(0x18)), exe.AsSpan(2048).ToArray());
    return memory;
}
var m = Memory();
const uint head = 0x80409E58, vertices = head + 0x1C, backup = 0x80500000, table = 0x80300000;
byte[] model = File.ReadAllBytes(Path.Combine(root, "Spider-Man/assets/builtin/spidey.psx"));
int meshTable = 12 + BitConverter.ToInt32(model, 8) * 36;
int headOffset = BitConverter.ToInt32(model, meshTable + 4 + 7 * 4);
ushort count = BitConverter.ToUInt16(model, headOffset + 2);
Check(count == 124, "regression uses actual 124-vertex head from staged model");
byte[] good = model.AsSpan(headOffset + 0x1C, count * 8).ToArray();
// The native loader converts shared-vertex Y indices to byte offsets in XY.
for (int i = 0; i < count; i++)
    if ((BitConverter.ToUInt16(good, i * 8 + 6) & 2) != 0)
        BinaryPrimitives.WriteUInt32LittleEndian(good.AsSpan(i * 8), (uint)BitConverter.ToUInt16(good, i * 8 + 2) * 8);
m.WriteU8(0x800B53CD, 2);
m.WriteU32(0x800A0904 + 2 * 64 + 0x10, table);
m.WriteU32(table + 7 * 4, head);
m.WriteU16(head + 2, count);
m.WriteU32(cpu.GP + 0xAB4, backup);
m.LoadBytes(vertices, good);
// Prior costume's first coordinate is interpreted as a joint offset by the new head.
m.WriteU32(backup, 0x06500000);
m.WriteU32(0x800B5918, 0x80310000);
m.WriteU32(0x800B58F0, 0x800A625C);
m.WriteU32(0x800B58F4, 0x800A819C);
m.WriteU32(0x800B5940, 0x800A7ED4);
// Replay the previous post-hook's unconditional XYZ restore, then the real crashing transform.
for (uint i = 0; i < count; i++)
    for (uint component = 0; component < 6; component += 2)
        m.WriteU16(vertices + i * 8 + component, m.ReadU16(backup + i * 8 + component));
cpu.A0 = vertices; cpu.A1 = 1;
bool reproduced = false;
try { Recompiled.SpiderMan.func_8007B9CC(cpu, m); }
catch (InvalidOperationException e) { reproduced = e.Message.Contains("79BA8194"); Console.WriteLine("BASELINE: " + e.Message); }
Check(reproduced, "old stale-backup restore reproduces exact reported unmapped address 0x79BA8194");
cpu.SP = 0x807EF000;
foreach (uint expression in new uint[] { 0, 1, 2 })
{
    m.LoadBytes(vertices, good);
    cpu.A0 = 2048; cpu.A1 = expression; cpu.S0 = 123; cpu.S1 = 456;
    Recompiled.SpiderMan.func_800472C0(cpu, m);
    Check(Enumerable.Range(0, good.Length).All(i => m.ReadU8(vertices + (uint)i) == good[i]), $"expression {expression} preserves every head coordinate and joint byte");
    Check(cpu.SP == 0x807EF000 && cpu.S0 == 123 && cpu.S1 == 456, "native morph restores stack and saved registers");
    Check(m.ReadU32(0x800B4FA8) == (expression == 1 ? 1u : 0) && m.ReadU32(0x800B4FAC) == (expression == 2 ? 1u : 0) && m.ReadU32(cpu.GP + 0xAB8) == 2048, "expression state still updates");
    cpu.A0 = vertices; cpu.A1 = 1;
    Recompiled.SpiderMan.func_8007B9CC(cpu, m);
    Check(cpu.SP == 0x807EF000, "actual shared-vertex transform completes after patched morph");
}
// A stale/freed backup must never even be dereferenced for incompatible topology.
m.WriteU32(cpu.GP + 0xAB4, 0xDEADBEEF);
cpu.A0 = 4096; cpu.A1 = 1;
Recompiled.SpiderMan.func_800472C0(cpu, m);
Check(m.ReadU32(vertices) == BitConverter.ToUInt32(good), "incompatible morph never reads freed backup");
m.WriteU32(cpu.GP + 0xAB4, backup);
m.WriteU16(head + 2, 38);
for (uint i = 0; i < 38; i++) { m.WriteU32(backup + i * 8, 100); m.WriteU32(backup + i * 8 + 4, 20); }
cpu.A0 = 2048; cpu.A1 = 0;
Recompiled.SpiderMan.func_800472C0(cpu, m);
Check(m.ReadU16(vertices) == 50 && m.ReadU16(vertices + 4) == 10, "38-vertex retail head keeps original morph behavior");

string cardPath = Path.Combine(output, "carda.sav");
File.Copy(Path.Combine(root, ".local/continue/carda-original.sav"), cardPath, true);
byte[] original = File.ReadAllBytes(cardPath);
var card = new MemoryCard(cardPath);
int[] chain = card.Chain(card.Find("BASLUS-00875SPD"));
byte[] data = Enumerable.Range(0, 0xA00).Select(i => card.ReadByte(chain, i)).ToArray();
for (int slot = 0; slot < 3; slot++)
{
    byte[] bytes = data.AsSpan(0x200 + slot * 0xB4, 0xB4).ToArray();
    m.LoadBytes(0x80340000, bytes); cpu.A0 = 0x80340000;
    Recompiled.SpiderMan.func_80263394(cpu, m);
    Check(cpu.V0 == ContinueSave.Checksum(bytes) && cpu.V0 == BitConverter.ToUInt32(bytes), $"slot {slot + 1} validated against native PS1 checksum function");
}
string Hash(int slot) => Convert.ToHexString(SHA256.HashData(data.AsSpan(0x200 + slot * 0xB4, 0xB4)));
var history = new[] { new ContinueSave.SavedSlot(2, Hash(2), 100), new ContinueSave.SavedSlot(0, Hash(0), 200) };
Check(ContinueSave.SelectSlot(data, history) == 0, "overwritten earlier slot outranks later progress when saved more recently");
byte[] corrupt = (byte[])data.Clone(); corrupt[0x210] ^= 1;
Check(ContinueSave.SelectSlot(corrupt, history) == 2, "corrupt latest slot falls back to previous valid save");
Check(ContinueSave.SelectSlot(data, [new(0, "stale checksum", 999), new(2, Hash(2), 100)]) == 2, "stale metadata cannot select changed card data");
Check(ContinueSave.SelectSlot(new byte[0xA00], []) == -1, "empty card does not enable Continue");
File.WriteAllText(cardPath + ".continue.json", JsonSerializer.Serialize(new[] { new ContinueSave.SavedSlot(2, Hash(2), 100) }));
Runtime.CardA = card;
Runtime.CardB = new MemoryCard(Path.Combine(output, "missing.sav")) { Enabled = false };
m = Memory(); m.WriteU8(0x800A5700, 0);
cpu.SP = 0x807EF000;
var registers = cpu.Snapshot();
ContinueSave.Startup(cpu, m);
Check(Enumerable.Range(0, 0xB4).All(i => m.ReadU8(0x800A5688 + (uint)i) == data[0x368 + i]), "startup loads entire latest slot 3 including Continue flag");
Check(cpu.Snapshot().gpr.SequenceEqual(registers.gpr), "autoload preserves caller registers");
// Manual Load Game's native copy replaces the startup slot and must stay authoritative.
m.LoadBytes(0x800A5688, data.AsSpan(0x200, 0xB4).ToArray());
ContinueSave.Startup(cpu, m);
Check(m.ReadU32(0x800A5688) == BitConverter.ToUInt32(data, 0x200), "manual slot 1 load remains selected on returning to menu");
Check(File.ReadAllBytes(cardPath).SequenceEqual(original), "autoload and manual-load checks leave card byte-identical");
// Record successful saves only, using the actual on-disk card and native copy destination.
m.LoadBytes(0x800967CC, data);
m.WriteU8(0x800967B6, (byte)'0');
cpu.A0 = 0x800969CC; ContinueSave.CaptureOverwrite(cpu, m);
cpu.V0 = 1; ContinueSave.Saved(cpu, m);
Check(JsonSerializer.Deserialize<List<ContinueSave.SavedSlot>>(File.ReadAllText(cardPath + ".continue.json"))!.Count == 1, "failed native save does not change latest slot");
cpu.A0 = 0x800969CC; ContinueSave.CaptureOverwrite(cpu, m);
cpu.V0 = 0; ContinueSave.Saved(cpu, m);
var savedHistory = JsonSerializer.Deserialize<List<ContinueSave.SavedSlot>>(File.ReadAllText(cardPath + ".continue.json"))!;
Check(ContinueSave.SelectSlot(data, savedHistory) == 0, "successful slot 1 overwrite becomes latest on next startup");
cpu.V0 = 0x800969CC + 0xB4; ContinueSave.CaptureNew(cpu, m);
cpu.V0 = 0; ContinueSave.Saved(cpu, m);
Check(ContinueSave.SelectSlot(data, JsonSerializer.Deserialize<List<ContinueSave.SavedSlot>>(File.ReadAllText(cardPath + ".continue.json"))!) == 1, "new-name save path records slot 2");
m.WriteU8(0x800969CC + 1, 0xEE);
string previous = File.ReadAllText(cardPath + ".continue.json");
cpu.A0 = 0x800969CC; ContinueSave.CaptureOverwrite(cpu, m);
cpu.V0 = 0; ContinueSave.Saved(cpu, m);
Check(File.ReadAllText(cardPath + ".continue.json") == previous, "disk mismatch never records an unpersisted save");
Check(File.ReadAllBytes(cardPath).SequenceEqual(original), "save-order tracking never modifies card bytes");
File.WriteAllText(cardPath + ".continue.json", "[null,{}, {\"Slot\":99}]");
m = Memory(); m.WriteU8(0x800A5700, 0);
ContinueSave.Startup(cpu, m);
Check(m.ReadU32(0x800A5688) == BitConverter.ToUInt32(data, 0x368), "malformed history records cannot break startup; legacy fallback still works");
Runtime.CardA.Enabled = false;
m = Memory(); m.WriteU8(0x800A5700, 0);
ContinueSave.Startup(cpu, m);
Check(m.ReadU8(0x800A5700) == 0, "disabled memory card cannot autoload");
Console.WriteLine($"{checks} checks passed; no window, audio, or host input.");
