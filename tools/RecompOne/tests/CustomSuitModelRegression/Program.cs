using System.Buffers.Binary;
using RecompOne.Runtime.Assets.Suits;

byte[] Fixture(int flags = 0x1f, int faceLength = 36)
{
    using var s = new MemoryStream(); using var w = new BinaryWriter(s);
    w.Write((ushort)4); w.Write((ushort)2); w.Write(0); w.Write(18);
    w.Write(new byte[18 * 36]); w.Write(18); w.Write(new byte[18 * 4]);
    var pointers = new List<int>();
    for (int mesh = 0; mesh < 18; mesh++)
    {
        pointers.Add((int)s.Position);
        w.Write((ushort)0); w.Write((ushort)3); w.Write((ushort)4); w.Write((ushort)1); w.Write(new byte[20]);
        w.Write(new byte[3 * 8 + 4 * 8]);
        byte[] face = new byte[faceLength]; face[0] = (byte)flags; face[2] = (byte)faceLength; face[4] = 0; face[5] = 1; face[6] = 2; face[12] = 3; w.Write(face);
    }
    int meta = (int)s.Position; w.Write(uint.MaxValue); w.Write(new byte[18 * 4]);
    w.Write(1); w.Write(0xDEADBEEFu); w.Write(0); w.Write(0); w.Write(0);
    byte[] b = s.ToArray(); BinaryPrimitives.WriteInt32LittleEndian(b.AsSpan(4),meta);
    for(int i=0;i<18;i++) BinaryPrimitives.WriteInt32LittleEndian(b.AsSpan(664+i*4),pointers[i]);
    return b;
}
byte[] bytes = Fixture();
var model = SuitModel.Parse(bytes);
foreach (int size in new[] { SuitModel.ByteLimit + 1, SuitModel.GuestByteLimit + 1 })
{
    byte[] oversized = new byte[size];
    bytes.CopyTo(oversized, 0);
    try { SuitModel.Parse(oversized); throw new Exception("accepted oversized non-retargeted actor"); }
    catch (InvalidDataException) { Console.WriteLine("PASS: source/guest size bound " + size); }
}
if (!model.Materials.SetEquals(new[] { 0xDEADBEEFu })) throw new Exception("material IDs not read from model");
void Reject(Action<byte[]> mutate, string name)
{
    byte[] b = (byte[])bytes.Clone(); mutate(b);
    try { SuitModel.Parse(b); } catch (Exception e) when (e is InvalidDataException or OverflowException) { Console.WriteLine("PASS: " + name); return; }
    throw new Exception("accepted " + name);
}
Reject(b => b[0] = 3, "wrong container version");
Reject(b => BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(4),uint.MaxValue),"metadata pointer overflow");
Reject(b => b[8] = 19,"incompatible hierarchy size");
Reject(b => BinaryPrimitives.WriteInt32LittleEndian(b.AsSpan(664),0),"mesh points into header");
int first = BinaryPrimitives.ReadInt32LittleEndian(bytes.AsSpan(664));
Reject(b => BinaryPrimitives.WriteUInt16LittleEndian(b.AsSpan(first+2),257),"native vertex limit");
Reject(b => b[first+28+6]=2,"unresolved stitch");
Reject(b => b[first+28+3*8+4*8+4]=3,"triangle index out of range");
Reject(b => b[first+28+3*8+4*8+12]=4,"normal index out of range");
Reject(b => b[first+28+3*8+4*8+2]=35,"unsupported face record");
SuitModel.Parse(Fixture(0x0f));
SuitModel.Parse(Fixture(0x3f, 40));
Console.WriteLine("PASS: native quads and extended triangles");
int firstFace = first+28+3*8+4*8;
Reject(b => { b[firstFace]=0x0f; b[firstFace+7]=3; }, "quad fourth index out of range");
Reject(b => b[firstFace]=0x3f, "extended triangle with short record");
Reject(b => b[firstFace]=0x5f, "unsupported transparent face");
Reject(b => { b[firstFace]=0x3f; b[firstFace+2]=40; }, "extended triangle crosses mesh boundary");
bytes[0]=0;
if (model.Bytes.Span[0]!=4) throw new Exception("model aliases caller memory");
string[] actors = args.Length == 2 && args[0] == "--catalogue"
    ? Directory.GetFiles(args[1], "actor.psx", SearchOption.AllDirectories).Order().ToArray()
    : args;
if (args.Length != 0 && actors.Length == 0) throw new Exception("empty actor catalogue");
var failures = new List<string>();
foreach (string actorPath in actors)
{
    try
    {
        var real = SuitModel.Read(actorPath);
        if (real.RetargetRig?.Packets.Length > 18) CheckPagedActor(actorPath, real);
        Console.WriteLine($"PASS: converted actor {actorPath}: {real.Bytes.Length} bytes, {real.Materials.Count} material IDs");
    }
    catch (Exception e)
    {
        failures.Add($"actor {actorPath}: {e.Message}");
    }
}

foreach (string actorPath in actors)
{
    try { CheckManifest(actorPath); }
    catch (Exception e) { failures.Add($"manifest {actorPath}: {e.Message}"); }
}
foreach (string failure in failures) Console.Error.WriteLine("FAIL: " + failure);
Console.WriteLine($"Catalogue audit: {actors.Length} actors, {failures.Count} failures.");
return failures.Count == 0 ? 0 : 1;

void CheckPagedActor(string path, SuitModel model)
{
    byte[] source = File.ReadAllBytes(path);
    int U(byte[] b, int offset) => checked((int)BinaryPrimitives.ReadUInt32LittleEndian(b.AsSpan(offset)));
    int count = U(source, 660), root = U(source, 664), extra = U(source, 664 + 18 * 4);
    int terminals = Enumerable.Range(0, count).Count(i =>
        BinaryPrimitives.ReadUInt16LittleEndian(source.AsSpan(U(source, 664 + i * 4) + 26)) == ushort.MaxValue);
    if (terminals != 18 || model.RetargetRig!.Packets.Length != count)
        throw new Exception("paged mesh terminals/rig count changed");
    void Bad(Action<byte[]> change, string name)
    {
        var mutated = (byte[])source.Clone(); change(mutated);
        try { SuitModel.Parse(mutated); }
        catch (InvalidDataException) { Console.WriteLine("PASS: rejected paged " + name); return; }
        throw new Exception("accepted paged " + name);
    }
    Bad(b => BinaryPrimitives.WriteUInt16LittleEndian(b.AsSpan(root + 26), 0), "root cycle");
    Bad(b => BinaryPrimitives.WriteUInt16LittleEndian(b.AsSpan(extra + 26), ushort.MaxValue), "extra terminal");
    Bad(b => BinaryPrimitives.WriteUInt16LittleEndian(b.AsSpan(extra + 34), 2), "cross-page stitch");
    Bad(b => BinaryPrimitives.WriteInt32LittleEndian(b.AsSpan(660), 65), "mesh count overflow");
    int p = U(source, 4);
    while (BinaryPrimitives.ReadUInt32LittleEndian(source.AsSpan(p)) != NativeRetargetRig.Tag)
        p += 8 + U(source, p + 4);
    Bad(b => BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(p), 0x12345678), "missing preserved rig");
    Bad(b => BinaryPrimitives.WriteInt32LittleEndian(b.AsSpan(p + 8 + 280), count - 1), "rig/native page mismatch");
    float[] drivers = new float[216];
    for (int i = 0; i < 18; i++) for (int axis = 0; axis < 3; axis++)
    {
        drivers[i * 12 + axis * 4 + axis] = 1;
        drivers[i * 12 + axis * 4 + 3] = BitConverter.Int32BitsToSingle(
            BinaryPrimitives.ReadInt32LittleEndian(model.RetargetRig.PreservedData.Span.Slice(64 + i * 12 + axis * 4)));
    }
    model.RetargetRig.Evaluate(drivers);
    for (int i = 0; i < count; i++)
        if (model.RetargetRig.LocalVertices[i].Length != model.RetargetRig.Packets[i].Length * 6 ||
            model.RetargetRig.LocalVertices[i].Any(v => !float.IsFinite(v)))
            throw new Exception("managed page evaluation omitted vertices");
    Console.WriteLine($"PASS: {count} managed pages evaluated with 18 native drivers/terminals");
}

void CheckManifest(string actorPath)
{
    string manifestPath = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(actorPath))!, "suit.json");
    var suit = SuitManifest.Read(manifestPath);
    if (suit.Decode().Values.Any(texture => !texture.ModelSurface))
        throw new Exception("suit textures lost their model-surface classification");
    if (suit.CustomModel == null || !suit.Textures.Keys.All(suit.CustomModel.Materials.Contains))
        throw new Exception("custom manifest did not use its actor material IDs");
    Console.WriteLine("PASS: real custom manifest and actor material mapping: " + suit.Id);
    string testDir = Path.Combine(Path.GetTempPath(), "custom-suit-test-" + Guid.NewGuid());
    Directory.CreateDirectory(testDir);
    try
    {
        var doc = System.Text.Json.Nodes.JsonNode.Parse(File.ReadAllText(manifestPath))!;
        foreach (string bad in new[] { "../actor.psx", Path.GetFullPath(actorPath), "actor.fbx", "missing.psx" })
        {
            doc["modelFile"] = bad;
            string target = Path.Combine(testDir, "suit.json");
            File.WriteAllText(target, doc.ToJsonString());
            bool rejected = false;
            try { SuitManifest.Read(target); } catch (Exception e) when (e is InvalidDataException or IOException) { rejected = true; }
            if (!rejected) throw new Exception("accepted invalid modelFile " + bad);
            Console.WriteLine("PASS: rejected modelFile " + bad);
        }
    }
    finally
    {
        File.Delete(Path.Combine(testDir, "suit.json"));
        Directory.Delete(testDir);
    }
}
