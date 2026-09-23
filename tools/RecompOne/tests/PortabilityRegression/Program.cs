using System.Buffers.Binary;
using System.IO.Compression;
using RecompOne.Runtime.Assets;
using RecompOne.Runtime.Host;

int passed = 0;
void Check(bool condition, string name)
{
    if (!condition) throw new Exception("FAIL: " + name);
    ++passed;
    Console.WriteLine("PASS " + name);
}
void Put(byte[] b, int off, uint value) => BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(off, 4), value);
void Bad(byte[] b, string name)
{
    try { NativeActorLodValidator.Validate(b, name); }
    catch (InvalidDataException) { Check(true, name); return; }
    throw new Exception("FAIL: accepted " + name);
}
string temp = Path.GetFullPath(Path.Combine(Path.GetTempPath(), "OpenSpidey-path-regression"));
string app = Path.Combine(temp, "game folder");
string host = Path.Combine(temp, "sdk", OperatingSystem.IsWindows() ? "dotnet.exe" : "dotnet");
string cache = Path.Combine(temp, "bundle-cache");
string dll = Path.Combine(app, "SpiderMan.dll");
string exe = Path.Combine(app, OperatingSystem.IsWindows() ? "SpiderMan.exe" : "SpiderMan");
Check(RuntimePaths.ResolveApplicationDirectory(host, dll, cache) == app, "managed DLL resolves beside game, not dotnet");
Check(RuntimePaths.ResolveApplicationDirectory(exe, Path.Combine(cache, "SpiderMan.dll"), cache) == app, "single-file resolves beside apphost, not extraction cache");
Check(RuntimePaths.ResolveApplicationDirectory(exe, dll, app) == app, "ordinary apphost");
Check(RuntimePaths.ResolveApplicationDirectory(null, dll, cache) == app, "absent process path uses entry assembly");
Check(RuntimePaths.ResolveApplicationDirectory(null, null, app) == app, "base-directory fallback");
Check(RuntimePaths.IsDotnetHost("dotnet") && RuntimePaths.IsDotnetHost("DOTNET.EXE") && !RuntimePaths.IsDotnetHost("my-dotnet-game"), "dotnet basename matching");
Check(OperatingSystem.IsWindows() ? RuntimePaths.RuntimeMutexName.StartsWith(@"Local\") : !RuntimePaths.RuntimeMutexName.Contains('\\'), "platform-specific mutex namespace");
string[] names = { "SPIDEY_DATA", "SPIDEY_ASSET_DIR", "SPIDEY_SUIT_MOD_DIR", "SPIDEY_LOG_DIR", "RECOMP_ASSET_PACK_DIR" };
string?[] saved = names.Select(Environment.GetEnvironmentVariable).ToArray();
try
{
    foreach (string n in names) Environment.SetEnvironmentVariable(n, "relative folder");
    string[] input = { Path.Combine("disc folder", "game.cue"), "unchanged" };
    string[] output = RuntimePaths.PrepareLaunchArguments(input);
    Check(output[0] == Path.GetFullPath(input[0]) && output[1] == input[1], "caller-relative disc argument");
    Check(input[0] != output[0], "arguments are not modified in place");
    foreach (string n in names) Check(Environment.GetEnvironmentVariable(n) == "relative folder", n + " historical application-relative semantics retained");
    Check(RuntimePaths.PrepareLaunchArguments(Array.Empty<string>()).Length == 0, "no disc argument");
}
finally { for (int i = 0; i < names.Length; ++i) Environment.SetEnvironmentVariable(names[i], saved[i]); }

// One object, two 28-byte LOD headers. Only the final mesh is terminal.
byte[] fixture = new byte[128];
Put(fixture, 0, 4); Put(fixture, 4, 116); Put(fixture, 8, 1);
Put(fixture, 48, 2); Put(fixture, 52, 60); Put(fixture, 56, 88);
BinaryPrimitives.WriteUInt16LittleEndian(fixture.AsSpan(86, 2), 1);
BinaryPrimitives.WriteUInt16LittleEndian(fixture.AsSpan(114, 2), ushort.MaxValue);
Put(fixture, 116, 0x2A); Put(fixture, 120, 0); Put(fixture, 124, uint.MaxValue);
var layout = NativeActorLodValidator.Validate(fixture, "fixture");
Check(layout is { Objects: 1, Meshes: 2, TerminalLods: 1, Animated: true }, "valid LOD chain");
byte[] changed = (byte[])fixture.Clone();
BinaryPrimitives.WriteUInt16LittleEndian(changed.AsSpan(86, 2), ushort.MaxValue);
Bad(changed, "extra terminal LOD rejected");
changed = (byte[])fixture.Clone(); Put(changed, 8, uint.MaxValue); Bad(changed, "object count overflow rejected");
changed = (byte[])fixture.Clone(); Put(changed, 48, uint.MaxValue); Bad(changed, "mesh count overflow rejected");
changed = (byte[])fixture.Clone(); Put(changed, 52, 8); Bad(changed, "mesh before pointer table rejected");
changed = (byte[])fixture.Clone(); Put(changed, 120, uint.MaxValue); Bad(changed, "metadata block overflow rejected");
for (int len = 0; len < fixture.Length; ++len) Bad(fixture[..len], "truncated actor " + len);
Check(NativeActorLodValidator.Validate(new byte[12], "texture").Objects == 0, "texture companion bypass");
var rng = new Random(50406);
for (int iteration = 0; iteration < 2000; ++iteration)
{
    changed = (byte[])fixture.Clone();
    for (int j = 0; j < 1 + iteration % 5; ++j) changed[rng.Next(changed.Length)] ^= (byte)rng.Next(1, 256);
    try { NativeActorLodValidator.Validate(changed, "mutation"); }
    catch (InvalidDataException) { }
    // A valid mutation may still pass. Bounds exceptions/crashes must not.
}
Check(true, "2000 deterministic mutations: accept or InvalidDataException only");
// The SM1 unpacked animation path has 30 fixed slots; packed 0x2C bosses
// are deliberately not assigned that limit. These are synthetic boundary actors.
byte[] CapacityActor(int n, uint tag)
{
    int table=12+n*36,first=table+4+n*4,meta=first+n*28;
    byte[] b=new byte[meta+12];Put(b,0,4);Put(b,4,(uint)meta);Put(b,8,(uint)n);Put(b,table,(uint)n);
    for(int i=0;i<n;i++) {int at=first+i*28;Put(b,table+4+i*4,(uint)at);BinaryPrimitives.WriteUInt16LittleEndian(b.AsSpan(at+26,2),ushort.MaxValue);}
    Put(b,meta,tag);Put(b,meta+4,0);Put(b,meta+8,uint.MaxValue);return b;
}
Check(NativeActorLodValidator.Validate(CapacityActor(30,0x2A),"30 unpacked",30).TerminalLods==30,"SM1 30-slot boundary accepted");
try { NativeActorLodValidator.Validate(CapacityActor(31,0x2A),"31 unpacked",30);throw new Exception("31-slot overflow accepted"); }
catch(InvalidDataException) { Check(true,"SM1 31-slot unpacked overflow rejected"); }
Check(NativeActorLodValidator.Validate(CapacityActor(46,0x2C),"46 packed",30).TerminalLods==46,"packed boss not incorrectly capped at 30");
try { NativeActorLodValidator.Validate(fixture,"negative profile",-1);throw new Exception("negative profile accepted"); }
catch(ArgumentOutOfRangeException) { Check(true,"invalid profile rejected"); }
string loaderHome = Path.Combine(Path.GetTempPath(), "OpenSpidey-loader-" + Guid.NewGuid().ToString("N"));
string? oldAssetRoot = Environment.GetEnvironmentVariable("SPIDEY_ASSET_DIR");
try
{
    Directory.CreateDirectory(Path.Combine(loaderHome, "wad"));
    string overrides = Path.Combine(loaderHome, "overrides"); Directory.CreateDirectory(overrides);
    File.WriteAllBytes(Path.Combine(loaderHome, "wad", "lizman.psx"), fixture);
    changed = (byte[])fixture.Clone();
    BinaryPrimitives.WriteUInt16LittleEndian(changed.AsSpan(86,2), ushort.MaxValue);
    File.WriteAllBytes(Path.Combine(overrides, "lizman.psx"), changed);
    Environment.SetEnvironmentVariable("SPIDEY_ASSET_DIR", overrides);
    LooseWadOverrides.Initialize(loaderHome);
    var cpu = new RecompOne.Runtime.Context.CpuContext();
    var memory = new RecompOne.Runtime.Memory.CaptureMemory();
    LooseWadOverrides.Find("lizman.psx"); LooseWadOverrides.FindExit(cpu);
    Check(cpu.V0 == 2048, "rejected override selects real retail entry size");
    Check(!LooseWadOverrides.TryAllocatePending(cpu.V0, out _), "rejected override does not allocate expanded RAM");
    Check(memory.Writes == 0, "rejected asset was never written to guest memory");
    cpu.A0 = 4096; LooseWadOverrides.Read(cpu, memory);
    Check(memory.Bytes.AsSpan(0,fixture.Length).SequenceEqual(fixture), "retail fallback bytes reach loader intact");
    File.WriteAllBytes(Path.Combine(overrides, "lizman.psx"), fixture);
    LooseWadOverrides.Find("lizman.psx"); LooseWadOverrides.FindExit(cpu);
    Check(LooseWadOverrides.TryAllocatePending(cpu.V0, out uint address), "valid override still uses bounded arena");
    Check(LooseWadOverrides.TryFree(address), "valid override allocation reclaimed");
    try { LooseWadOverrides.FindModel("bad suit", changed); throw new Exception("bad suit accepted"); }
    catch (InvalidDataException) { }
    cpu.V0 = 0; LooseWadOverrides.FindExit(cpu);
    Check(cpu.V0 == 0 && !LooseWadOverrides.TryAllocatePending(2048,out _), "rejected custom lookup clears stale pending bytes");
}
finally
{
    Environment.SetEnvironmentVariable("SPIDEY_ASSET_DIR", oldAssetRoot);
    Directory.Delete(loaderHome, recursive:true);
}
foreach (string path in args)
{
    using var zip = ZipFile.OpenRead(path);
    foreach (var item in zip.Entries.Where(e => e.Name.EndsWith(".psx", StringComparison.OrdinalIgnoreCase) && e.FullName == e.Name))
    {
        using var source = item.Open(); using var bytes = new MemoryStream(); source.CopyTo(bytes);
        byte[] data = bytes.ToArray(); byte[] before = (byte[])data.Clone();
        var actor = NativeActorLodValidator.Validate(data, item.FullName);
        Check(data.AsSpan().SequenceEqual(before), Path.GetFileName(Path.GetDirectoryName(path)) + "/" + item.Name + " validated without modification");
    }
}
Console.WriteLine($"{passed} checks passed; no game process or renderer was started.");
