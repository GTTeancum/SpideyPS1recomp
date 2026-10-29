using System.IO.Compression;
using Recompiled;
using RecompOne.Runtime.Assets;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

if (args.Length is < 1 or > 2)
    throw new ArgumentException("Usage: AnimationReleaseProbe <path/to/spiderman/runtime-assets.zip> [lizman.psx|lizman2.psx]");
string assetName = args.Length == 2 ? args[1] : "lizman2.psx";
if (assetName != "lizman.psx" && assetName != "lizman2.psx")
    throw new ArgumentException("This historical crash probe supports lizman.psx and lizman2.psx");
using var archive = ZipFile.OpenRead(args[0]);
using var stream = archive.GetEntry(assetName)?.Open()
    ?? throw new InvalidDataException($"Archive has no {assetName}");
using var bytes = new MemoryStream();
stream.CopyTo(bytes);
byte[] data = bytes.ToArray();
uint U32(int offset) => BitConverter.ToUInt32(data, offset);
ushort U16(int offset) => BitConverter.ToUInt16(data, offset);
int objectCount = checked((int)U32(8));
int countOffset = checked(12 + objectCount * 36);
int meshCount = checked((int)U32(countOffset));
uint[] meshes = Enumerable.Range(0, meshCount).Select(i => U32(countOffset + 4 + i * 4)).ToArray();
ushort[] markers = meshes.Select(p => U16(checked((int)p + 26))).ToArray();
int bankOffset = checked((int)U32(4));
while (U32(bankOffset) != 0x2A)
{
    if (U32(bankOffset) == uint.MaxValue) throw new InvalidDataException("No raw animation bank");
    bankOffset = checked(bankOffset + 8 + (int)U32(bankOffset + 4));
}
bankOffset += 8;
Console.WriteLine($"Bundled {assetName}: header=0x{U32(0):X8}, {objectCount} objects, {meshCount} meshes, " +
    $"{markers.Count(x => x == 0xFFFF)} terminal markers");

const uint Resource = 0x800A0904, MeshTable = 0x80110004, MeshBase = 0x80111000, Actor = 0x80120000;
const uint Scratch = Sm1AnimationBounds.AnimationScratch, Grid = Sm1AnimationBounds.CollisionGrid;
string root = Path.Combine(Path.GetTempPath(), "sm1-animation-probe-" + Guid.NewGuid());
Directory.CreateDirectory(Path.Combine(root, "wad"));
string? previousAssets = Environment.GetEnvironmentVariable("SPIDEY_ASSET_DIR");
try
{
    Environment.SetEnvironmentVariable("SPIDEY_ASSET_DIR", null);
    LooseWadOverrides.Initialize(root);
    uint model = LooseWadOverrides.AllocateScratch((uint)data.Length, $"bundled {assetName} probe");
    var m = new PSMemory(0x800000);
    var c = new CpuContext { SP = 0x807FFF00, GP = 0x800B47F4, A0 = 0 };
    m.LoadBytes(model, data);
    m.WriteU32(Resource + 16, MeshTable);
    m.WriteU32(Resource + 20, model);
    m.WriteU32(Resource + 24, model + (uint)bankOffset);
    m.WriteU32(MeshTable - 4, (uint)meshCount);
    // Keep the actual header, object records, metadata and animations. Synthetic
    // zero-polygon meshes retain every original terminal marker and isolate the
    // native counting routine from unrelated texture/render initialization.
    for (int i = 0; i < meshCount; i++)
    {
        uint mesh = MeshBase + (uint)(28 * i);
        m.WriteU32(MeshTable + (uint)(i * 4), mesh);
        m.WriteU16(mesh + 26, markers[i]);
    }
    Recompiled.SpiderMan.func_80074C98_Impl(c, m);
    ushort parsedCount = m.ReadU16(Resource + 52);
    Console.WriteLine($"Released func_80074C98_Impl computed animation count: {parsedCount}");
    if (parsedCount != 37) throw new Exception("Parser did not reproduce the historical lizman2 overcount");

    void Decode(string label, ushort expectedCount, uint expectedBytes, bool corrupt)
    {
        for (uint offset = 0; offset < 0x400; offset += 4) m.WriteU32(Scratch + offset, 0xA5A5A5A5);
        c.A0 = Actor;
        c.SP = 0x807FFF00;
        Recompiled.SpiderMan.func_800742A8(c, m);
        uint grid8 = m.ReadU32(Grid + 8), grid12 = m.ReadU32(Grid + 12);
        uint lastChanged = 0;
        for (uint offset = 0; offset < 0x400; offset++)
            if (m.ReadU8(Scratch + offset) != 0xA5) lastChanged = offset;
        Console.WriteLine($"{label}: count={m.ReadU16(Resource + 52)}, bytes={lastChanged + 1}, " +
            $"grid+8=0x{grid8:X8}, grid+12=0x{grid12:X8}");
        if (m.ReadU16(Resource + 52) != expectedCount || lastChanged + 1 != expectedBytes)
            throw new Exception("Unexpected native decoder write extent");
        if (corrupt && (grid8 != 0xF2820894 || grid12 != 0x0D6F0201))
            throw new Exception("Exact reported crash values not reproduced");
        if (!corrupt && (grid8 != 0xA5A5A5A5 || grid12 != 0xA5A5A5A5))
            throw new Exception("Patched decoder corrupted collision-grid sentinels");
    }

    Decode("Before hook", 37, 888, corrupt: true);
    // func_80068BB0 finishes its metadata walk by returning this entry pointer
    // in V0. Supply that documented post-hook contract; no full LoadPsx/game run
    // is implied by this isolated probe.
    c.V0 = Resource;
    var registers = c.Snapshot();
    Sm1AnimationBounds.AfterModelLoad(c, m);
    if (!registers.gpr.SequenceEqual(c.Snapshot().gpr) || registers.hi != c.HI || registers.lo != c.LO)
        throw new Exception("Post-hook changed guest registers");
    Decode("After actual AfterModelLoad hook", 19, 456, corrupt: false);
    Console.WriteLine("PASS: released native parser/decoder reproduce issue #5 corruption; the actual runtime hook preserves the grid.");
    LooseWadOverrides.TryFree(model);
}
finally
{
    Environment.SetEnvironmentVariable("SPIDEY_ASSET_DIR", previousAssets);
    Directory.Delete(root, true);
}
