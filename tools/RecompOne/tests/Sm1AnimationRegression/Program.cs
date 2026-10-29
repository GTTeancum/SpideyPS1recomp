using Recompiled;
using RecompOne.Runtime.Assets;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;

// No game assets, generated game code, display, or audio device are required.
// The fixture represents the resource entry returned by the native model loader.
const uint Entry = 0x800A0904u;
const uint AllocationSize = 0x10000u;
const uint MetadataOffset = 0x1000u;
const uint PrefixLength = 8u;
const uint BankOffset = MetadataOffset + 8u + PrefixLength + 8u;
int failures = 0;
void Check(bool pass, string name)
{
    Console.WriteLine($"{(pass ? "PASS" : "FAIL")}: {name}");
    if (!pass) failures++;
}

string root = Path.Combine(Path.GetTempPath(), "sm1-animation-" + Guid.NewGuid());
Directory.CreateDirectory(Path.Combine(root, "wad"));
string? previousAssets = Environment.GetEnvironmentVariable("SPIDEY_ASSET_DIR");
try
{
    Environment.SetEnvironmentVariable("SPIDEY_ASSET_DIR", null);
    LooseWadOverrides.Initialize(root);
    uint model = LooseWadOverrides.AllocateScratch(AllocationSize, "synthetic animation model");
    uint bank = model + BankOffset;
    var memory = new PSMemory(0x800000);
    var context = new CpuContext { HI = 0x12345678, LO = 0x87654321 };
    for (int i = 1; i < 32; i++) context[i] = (uint)i * 0x01010101u;
    context.V0 = Entry;
    context.SetGteRead(4, context.A0, new GteScreen.VertexTag(100f, 12.5f, 20.25f, true));

    uint Reset(uint parts = 19, ushort cachedParts = 37, ushort secondStrideMinusOne = 1)
    {
        memory.ZeroRange(model, AllocationSize);
        memory.ZeroRange(Entry, 64u);
        memory.WriteU32(model, 0x00020004u); // Two ushort fields: version 4, magic 2.
        memory.WriteU32(model + 4u, MetadataOffset);
        memory.WriteU32(model + 8u, parts);
        memory.WriteU32(Entry + 20u, model);
        memory.WriteU32(Entry + 24u, bank);
        memory.WriteU16(Entry + 52u, cachedParts);
        // A preceding unrelated tag exercises traversal of the real metadata chain.
        memory.WriteU32(model + MetadataOffset, 0x1234u);
        memory.WriteU32(model + MetadataOffset + 4u, PrefixLength);
        memory.WriteU32(bank - 8u, 0x2Au);
        uint firstSize = 10u * parts * 24u; // 40 frames sampled every four frames.
        uint stride = (uint)secondStrideMinusOne + 1u;
        uint secondSize = ((17u + stride - 1u) / stride) * parts * 24u;
        uint length = 20u + firstSize + secondSize;
        memory.WriteU32(bank - 4u, length);
        memory.WriteU32(bank, 2u);
        memory.WriteU32(bank + 4u, 20u);
        memory.WriteU16(bank + 8u, 40);
        memory.WriteU16(bank + 10u, 3);
        memory.WriteU32(bank + 12u, 20u + firstSize);
        memory.WriteU16(bank + 16u, 17);
        memory.WriteU16(bank + 18u, secondStrideMinusOne);
        memory.WriteU32(bank + length, uint.MaxValue);
        context.V0 = Entry;
        return length;
    }

    void Expect(ushort parts, string name, bool corrected = false)
    {
        var guarded = new BoundedMemory(memory, model, AllocationSize, Entry);
        var registers = context.Snapshot();
        var tag = context.GetGteVertexTag(4);
        Sm1AnimationBounds.AfterModelLoad(context, guarded);
        Check(memory.ReadU16(Entry + 52u) == parts && guarded.Writes == (corrected ? 1 : 0), name);
        if (!registers.gpr.SequenceEqual(context.Snapshot().gpr) ||
            registers.hi != context.HI || registers.lo != context.LO ||
            tag != context.GetGteVertexTag(4))
            throw new Exception("Post-loader hook changed guest registers or projection tags");
    }

    Reset();
    memory.WriteU32(Sm1AnimationBounds.CollisionGrid + 8u, 0xA5A5A5A5);
    memory.WriteU32(Sm1AnimationBounds.CollisionGrid + 12u, 0xA5A5A5A5);
    Expect(19, "duplicate-LOD count 37 is corrected to the 19-part raw animation layout", corrected: true);
    Check(19u * Sm1AnimationBounds.BytesPerPart == 456u &&
        Sm1AnimationBounds.AnimationScratch + 456u < Sm1AnimationBounds.CollisionGrid &&
        memory.ReadU32(Sm1AnimationBounds.CollisionGrid + 8u) == 0xA5A5A5A5 &&
        memory.ReadU32(Sm1AnimationBounds.CollisionGrid + 12u) == 0xA5A5A5A5,
        "corrected frame extent is 456 bytes and the hook preserves neighboring grid data");
    memory.WriteU32(model + 4u, uint.MaxValue);
    memory.WriteU32(Entry + 24u, 0x0D6F0201u);
    Expect(19, "already-correct counts return without following unverified metadata");

    Reset(cachedParts: 18);
    Expect(18, "undercounts are not guessed from object count");
    Reset(parts: 0);
    Expect(37, "zero-part actors are unchanged");
    Reset(parts: 31);
    Expect(37, "actors above the native 30-part scratch capacity are not guessed");
    Reset(parts: 30);
    Expect(30, "validated 30-part banks fit the exact native scratch boundary", corrected: true);
    Check(Sm1AnimationBounds.MaxParts == 30 &&
        Sm1AnimationBounds.AnimationScratch + 30u * Sm1AnimationBounds.BytesPerPart ==
        Sm1AnimationBounds.CollisionGrid, "native capacity ends exactly at collision-grid storage");
    Reset(secondStrideMinusOne: 2);
    Expect(19, "non-power-of-two sample intervals use ceiling frame counts", corrected: true);

    Reset(); memory.WriteU16(model, 6);
    Expect(37, "other model versions are unchanged");
    Reset(); memory.WriteU16(model + 2u, 0);
    Expect(37, "invalid model magic is unchanged");
    Reset(); memory.WriteU32(bank - 8u, 0x2Cu);
    Expect(37, "compressed animation tag 0x2C is not treated as raw transforms");
    Reset(); memory.WriteU32(Entry + 24u, bank + 4u);
    Expect(37, "an interior bank pointer must match an actual metadata payload");
    Reset(); memory.WriteU32(Entry + 24u, 0x0D6F0201u);
    Expect(37, "unmapped bank pointers are not dereferenced");

    Reset(); memory.WriteU32(model + 4u, uint.MaxValue);
    Expect(37, "overflowing metadata offsets are rejected within the allocation");
    Reset(); memory.WriteU32(model + 4u, AllocationSize - 4u);
    memory.WriteU32(model + AllocationSize - 4u, 0x2Au);
    Expect(37, "truncated metadata headers never read beyond the allocation");
    Reset(); memory.WriteU32(model + MetadataOffset + 4u, uint.MaxValue);
    Expect(37, "oversized preceding metadata chunks are rejected");
    Reset(); memory.WriteU32(bank - 4u, AllocationSize);
    Expect(37, "animation chunks cannot extend beyond the live model allocation");
    Reset(); memory.WriteU32(bank, uint.MaxValue);
    Expect(37, "oversized clip tables are rejected before reading entries");
    Reset(); memory.WriteU32(bank + 4u, 12u);
    Expect(37, "clip payloads cannot overlap their table");
    Reset(); memory.WriteU32(bank + 12u, 19u);
    Expect(37, "backward clip offsets are rejected");
    uint length = Reset(); memory.WriteU32(bank + 12u, length + 1u);
    Expect(37, "clip offsets cannot leave the animation chunk");
    Reset(); memory.WriteU16(bank + 8u, 41);
    Expect(37, "a single mismatched early clip prevents correction");
    Reset(); memory.WriteU16(bank + 16u, 19);
    Expect(37, "the final clip must also prove the part count");
    Reset(); memory.WriteU16(bank + 16u, 0);
    Expect(37, "empty clips are conservatively left unchanged");
    length = Reset(); memory.WriteU32(bank - 4u, length + 24u);
    Expect(37, "padded or ambiguous raw banks are left unchanged");

    Reset(); context.V0 = Entry + 1u;
    Expect(37, "misaligned model-table pointers are ignored");
    Reset(); context.V0 = Entry + 40u * 64u;
    Expect(37, "slots beyond the native model table are ignored");
    Reset(); memory.WriteU32(Entry + 20u, model + 16u);
    Expect(37, "interior allocation addresses cannot establish model ownership");
    Reset();
    Check(LooseWadOverrides.TryGetAllocationSize(model, out uint size) && size == AllocationSize,
        "allocation query returns the exact tracked bounds");
    Check(LooseWadOverrides.TryFree(model), "synthetic model allocation can be released");
    Expect(37, "freed or retail-owned models are not inspected or changed");
    Check(!LooseWadOverrides.TryGetAllocationSize(model, out size) && size == 0,
        "released allocations no longer expose bounds");
}
finally
{
    Environment.SetEnvironmentVariable("SPIDEY_ASSET_DIR", previousAssets);
    Directory.Delete(root, true);
}

Console.WriteLine($"SM1 animation regression: {(failures == 0 ? "PASS" : $"{failures} failure(s)")}");
return failures == 0 ? 0 : 1;

// Each call fails immediately if validation reads outside the one known model
// allocation/resource record or writes anything except the repaired count.
sealed class BoundedMemory(PSMemory memory, uint model, uint size, uint entry) : IMemory
{
    public int Writes { get; private set; }
    void Read(uint address, uint bytes)
    {
        bool In(uint start, uint length) => address >= start && bytes <= length &&
            address - start <= length - bytes;
        if (!In(model, size) && !In(entry, 64u))
            throw new Exception($"Validation read outside model/resource storage: 0x{address:X8}");
    }
    public byte ReadU8(uint address) { Read(address, 1); return memory.ReadU8(address); }
    public ushort ReadU16(uint address) { Read(address, 2); return memory.ReadU16(address); }
    public uint ReadU32(uint address) { Read(address, 4); return memory.ReadU32(address); }
    public void WriteU16(uint address, ushort value)
    {
        if (address != entry + 52u) throw new Exception("Unexpected hook write");
        Writes++;
        memory.WriteU16(address, value);
    }
    public void WriteU8(uint address, byte value) => throw new Exception("Unexpected hook write");
    public void WriteU32(uint address, uint value) => throw new Exception("Unexpected hook write");
    public uint ReadWordLeft(uint current, uint address) => throw new NotSupportedException();
    public uint ReadWordRight(uint current, uint address) => throw new NotSupportedException();
    public void WriteWordLeft(uint address, uint value) => throw new NotSupportedException();
    public void WriteWordRight(uint address, uint value) => throw new NotSupportedException();
    public void LoadBytes(uint address, byte[] data) => throw new NotSupportedException();
    public void ZeroRange(uint address, uint length) => throw new NotSupportedException();
    public void SetCd(RecompOne.Runtime.Cdrom.CdController cd) => throw new NotSupportedException();
}
