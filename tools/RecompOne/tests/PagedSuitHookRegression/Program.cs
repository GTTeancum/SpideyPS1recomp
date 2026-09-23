using System.Reflection;
using Recompiled;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

var memory = new PSMemory(0x800000);
var cpu = new CpuContext();
int checks = 0;
void Check(bool value, string label)
{
    if (!value) throw new Exception(label);
    checks++;
}
void Set(string name, object value) => typeof(SuitRetargeting)
    .GetField(name, BindingFlags.Static | BindingFlags.NonPublic)!.SetValue(null, value);
var meshes = (uint[])typeof(SuitRetargeting)
    .GetField("Mesh", BindingFlags.Static | BindingFlags.NonPublic)!.GetValue(null)!;
for (int i = 0; i < meshes.Length; i++) meshes[i] = 0x80100000u + (uint)i * 4096;
const uint actor = 0x80010000, pose = 0x80020000, stack = 0x80030000, table = 0x80040000;
cpu.GP = 0x80050000;
void Root()
{
    cpu.S2 = actor; cpu.S3 = pose; cpu.SP = stack; cpu.S1 = 0; cpu.S0 = meshes[0];
    memory.WriteU32(stack + 0xA8, table);
}
Root();
Check(!SuitRetargeting.BeginPagedPart(cpu, memory), "ordinary actor takes original LOD path");
Check(!SuitRetargeting.TryNextPage(cpu, memory), "ordinary actor never repeats pages");
foreach (int count in new[] {21, 29, 64})
{
    Set("_pagedActor", actor); Set("_pagedPose", pose); Set("_pagedStack", stack);
    Set("_pagedTable", table); Set("_pageCount", count);
    Root(); memory.WriteU32(cpu.GP + 0x1164, 1);
    Check(SuitRetargeting.BeginPagedPart(cpu, memory), "paged root bypasses LOD selection");
    for (int page = 18; page < count; page++)
    {
        memory.WriteU32(cpu.GP + 0x1164, 0);
        Check(SuitRetargeting.TryNextPage(cpu, memory), "extra page scheduled");
        Check(cpu.S0 == meshes[page], "each extra page submitted in order");
        Check(cpu.S1 == 0 && cpu.S3 == pose && cpu.SP == stack && cpu.S2 == actor,
            "extra page uses original root matrix and leaves native loop counters intact");
        Check(memory.ReadU32(cpu.GP + 0x1164) == 1, "root visibility restored");
    }
    Check(!SuitRetargeting.TryNextPage(cpu, memory), "bounded page loop terminates");
    for (uint part = 1; part < 18; part++)
    {
        cpu.S1 = part; cpu.S3 = pose + part * 24; cpu.S0 = meshes[part];
        Check(SuitRetargeting.BeginPagedPart(cpu, memory), "ordinary paged part bypasses LOD");
        Check(!SuitRetargeting.TryNextPage(cpu, memory), "no extra pages under other drivers");
    }
    Root(); cpu.S2 += 4;
    Check(!SuitRetargeting.BeginPagedPart(cpu, memory), "NPC actor mismatch stays native");
    Root(); cpu.SP += 4;
    Check(!SuitRetargeting.BeginPagedPart(cpu, memory), "nested draw stack mismatch stays native");
    Root(); cpu.S3 += 24;
    Check(!SuitRetargeting.BeginPagedPart(cpu, memory), "pose mismatch stays native");
    Root(); memory.WriteU32(stack + 0xA8, table + 4);
    Check(!SuitRetargeting.BeginPagedPart(cpu, memory), "resident table mismatch stays native");
    Root(); cpu.S0 += 4;
    bool rejected = false;
    try { SuitRetargeting.BeginPagedPart(cpu, memory); }
    catch (InvalidDataException) { rejected = true; }
    Check(rejected, "root mesh mismatch rejected");
}
Root();
SuitRetargeting.ApplyPose(cpu, memory);
Check(!SuitRetargeting.BeginPagedPart(cpu, memory), "new non-custom draw clears stale page context");
Console.WriteLine($"PASS: {checks} process-local page scheduling checks; no GPU or visual acceptance.");
