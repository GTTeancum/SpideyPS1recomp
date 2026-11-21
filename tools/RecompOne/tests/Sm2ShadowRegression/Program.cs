using System.Numerics;
using System.Reflection;
using System.Text;
using Recompiled;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hle;
using RecompOne.Runtime.Memory;

var memory = new PSMemory(0x800000);
const uint player = 0x80100000, camera = 0x80110000, face = 0x80120000;
const uint frame = 0x80130000, ot = 0x80140000, packet = 0x00150000;
const uint stack = 0x807ef000;
var cpu = new CpuContext { SP = stack, GP = 0x800C1764, A0 = player, S3 = player };
int checks = 0;
void Check(bool condition, string description)
{
    if (!condition) throw new Exception(description);
    ++checks;
    Console.WriteLine("PASS " + description);
}
T Field<T>(string name) => (T)typeof(PlayerShadow)
    .GetField(name, BindingFlags.Static | BindingFlags.NonPublic)!.GetValue(null)!;
void Level(string name) => memory.LoadBytes(0x800B3184, Encoding.ASCII.GetBytes(name + "\0"));
bool Draws()
{
    cpu.A0 = player;
    PlayerShadow.ActorBegin(cpu, memory);
    return Field<bool>("_drawing");
}
void Hit(bool valid)
{
    memory.WriteU32(stack + 0xA8, valid ? 1u : 0u);
    PlayerShadow.GroundCollision(cpu, memory);
}

Level("e1m3_t");
memory.WriteU32(0x800C21F4, player);
memory.WriteU32(cpu.GP + 0x1310, camera);
for (uint i = 0; i < 9; i++)
    memory.WriteU16(camera + 0x74 + i * 2, (ushort)(i % 4 == 0 ? 4096 : 0));
memory.WriteU32(stack + 0xB0, 100 * 4096);
memory.WriteU16(stack + 0xBA, unchecked((ushort)-4096));
memory.WriteU32(stack + 0xC0, face);
memory.WriteU16(face, 1);
Hit(true);
Check(Draws() && Field<float>("_height") == 100 && Field<Vector3>("_normal") == -Vector3.UnitY,
    "SM2 reads height, normal and face from its native ground query");
cpu.S3 = player + 0x1000;
Hit(false);
Check(Draws(), "NPC ground query leaves the player receiver in place");
cpu.S3 = player;
memory.WriteU32(player + 0xC10, 1);
memory.WriteU32(player + 8, unchecked((uint)(-800 * 4096)));
Check(Draws() && Vector3.Transform(new Vector3(10, -800, 20), Field<Matrix4x4>("_projection")) == new Vector3(10, 100, 20),
    "ceiling attachment projects the raised pose onto the cached room floor");
memory.WriteU16(face, 0x41);
Check(!Draws(), "SM2 semi-transparent textured receiver suppresses the silhouette");
memory.WriteU16(face, 1);
memory.WriteU32(player + 0xF3C, 0x400);
Check(Draws(), "SM1's state offset does not control SM2");
memory.WriteU32(player + 0xFB4, 0x400);
Check(!Draws(), "SM2's excluded player state suppresses drawing");
memory.WriteU32(player + 0xFB4, 0);
memory.WriteU32(player + 0xC10, 0);
Hit(false);
Check(!Draws(), "a short ground-probe miss invalidates the receiver");
Hit(true);
memory.WriteU32(camera + 8, 110);
memory.WriteU16(camera + 0x74 + 14, 4096);
var registers = cpu.Snapshot();
uint[] controls = Enumerable.Range(0, 32).Select(Gte.ReadControl).ToArray();
Check(!Draws(), "camera below the receiver and looking through it uses SM2's native ray");
Check(cpu.Snapshot().gpr.SequenceEqual(registers.gpr) && Enumerable.Range(0, 32).Select(Gte.ReadControl).SequenceEqual(controls),
    "native camera probe preserves the caller");
memory.WriteU32(camera + 8, 0);
memory.WriteU16(camera + 0x74 + 14, 0);
foreach (string name in new[] { "tr03_t", "WR03_T" })
{
    Level(name);
    Check(!Draws(), name + " requires a fresh long-probe hit even with a cached ground hit");
    Check(memory.ReadU32(stack - 0x100 + 16) - memory.ReadU32(stack - 0x100 + 4) == 8000u * 4096,
        name + " casts exactly 8000 world units down");
}
foreach (string name in new[] { "tr02_t", "wr04_t", "dem1_t", "e1m0_t" })
{
    Level(name);
    Check(Draws(), name + " uses the ordinary ground receiver");
}
cpu.A0 = cpu.S2 = player + 0x1000;
Check(PlayerShadow.UpdateBlob(cpu, memory) && !PlayerShadow.ClearPlayerQuads(cpu, memory),
    "other actors retain both native shadow paths");
cpu.A0 = player;
memory.WriteU16(player + 0x46, 0x108);
Check(!PlayerShadow.UpdateBlob(cpu, memory) && memory.ReadU16(player + 0x46) == 0x100 && cpu.SP == stack,
    "player blob cleanup preserves unrelated flags and restores the stack");
memory.WriteU8(player + 0x13C, 1);
Recompiled.SpiderMan2.func_80057BA4(cpu, memory);
Check(memory.ReadU8(player + 0x13C) == 0 && cpu.SP == stack, "retail player quad cleanup branch runs and returns");

SceneDepth.Install();
memory.WriteU32(cpu.GP + 0x1358, 1);
Gte.ProjectionDepthScale = 1f;
SceneDepth.ActorTransformEnter(cpu, memory);
Check(GpuHle.SceneDepthEnabled && Gte.ProjectionDepthScale == 1f / 16, "near actor and scenery depth share world units");
memory.WriteU32(cpu.GP + 0x1358, 0);
SceneDepth.ActorTransformEnter(cpu, memory);
Check(Gte.ProjectionDepthScale == 1, "far actor transform uses unscaled world depth");
SceneDepth.ActorTransformExit(cpu, memory);
Check(Gte.ProjectionDepthScale == 1f / 16, "nested transform restores the near actor scale");
SceneDepth.ActorTransformExit(cpu, memory);
Check(Gte.ProjectionDepthScale == 1, "scenery retains its own depth scale after actor drawing");

memory.WriteU32(cpu.GP + 0xD18, frame);
memory.WriteU32(frame + 0x70, ot);
memory.WriteU32(cpu.GP + 0xD20, packet);
memory.WriteU32(cpu.GP + 0x8D0, packet + 0x100);
memory.WriteU32(ot, 0x00FFFFFF);
Check(Draws(), "ordinary opaque receiver accepts a posed player");
Field<List<Vector3>>("Triangles").AddRange([new(0, 0, 10), new(1, 0, 20), new(2, 8, 12)]);
PlayerShadow.ActorEnd(cpu, memory);
Check(GpuPrims.Any && memory.ReadU32(ot) == packet && memory.ReadU32(packet) == 0x01FFFFFF &&
    memory.ReadU32(cpu.GP + 0xD20) == packet + 8, "SM2 native packet cursor and ordering table contain the stencil insertion");
PlayerShadow.Reset(cpu, memory);
Check(!Draws() && !GpuPrims.Any, "stage transition clears the previous room's plane and pending stencil");
Console.WriteLine($"{checks} SM2 shadow checks passed.");
