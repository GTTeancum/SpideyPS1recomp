using System.Reflection;
using System.Text;
using Recompiled;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;

// Exercise the actual opt-in hook against synthetic descriptor/name storage.
// No retail data, generated game, native routine invocation, display, or audio is used.
const uint CurrentName = 0x800B3184;
const uint LoaderReturn = 0x8001A92C;
const uint Scratch = 0x802B0000;
int checks = 0;
void Check(bool passed, string description)
{
    if (!passed) throw new Exception(description);
    checks++;
    Console.WriteLine($"PASS: {description}");
}

(FixtureMemory memory, CpuContext context) Reset(string target, string current = "e1m0_t")
{
    // Each fixture represents a fresh process. Do not add a production reset API.
    foreach (string field in new[] { "_target", "_source", "_descriptorSelected" })
        typeof(LevelSwitch).GetField(field, BindingFlags.NonPublic | BindingFlags.Static)!
            .SetValue(null, field == "_descriptorSelected" ? false : null);
    Environment.SetEnvironmentVariable("SPIDEY_LEVEL", target);
    LevelSwitch.Install();
    var memory = new FixtureMemory(current);
    var context = new CpuContext { HI = 0x12345678, LO = 0x87654321 };
    for (int i = 1; i < 32; i++) context[i] = (uint)i * 0x01010101;
    context.A0 = CurrentName;
    context.RA = LoaderReturn;
    context.SetGteRead(4, context.A0, new GteScreen.VertexTag(100f, 12.5f, 20.25f, true));
    return (memory, context);
}

string previous = Environment.GetEnvironmentVariable("SPIDEY_LEVEL");
try
{
    var (memory, context) = Reset(null);
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Reads == 0 && memory.Writes.Count == 0, "no override leaves native descriptor selection untouched");
    Check(LevelSwitch.Redirect(context, memory, "e1m0_t.trg") == "e1m0_t.trg" &&
          context.A0 == CurrentName, "no override leaves archive lookup and its argument untouched");

    (memory, context) = Reset("  E1M3  ");
    context.RA = 0x8024B928; // A menu/save-preview caller must not consume the override.
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Reads == 0 && memory.Writes.Count == 0, "save-preview caller cannot select or consume the override");
    context.RA = LoaderReturn;
    context.A0 = 0x80110000;
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Reads == 0 && memory.Writes.Count == 0, "non-current descriptor buffers are unchanged");
    context.A0 = CurrentName;
    memory.SeedName("dem1_t");
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Text(CurrentName) == "dem1_t" && memory.Writes.Count == 0,
          "attract-demo stage loading does not consume a campaign override");
    memory.SeedName("e1m0_t");
    context.SetGteRead(4, context.A0, new GteScreen.VertexTag(100f, 12.5f, 20.25f, true));
    var registers = context.Snapshot();
    var tag = context.GetGteVertexTag(4);
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Text(CurrentName) == "e1m3_t", "E1M3 replaces tutorial metadata name before the native lookup");
    Check(memory.Writes.Count == 7 && memory.Writes.All(a => a >= CurrentName && a < CurrentName + 9),
          "selection writes only the bounded trigger name and terminator");
    Check(memory.Peek(CurrentName + 9) == 0xA5, "adjacent restart/suffix state is preserved");
    Check(registers.gpr.SequenceEqual(context.Snapshot().gpr) && registers.hi == context.HI &&
          registers.lo == context.LO && tag == context.GetGteVertexTag(4),
          "descriptor pre-hook preserves guest registers and projection tags");
    int writes = memory.Writes.Count;
    Check(LevelSwitch.Redirect(context, memory, "e1m3e.VAB") == "e1m3e.VAB" && memory.Writes.Count == writes,
          "selected descriptor uses native resource names without scratch redirection");
    memory.SeedName("E1M4_T");
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Text(CurrentName) == "E1M4_T" && memory.Writes.Count == writes,
          "subsequent E1M4 descriptor name is not redirected by the hook");
    Check(LevelSwitch.Redirect(context, memory, "E1M4_T.trg") == "E1M4_T.trg" && memory.Writes.Count == writes,
          "next stage cannot become a new fallback source after descriptor selection");
    Check(LevelSwitch.Redirect(context, memory, "e1m0_t.trg") == "e1m0_t.trg",
          "later native requests for the original tutorial are not redirected");

    (memory, context) = Reset("e1m4");
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Text(CurrentName) == "e1m4_t", "E1M4 selects the Shocker descriptor instead of tutorial metadata");
    memory.SeedName("e1m4_t");
    writes = memory.Writes.Count;
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Text(CurrentName) == "e1m4_t" && memory.Writes.Count == writes,
          "repeated E1M4 lookup retains the already-selected native stage");

    (memory, context) = Reset("e1m3", "E1M3_T");
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Writes.Count == 0 && memory.Text(CurrentName) == "E1M3_T",
          "already-correct case-insensitive descriptor is not rewritten");
    Check(LevelSwitch.Redirect(context, memory, "e1m4_t.trg") == "e1m4_t.trg",
          "already-correct descriptor still completes the one-shot selection");

    (memory, context) = Reset("e6m4");
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Text(CurrentName) == "e6m4_t", "last of the 24 native campaign descriptors is selectable");

    (memory, context) = Reset("e1m9");
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Text(CurrentName) == "e1m0_t" && memory.Writes.Count == 0,
          "missing descriptor does not replace the current campaign name or read beyond the table");

    (memory, context) = Reset("e1m4444");
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Reads == 0 && memory.Writes.Count == 0,
          "oversized target cannot overflow the nine-byte native name buffer");

    (memory, context) = Reset("e1m3", "e1m0_tXXX");
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Writes.Count == 0, "unterminated current name is left unchanged");
    memory.SeedName("e1m0_t");
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Text(CurrentName) == "e1m3_t", "invalid current name did not consume a later valid stage selection");

    (memory, context) = Reset("tr01");
    LevelSwitch.SelectDescriptor(context, memory);
    Check(memory.Reads == 0 && memory.Writes.Count == 0, "non-campaign target retains archive fallback");
    Check(LevelSwitch.Redirect(context, memory, "dem1_t.trg") == "dem1_t.trg" && context.A0 == CurrentName,
          "fallback waits through attract-demo files");
    Check(LevelSwitch.Redirect(context, memory, "e1m0e.VAB") == "tr01e.VAB" && context.A0 == Scratch &&
          memory.Text(Scratch) == "tr01e.VAB", "non-descriptor target still redirects its first gameplay prefix");
    Check(LevelSwitch.Redirect(context, memory, "e1m1_t.trg") == "e1m1_t.trg",
          "fallback preserves later stage prefixes");
    Check(LevelSwitch.Redirect(context, memory, "E1M0_G.psx") == "TR01_G.psx",
          "fallback preserves archive-name case and suffix");

    (memory, context) = Reset("dem2");
    Check(LevelSwitch.Redirect(context, memory, "dem1_t.trg") == "dem2_t.trg",
          "explicit demo targets retain their existing redirect behavior");
    Console.WriteLine($"PASS: {checks} SM2 level-selection checks; synthetic storage only, no gameplay.");
}
finally
{
    Environment.SetEnvironmentVariable("SPIDEY_LEVEL", previous);
}

sealed class FixtureMemory : IMemory
{
    const uint CurrentName = 0x800B3184;
    const uint Table = 0x800A3638;
    const uint Scratch = 0x802B0000;
    readonly Dictionary<uint, byte> bytes = new();
    public int Reads { get; private set; }
    public List<uint> Writes { get; } = new();

    public FixtureMemory(string current)
    {
        string[] names = ["e1m0", "e1m1", "e1m2", "e1m3", "e1m4", "e2m1", "e2m2", "e2m3",
            "e3m0", "e3m1", "e3m2", "e3m3", "e4m1", "e4m2", "e5m1", "e5m2", "e5m3", "e5m4",
            "e5m5", "e5m6", "e6m1", "e6m2", "e6m3", "e6m4"];
        for (int i = 0; i < names.Length; i++)
        {
            uint name = 0x80100000 + (uint)i * 16;
            Seed(name, Encoding.ASCII.GetBytes(names[i] + "_t\0"));
            Seed(Table + (uint)i * 20 + 4, BitConverter.GetBytes(name));
        }
        SeedName(current);
        bytes[CurrentName + 9] = 0xA5;
    }

    void Seed(uint address, byte[] data)
    {
        for (int i = 0; i < data.Length; i++) bytes[address + (uint)i] = data[i];
    }
    public void SeedName(string text)
    {
        for (uint i = 0; i < 9; i++) bytes[CurrentName + i] = 0;
        Seed(CurrentName, Encoding.ASCII.GetBytes(text));
    }
    public byte Peek(uint address) => bytes.TryGetValue(address, out byte value) ? value
        : throw new Exception($"Unexpected read outside fixture storage: 0x{address:X8}");
    public string Text(uint address)
    {
        var text = new StringBuilder();
        for (int i = 0; i < 40 && Peek(address + (uint)i) != 0; i++)
            text.Append((char)Peek(address + (uint)i));
        return text.ToString();
    }
    public byte ReadU8(uint address) { Reads++; return Peek(address); }
    public ushort ReadU16(uint address) => (ushort)(ReadU8(address) | ReadU8(address + 1) << 8);
    public uint ReadU32(uint address) => (uint)ReadU16(address) | (uint)ReadU16(address + 2) << 16;
    public void WriteU8(uint address, byte value)
    {
        if (!(address >= CurrentName && address < CurrentName + 9) &&
            !(address >= Scratch && address < Scratch + 64))
            throw new Exception($"Unexpected write outside permitted name storage: 0x{address:X8}");
        Writes.Add(address);
        bytes[address] = value;
    }
    public void WriteU16(uint address, ushort value) => throw new NotSupportedException();
    public void WriteU32(uint address, uint value) => throw new NotSupportedException();
    public uint ReadWordLeft(uint current, uint address) => throw new NotSupportedException();
    public uint ReadWordRight(uint current, uint address) => throw new NotSupportedException();
    public void WriteWordLeft(uint address, uint value) => throw new NotSupportedException();
    public void WriteWordRight(uint address, uint value) => throw new NotSupportedException();
    public void LoadBytes(uint address, byte[] data) => throw new NotSupportedException();
    public void ZeroRange(uint address, uint length) => throw new NotSupportedException();
    public void SetCd(RecompOne.Runtime.Cdrom.CdController cd) => throw new NotSupportedException();
}
