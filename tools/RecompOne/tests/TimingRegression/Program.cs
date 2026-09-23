using System.Diagnostics;
using System.Reflection;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Memory;
using RecompOne.Runtime.Sdk;
using RecompOne.Runtime.Cdrom;
using RecompOne.Runtime.Dispatch;
using RecompOne.Runtime.Bios;
using RecompOne.Runtime.Hardware;

void Check(bool condition, string name)
{
    if (!condition) throw new Exception(name);
    Console.WriteLine("PASS " + name);
}
var memory = new PSMemory();
var cpu = new CpuContext();
Runtime.SetContext(cpu, memory);
cpu.S0 = 0x12345678;
BiosB.IntrEnvInInterruptAddr = 0x80011000;
memory.WriteU32(0x80011002, 0x80012000);
Dispatcher.Register("test-irq", new TestIrq());
Dispatcher.Load("test-irq");
Runtime.VBlanksPerFrame = 1;
Check(Runtime.VBlanksPerFrame == 2, "presentation cannot exceed 30 FPS");
long lastVBlank = 0;
int edges = 0;
var presentationTimes = new List<double>();
var clock = Stopwatch.StartNew();
long lastPresentation = Runtime.Presents;
Event.AddListener<VSyncEvent>(e =>
{
    Check(e.Frame == lastVBlank + 1, "one counter edge per console interrupt");
    Check(memory.ReadU32(0x80013000) == e.Frame, "game IRQ counter observes exactly one edge");
    lastVBlank = e.Frame;
    edges++;
    if (Runtime.Presents != lastPresentation)
    {
        presentationTimes.Add(clock.Elapsed.TotalMilliseconds);
        lastPresentation = Runtime.Presents;
    }
});
for (int i = 0; i < 12; i++) LibEtc.Pump(cpu, memory);
Check(edges == 12 && Runtime.Presents == 6, "two separately observable vblanks per presentation");
Check(cpu.S0 == 0x12345678, "interrupt delivery preserves interrupted CPU registers");
Console.WriteLine("Presentation completion intervals: " + string.Join(", ", presentationTimes.Zip(presentationTimes.Skip(1)).Select(p => (p.Second - p.First).ToString("F3"))));
Check(presentationTimes.Skip(1).Zip(presentationTimes.Skip(2)).All(p => p.Second - p.First >= 32.5), "warmed presentation intervals remain at least 1/30 second");
long before = Runtime.Presents;
for (int i = 0; i < 50; i++) Runtime.ServiceOnly();
Check(Runtime.Presents == before && edges == 12, "service passes do not invent frames or interrupts");
Thread.Sleep(100);
LibEtc.Pump(cpu, memory);
LibEtc.Pump(cpu, memory);
double recovery = clock.Elapsed.TotalMilliseconds;
LibEtc.Pump(cpu, memory);
LibEtc.Pump(cpu, memory);
Check(clock.Elapsed.TotalMilliseconds - recovery >= 32.5, "a stall cannot cause catch-up bursts");

var frameClock = typeof(Runtime).Assembly.GetType("RecompOne.Runtime.Host.FrameClock")!;
frameClock.GetProperty("VSync")!.SetValue(null, true);
Action throttle = frameClock.GetMethod("Throttle")!.CreateDelegate<Action>();
throttle();
double first = clock.Elapsed.TotalMilliseconds;
throttle();
Check(clock.Elapsed.TotalMilliseconds - first >= 32.5, "monitor vsync cannot bypass the 30 FPS cap");

uint counterBefore = memory.ReadU32(0x80013000);
cpu.A0 = 2;
LibEtc.WaitCounter(cpu, memory, 0x80013000);
Check(memory.ReadU32(0x80013000) == counterBefore + 2 && cpu.V0 == 0 && cpu.V1 == counterBefore + 2,
    "yielding game wait observes two real IRQs and preserves retail return values");
counterBefore = memory.ReadU32(0x80013000);
cpu.A0 = 0;
LibEtc.WaitCounter(cpu, memory, 0x80013000);
Check(memory.ReadU32(0x80013000) == counterBefore && cpu.V1 == counterBefore,
    "zero-length game wait does not invent a tick");

// Exercise the real game's timing hooks without any follower/proof environment.
// A late first edge must not add its lateness to the second edge's deadline.
// The old clock needed ~41.7 ms for this 25 ms workload, despite a 33.3 ms budget.
for (int warm = 0; warm < 4; warm++) { LibEtc.Pump(cpu, memory); LibEtc.Pump(cpu, memory); }
var loadedClock = Stopwatch.StartNew();
for (int frame = 0; frame < 30; frame++)
{
    long workStart = Stopwatch.GetTimestamp();
    while (Stopwatch.GetElapsedTime(workStart).TotalMilliseconds < 25) Thread.SpinWait(32);
    LibEtc.Pump(cpu, memory); LibEtc.Pump(cpu, memory);
}
double loadedInterval = loadedClock.Elapsed.TotalMilliseconds / 30;
Check(loadedInterval < 36, $"25 ms frame work retains 30 FPS headroom ({loadedInterval:F2} ms average)");
var overloadedClock = Stopwatch.StartNew();
for (int frame = 0; frame < 20; frame++)
{
    long workStart = Stopwatch.GetTimestamp();
    while (Stopwatch.GetElapsedTime(workStart).TotalMilliseconds < 40) Thread.SpinWait(32);
    LibEtc.Pump(cpu, memory); LibEtc.Pump(cpu, memory);
}
double overloadedInterval = overloadedClock.Elapsed.TotalMilliseconds / 20;
Check(overloadedInterval < 46, $"40 ms frame work does not incur an extra IRQ wait ({overloadedInterval:F2} ms average)");

const uint player = 0x80100000, script = 0x80110000, levelName = 0x80120000;
void LoadTimingLevel(string name)
{
    for (int i = 0; i <= name.Length; i++)
        memory.WriteU8(levelName + (uint)i, i == name.Length ? (byte)0 : (byte)name[i]);
    cpu.A0 = levelName;
    Recompiled.Timing.LoadTriggers(cpu, memory);
}
void StartTimingScript()
{
    cpu.A0 = player; cpu.A1 = script;
    Recompiled.Timing.StartPlayerScript(cpu, memory);
}
uint WaitGame(uint returnAddress = 0x8002C294)
{
    cpu.A0 = 1; cpu.RA = returnAddress; cpu.GP = 0x8001238C;
    uint previous = memory.ReadU32(0x80013000);
    Recompiled.Timing.WaitVBlanks(cpu, memory);
    return memory.ReadU32(0x80013000) - previous;
}
// Exact bytes at A1=800E2B3E in the native Chase initializer trace.
ushort[] chasePrefix = [0, 17, 30, 3, 10, 60, 520, 1, 54, 20, 3, 16, 2];
for (int i = 0; i < chasePrefix.Length; i++) memory.WriteU16(script + (uint)i * 2, chasePrefix[i]);
memory.WriteU32(0x800B5268, player);
memory.WriteU32(player + 0x1A8, 1);
LoadTimingLevel("l5a3_t"); StartTimingScript();
Check(WaitGame() == 1, "other levels retain normal gameplay cadence");
LoadTimingLevel("l5a1_t"); StartTimingScript();
Check(WaitGame(0x8002C274) == 1 && WaitGame() == 2,
    "Chase building script advances three IRQs through the two native waits");
memory.WriteU32(player + 0x1A8, 0);
Check(WaitGame() == 1, "completed or skipped cutscene restores normal cadence");
memory.WriteU32(player + 0x1A8, 1);
Check(WaitGame() == 1, "later scripts do not inherit completed cutscene cadence");
StartTimingScript(); LoadTimingLevel("l5a1_t");
Check(WaitGame() == 1, "level retry clears cutscene cadence");
StartTimingScript(); memory.WriteU16(script + 16, 55); StartTimingScript();
Check(WaitGame() == 1, "different player script clears cutscene cadence");
memory.WriteU16(script + 16, 54); StartTimingScript();
memory.WriteU32(0x800B5268, player + 0x1000);
Check(WaitGame() == 1, "replacement player cannot inherit cutscene cadence");

// ReadS must select the XA reader when no active movie ring exists, even after
// a previous movie initialized the ring. No disc or audio device is needed.
bool ReadingXa() => (bool)typeof(LibCd).GetField("_readActive", BindingFlags.Static | BindingFlags.NonPublic)!.GetValue(null)!;
cpu.A0 = 0x1B; cpu.A1 = 0; cpu.A2 = 0;
LibCd.CdControl(cpu, memory);
Check(ReadingXa(), "ReadS without a movie starts XA reading");
LibCdStream.StSetStream(cpu, memory);
cpu.A0 = 0x1B;
LibCd.CdControl(cpu, memory);
Check(!ReadingXa(), "ReadS with an active movie preserves movie streaming");
LibCdStream.StUnSetRing(cpu, memory);
cpu.A0 = 0x1B;
LibCd.CdControl(cpu, memory);
Check(ReadingXa() && !LibCdStream.InUse, "ReadS after a movie returns to XA reading");
cpu.A0 = 0x09;
LibCd.CdControl(cpu, memory);
Check(!ReadingXa(), "Pause stops XA reading");
// Movie teardown calls the blocking variant directly. It must stop the shared
// reader and return completion without polling untouched retail IRQ variables.
LibCdStream.StSetStream(cpu, memory);
cpu.A0 = 0x1B; cpu.A1 = 0; cpu.A2 = 0;
LibCd.CdControl(cpu, memory);
LibCdStream.StUnSetRing(cpu, memory);
const uint blockingResult = 0x80130000;
memory.WriteU32(blockingResult, 0xFFFFFFFF);
cpu.A0 = 0x09; cpu.A1 = 0; cpu.A2 = blockingResult;
LibCd.CdControlB(cpu, memory);
Check(cpu.V0 == 1 && !ReadingXa(), "blocking movie Pause completes and stops reading");
Check(memory.ReadU8(blockingResult) == 0x02, "blocking Pause returns motor-on status without read/play bits");
cpu.A0 = 0; cpu.A1 = blockingResult;
LibCd.CdSync(cpu, memory);
Check(cpu.V0 == 2, "blocking movie Pause leaves complete status for subsequent CD synchronization");
// Both games poll the SDK command query when deciding whether the next dialogue
// clip may start. The query must expose HLE state without retail RAM mirrors.
foreach (uint command in new uint[] { 0x16, 0x11, 0x09 })
{
    cpu.A0 = command; cpu.A1 = 0; cpu.A2 = 0;
    LibCd.CdControl(cpu, memory);
    cpu.V0 = 0xDEADBEEF;
    LibCd.CdLastCom(cpu, memory);
    Check(cpu.V0 == command, $"CD command query follows 0x{command:X2} for dialogue sequencing");
}
foreach (uint seek in new uint[] { 0x15, 0x16 })
{
    cpu.A0 = 0x1B;
    LibCd.CdControl(cpu, memory);
    cpu.A0 = seek;
    LibCd.CdControl(cpu, memory);
    Check(!ReadingXa(), $"seek 0x{seek:X2} finishes paused");
}

// Exercise the actual mixer: mute must silence CD samples while still consuming
// them, and demute must restore the signal without restarting the stream.
var spu = new Spu();
spu.WriteReg16(0x1F801D80, 0x3FFF);
spu.WriteReg16(0x1F801D82, 0x3FFF);
spu.WriteReg16(0x1F801DB0, 0x7FFF);
spu.WriteReg16(0x1F801DB2, 0x7FFF);
spu.SetCdMix(0x80, 0, 0x80, 0);
XaAudio.Reset();
var signal = Enumerable.Repeat(0x20002000, 4096).ToArray();
XaAudio.PushFrames(signal, signal.Length, 44100);
var pcm = new short[512];
// Allow the fixed stream-start jitter buffer to prime before testing mute.
for (int i = 0; i < 8; i++) spu.Mix(pcm, 256);
cpu.A0 = 0x0B;
LibCd.CdControl(cpu, memory);
int buffered = XaAudio.BufferedSamples;
spu.Mix(pcm, 256);
Check(pcm.All(s => s == 0) && XaAudio.BufferedSamples < buffered, "Mute silences CD output while decoding continues");
cpu.A0 = 0x0C;
LibCd.CdControl(cpu, memory);
spu.Mix(pcm, 256);
Check(pcm.Any(s => s != 0), "Demute restores CD audio from the ongoing stream");

// Model a 37.8 kHz mono XA packet every 106.7 ms, with the second and later
// packets arriving 6.5 ms late (the gap observed in the title music capture).
// After the one-time startup silence, the actual resampler must emit a
// continuous signal without inserting a gap or dropping source frames.
XaAudio.Reset();
var source = Enumerable.Range(0, 40320).Select(i =>
{
    int sample = 2000 + i % 1000;
    return sample | (sample << 16);
}).ToArray();
XaAudio.PushFrames(source, source.Length, 37800);
var expectedAudio = new short[44100];
for (int n = 0; n < expectedAudio.Length; n++)
    XaAudio.Next(out expectedAudio[n], out _);
XaAudio.Reset();
XaAudio.PushFrames(source[..4032], 4032, 37800);
var streamedAudio = new short[44100];
int packetIndex = 1;
for (int n = 0; n < 44100; n++)
{
    if (n >= 4704 + 287 && (n - 287) % 4704 == 0)
    {
        XaAudio.PushFrames(source[(packetIndex * 4032)..((packetIndex + 1) * 4032)], 4032, 37800);
        packetIndex++;
    }
    XaAudio.Next(out streamedAudio[n], out _);
}
int firstSignal = Array.FindIndex(streamedAudio, s => s != 0);
Check(firstSignal > 0 && firstSignal < 2205 && streamedAudio.SequenceEqual(expectedAudio),
    "XA startup buffering preserves every resampled frame across a late packet");

XaAudio.Reset();
var disc = new FilteredDisc();
using var fs = DiscFs.FromImage(disc);
Runtime.Cd = new CdController(fs, memory);
const uint parameters = 0x80010000;
foreach (byte mode in new byte[] { 0x48, 0xC8 })
{
    memory.WriteU8(parameters, mode);
    cpu.A0 = 0x0E; cpu.A1 = parameters;
    LibCd.CdControl(cpu, memory);
    memory.WriteU8(parameters, 0); memory.WriteU8(parameters + 1, 2);
    memory.WriteU8(parameters + 2, 0); memory.WriteU8(parameters + 3, 0);
    int reads = disc.Reads;
    var elapsed = Stopwatch.StartNew();
    cpu.A0 = 0x1B;
    LibCd.CdControl(cpu, memory);
    Thread.Sleep(250);
    cpu.A0 = 0x16; cpu.A1 = 0;
    LibCd.CdControl(cpu, memory);
    double expected = elapsed.Elapsed.TotalSeconds * ((mode & 0x80) != 0 ? 150 : 75);
    int sectors = disc.Reads - reads;
    Check(sectors > 0 && sectors <= expected + 2, $"filtered XA obeys {(mode == 0x48 ? 75 : 150)} sectors/s ({sectors} sectors)");
    reads = disc.Reads;
    Thread.Sleep(25);
    Check(disc.Reads == reads, "seek atomically stops background XA reads");
}
// A reset must retire the real XA producer before disc/memory replacement.
disc.BlockRead = true;
cpu.A0 = 0x1B; cpu.A1 = 0; cpu.A2 = 0;
LibCd.CdControl(cpu, memory);
Check(disc.ReadEntered.Wait(3000), "XA producer read held across reset");
var cdType = typeof(LibCd);
var oldXaWorker = (Thread)cdType.GetField("_xaThread", BindingFlags.NonPublic | BindingFlags.Static)!.GetValue(null)!;
using var resetStarted = new ManualResetEventSlim();
var xaReset = Task.Run(() =>
{
    resetStarted.Set();
    cdType.GetMethod("Reset", BindingFlags.NonPublic | BindingFlags.Static)!.Invoke(null, null);
});
resetStarted.Wait();
bool resetReturnedDuringRead = xaReset.Wait(100);
disc.ReadRelease.Set();
Check(xaReset.Wait(3000), "XA reset completes after disc read releases");
bool xaWorkerJoined = !oldXaWorker.IsAlive;
oldXaWorker.Join(3000); // Clean up even when checking the pre-fix implementation.
Check(!resetReturnedDuringRead, "XA reset waits for in-flight worker before clearing state");
Check(xaWorkerJoined, "XA reset returns with the old worker terminated");
int stoppedReads = disc.Reads;
Thread.Sleep(25);
Check(disc.Reads == stoppedReads && !ReadingXa(), "reset leaves XA stopped");
memory.WriteU8(parameters, 0x48);
cpu.A0 = 0x0E; cpu.A1 = parameters;
LibCd.CdControl(cpu, memory);
memory.WriteU8(parameters, 0); memory.WriteU8(parameters + 1, 2);
memory.WriteU8(parameters + 2, 0);
cpu.A0 = 0x1B; cpu.A1 = parameters;
LibCd.CdControl(cpu, memory);
Check(SpinWait.SpinUntil(() => Volatile.Read(ref disc.Reads) > stoppedReads, 3000), "new XA worker reads after restart");
Check(!oldXaWorker.IsAlive, "XA restart cannot revive old worker");
cdType.GetMethod("Reset", BindingFlags.NonPublic | BindingFlags.Static)!.Invoke(null, null);
Runtime.Cd = null;

// Script edges must reach the real libpad buffer during their requested tick,
// not depend on an extra host event poll between native game updates.
const uint inputBuffer = 0x80140000;
cpu.A0 = inputBuffer; cpu.A1 = 0;
LibPad.PadInitDirect(cpu, memory);
Controller.ScriptExclusive = true;
ushort[] scriptEdges = [Controller.Cross, 0, Controller.Right];
int prepared = 0;
bool prepBeforeTick = true;
Action<VSyncInputEvent> prepareInput = e =>
{
    prepBeforeTick &= e.Frame == lastVBlank + 1;
    Controller.ScriptHeld = scriptEdges[prepared++];
    Controller.ApplyInputOverrides();
};
Event.AddListener(prepareInput);
try
{
    for (int i = 0; i < scriptEdges.Length; i++)
    {
        LibEtc.Pump(cpu, memory);
        Check(memory.ReadU16(inputBuffer + 2) == (ushort)~scriptEdges[i],
            "script press/release sampled in the requested console tick");
    }
    Check(prepared == 3 && prepBeforeTick, "input preparation precedes exactly one tick per request");
}
finally
{
    Event.RemoveListener(prepareInput);
    Controller.ScriptHeld = 0;
    Controller.ApplyInputOverrides();
    Controller.ScriptExclusive = false;
}

var nativeInput = new Recompiled.NativeInputSchedule();
nativeInput.Add(2, 2, Controller.Cross);
nativeInput.Add(3, 2, Controller.Up);
Check(nativeInput.Sample(2) == 0, "native input requires archive arming");
nativeInput.Arm();
Check(nativeInput.Sample(20) == 0, "native input waits for new zero counter epoch");
Check(nativeInput.Sample(0) == 0 && nativeInput.Sample(1) == 0, "native input preserves pre-roll");
Check(nativeInput.Sample(2) == Controller.Cross && nativeInput.Sample(2) == Controller.Cross,
    "repeated console ticks cannot shorten native hold");
Check(nativeInput.Sample(3) == (Controller.Cross | Controller.Up), "native overlapping buttons combine");
Check(nativeInput.Sample(4) == Controller.Up && nativeInput.Sample(5) == 0, "native release uses update boundary");
Check(nativeInput.Sample(0) == 0, "counter restart releases native input");
nativeInput.Arm();
Check(nativeInput.Sample(2) == 0, "later archive load cannot replay finished epoch");

sealed class TestIrq : IOverlay
{
    public string Name => "test-irq";
    public IReadOnlyDictionary<uint, Action<CpuContext, IMemory>> Functions { get; } =
        new Dictionary<uint, Action<CpuContext, IMemory>>
        {
            [0x80012000] = (c, m) =>
            {
                m.WriteU32(0x80013000, m.ReadU32(0x80013000) + 1);
                c.S0 = 99;
            }
        };
}
