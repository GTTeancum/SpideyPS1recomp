using Recompiled;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;

#if SM2
const uint playerPointer = 0x800C21F4, cameraPointer = 0x800C2994, paused = 0x800C1880;
const uint active = 0xDC, mode = 0x288, target = 0xE4, script = 0x190, yawOffset = 0x21E;
const uint tween = 0x1540, viewerYaw = 0x800B327E;
const uint gp = 0x800C1764, followTarget = 0xEC, desiredPosition = 0x234, distance = 0x1228;
const string discExe = "spiderman2/extracted/SLUS_013.78";
#else
const uint playerPointer = 0x800B5268, cameraPointer = 0x800B5834, paused = 0x800B4990;
const uint active = 0xF4, mode = 0x2A0, target = 0xFC, script = 0x1A8, yawOffset = 0x236;
const uint tween = 0x136C, viewerYaw = 0x800A5786;
const uint gp = 0x800B47F4, followTarget = 0x104, desiredPosition = 0x24C, distance = 0x1038;
const string discExe = "spiderman/extracted/SLUS_008.75";
#endif
int checks = 0;
void Check(bool ok, string name)
{
    if (!ok) throw new Exception(name);
    checks++;
    Console.WriteLine("PASS: " + name);
}

// No Runtime.Run, window, audio device, SDL polling or operating-system input.
foreach (byte axis in new byte[] { 103, 127, 128, 152 })
{
    var stick = new StickOrbit();
    Check(!stick.Update(axis, 0, 1, 100, out var angle) && angle == 100,
        $"dead zone axis={axis}");
}
foreach (byte axis in new byte[] { 0, 255 })
{
    var stick = new StickOrbit();
    ushort angle = 0;
    for (int i = 0; i < 30; i++) stick.Update(axis, 2 * i, 1, angle, out angle);
    Check(angle == (axis == 255 ? 1024 : 3072), $"90 degrees in one second axis={axis}");
    for (int i = 30; i < 120; i++) stick.Update(axis, 2 * i, 1, angle, out angle);
    Check(angle == 0, $"full orbit wraps axis={axis}");
}
var timing = new StickOrbit();
foreach (int cadence in new[] { 1, 2, 3 })
{
    var fast = new StickOrbit(180);
    fast.Update(255, 0, 1, 0, out ushort start);
    ushort angle = start;
    for (int frame = cadence; frame <= 60; frame += cadence)
        fast.Update(255, frame, 1, angle, out angle);
    Check(((angle - start) & 4095) == 2048,
        $"gameplay makes a half turn per second at {60 / cadence} updates/s");
}
timing.Update(255, 0, 1, 0, out var first);
timing.Update(255, 0, 1, first, out var duplicate);
Check(first == duplicate, "duplicate presentation cannot speed up orbit");
timing.Update(255, 1000, 1, 100, out var resumed);
Check(resumed == 134, "resume reseeds without a large time jump");
timing.Update(255, 1002, 2, 2000, out var changed);
Check(changed == 2034, "new camera starts from its own yaw");
var gentle = new StickOrbit();
gentle.Update(153, 0, 1, 0, out var slow);
gentle.Update(153, 2, 1, slow, out slow);
Check(slow == 1, "small deflection accumulates fractional rotation");
gentle.Update(128, 4, 1, 300, out _);
gentle.Update(255, 6, 1, 300, out var fresh);
Check(fresh == 334, "release returns heading ownership to game");

var memory = new PSMemory(0x800000);
const uint camera = 0x80100000, player = 0x80110000;
var cpu = new CpuContext { S1 = camera, GP = gp, SP = 0x807EF000 };
memory.WriteU32(playerPointer, player);
memory.WriteU32(cameraPointer, camera);
memory.WriteU32(camera + active, 1);
memory.WriteU32(camera + mode, 3);
memory.WriteU32(camera + target, player);
memory.WriteU16(camera + yawOffset, 4090);
memory.WriteU32(cpu.GP + tween, 20);
Controller.RightX = 255;
Diag.Frame = 0;
var registers = cpu.Snapshot();
RightStickOrbit.Gameplay(cpu, memory);
Check(memory.ReadU16(camera + yawOffset) == 62 && memory.ReadU32(cpu.GP + tween) == 0,
    "gameplay rotates through zero and cancels auto-follow tween");
Check(cpu.Snapshot().gpr.SequenceEqual(registers.gpr), "instruction hook preserves CPU registers");
// Simulate native auto-follow trying to replace the yaw during the next update.
memory.WriteU16(camera + yawOffset, 1000);
Diag.Frame = 2;
RightStickOrbit.Gameplay(cpu, memory);
Check(memory.ReadU16(camera + yawOffset) == 131, "held stick retains manual yaw against auto-follow");
Controller.RightX = 128;
memory.WriteU16(camera + yawOffset, 400);
memory.WriteU32(cpu.GP + tween, 7);
Diag.Frame = 4;
RightStickOrbit.Gameplay(cpu, memory);
Check(memory.ReadU16(camera + yawOffset) == 400 && memory.ReadU32(cpu.GP + tween) == 7,
    "neutral leaves native yaw and tween untouched");

foreach (uint cameraMode in Enumerable.Range(0, 21).Select(i => (uint)i).Where(i => i != 3))
{
    Controller.RightX = 0;
    memory.WriteU32(camera + mode, cameraMode);
    RightStickOrbit.Gameplay(cpu, memory);
    Check(memory.ReadU16(camera + yawOffset) == 400 && memory.ReadU32(cpu.GP + tween) == 7,
        $"scripted/special camera mode {cameraMode} untouched");
}
memory.WriteU32(camera + mode, 3);
foreach (var (address, blockedValue, normalValue, reason) in new (uint, uint, uint, string)[]
{
    (paused, 1, 0, "paused"),
    (player + script, 1, 0, "player script"),
    (camera + target, player + 0x1000, player, "another target"),
    (camera + active, 0, 1, "inactive camera"),
    (cameraPointer, camera + 0x1000, camera, "another camera"),
    (playerPointer, 0, player, "no player"),
})
{
    memory.WriteU32(address, blockedValue);
    RightStickOrbit.Gameplay(cpu, memory);
    Check(memory.ReadU16(camera + yawOffset) == 400, reason + " cannot orbit");
    memory.WriteU32(address, normalValue);
}
memory.WriteU16(viewerYaw, 2);
Controller.RightX = 0;
Diag.Frame = 10;
cpu.S6 = 450;
Check(RightStickOrbit.Viewer(cpu, memory), "held viewer stick bypasses native rotation input");
#if SM2
Check(cpu.S0 == unchecked((uint)-450), "viewer branch preserves zoom radius in S0");
#else
Check(cpu.A1 == unchecked((uint)-450), "viewer branch preserves zoom radius in A1");
#endif
Check(memory.ReadU16(viewerYaw) == 4064, "viewer orbits left through zero");
Controller.RightX = 128;
Check(!RightStickOrbit.Viewer(cpu, memory), "neutral viewer stick retains native controls");
memory.WriteU16(viewerYaw, 1000); // Existing viewer buttons remain authoritative.
Controller.RightX = 255;
Diag.Frame = 12;
RightStickOrbit.Viewer(cpu, memory);
Check(memory.ReadU16(viewerYaw) == 1034, "viewer right starts from native heading");
Check(memory.ReadU16(camera + yawOffset) == 400, "viewer does not modify gameplay camera");
Controller.ReplayActive = true;
Controller.ReplayRightX = 0;
Controller.ApplyInputOverrides();
Check(Controller.RightX == 0, "process-local replay supplies right-stick axis");
Controller.ReplayActive = false;
Controller.ScriptExclusive = true;
Controller.ApplyInputOverrides();
Check(Controller.RightX == 128, "exclusive neutral input cannot inherit physical stick");

if (args.Length > 0)
{
    // Optional private retail tables: execute the real follow-camera geometry routine,
    // without entering the game loop, collision scene, renderer or audio runtime.
    byte[] exe = File.ReadAllBytes(Path.Combine(args[0], discExe));
    uint loadAddress = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(exe.AsSpan(0x18));
    memory.LoadBytes(loadAddress, exe.AsSpan(2048).ToArray());
    memory.WriteU32(cpu.GP + distance, 300);
    memory.WriteU32(cpu.GP + distance - 4, 0); // Horizontal orbit, no pitch.
    var positions = new List<(int X, int Y, int Z)>();
    foreach (ushort angle in new ushort[] { 0, 1024, 2048, 3072 })
    {
        memory.WriteU16(camera + yawOffset - 2, 0);
        memory.WriteU16(camera + yawOffset, angle);
        memory.WriteU16(camera + yawOffset + 2, 0);
        memory.WriteU32(camera + followTarget, 1000 << 12);
        memory.WriteU32(camera + followTarget + 4, 200 << 12);
        memory.WriteU32(camera + followTarget + 8, 3000 << 12);
        cpu.A0 = camera;
        cpu.SP = 0x807EF000;
#if SM2
        Recompiled.SpiderMan2.func_8007E7BC(cpu, memory);
#else
        Recompiled.SpiderMan.func_80072428(cpu, memory);
#endif
        var position = (
            X: (int)memory.ReadU32(camera + desiredPosition) - (1000 << 12),
            Y: (int)memory.ReadU32(camera + desiredPosition + 4) - (200 << 12),
            Z: (int)memory.ReadU32(camera + desiredPosition + 8) - (3000 << 12));
        positions.Add(position);
        double radius = Math.Sqrt((double)position.X * position.X + (double)position.Z * position.Z) / 4096;
        Check(Math.Abs(radius - 300) < 1 && position.Y == 0,
            $"native camera yaw={angle}: offset={position}, radius={radius:F2}");
        Check(cpu.SP == 0x807EF000, "native camera routine restores its stack");
    }
    Check(positions[0].X == -positions[2].X && positions[0].Z == -positions[2].Z &&
        positions[1].X == -positions[3].X && positions[1].Z == -positions[3].Z &&
        positions[0] != positions[1], "native yaw visits opposite sides of Spider-Man");
}
Console.WriteLine($"{checks} orbit checks passed; no game window or audio was initialized.");
