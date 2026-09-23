using System.Buffers.Binary;
using System.Diagnostics;
using System.Reflection;
using RecompOne.Runtime;
using RecompOne.Runtime.Cdrom;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;
using RecompOne.Runtime.Sdk;

int checks = 0;
void Check(bool value, string name)
{
    if (!value) throw new Exception(name);
    checks++;
}
var memory = new PSMemory();
var cpu = new CpuContext();
Runtime.SetContext(cpu, memory);
var disc = new MovieDisc();
using var fs = DiscFs.FromImage(disc);
Runtime.Cd = new CdController(fs, memory);
const uint ring = 0x80100000, parameters = 0x80110000;
double maximum = 0;
try
{
    // One slot guarantees producer backpressure until the consumer releases it.
    cpu.A0 = ring; cpu.A1 = 1;
    LibCdStream.StSetRing(cpu, memory);
    LibCdStream.StSetStream(cpu, memory);
    memory.WriteU8(parameters, 0); memory.WriteU8(parameters + 1, 2); memory.WriteU8(parameters + 2, 0);
    cpu.A0 = 2; cpu.A1 = parameters; cpu.A2 = 0;
    LibCd.CdControl(cpu, memory);
    cpu.A0 = 0x1B; cpu.A1 = 0;
    LibCd.CdControl(cpu, memory);
    Check(SpinWait.SpinUntil(() => Interlocked.Read(ref LibCdStream.FramesQueued) == 1, 3000), "first frame queued");
    Check(SpinWait.SpinUntil(() => Volatile.Read(ref disc.Reads) >= 8, 3000), "producer repeatedly encountered full ring");
    var consumer = Task.Run(() =>
    {
        for (uint expected = 0; expected < 12; expected++)
        {
            var deadline = Stopwatch.StartNew();
            do
            {
                cpu.A0 = parameters; cpu.A1 = parameters + 4;
                var call = Stopwatch.StartNew();
                LibCdStream.StGetNext(cpu, memory);
                maximum = Math.Max(maximum, call.Elapsed.TotalMilliseconds);
                if (cpu.V0 == 0) break;
                Thread.Sleep(1);
            } while (deadline.ElapsedMilliseconds < 3000);
            Check(cpu.V0 == 0, "consumer obtains frame under backpressure");
            Check(memory.ReadU32(memory.ReadU32(parameters)) == expected, "frame payload order preserved");
            Check(memory.ReadU32(memory.ReadU32(parameters + 4) + 8) == expected, "frame header matches payload");
            Thread.Sleep(5); // Let producer hit the full ring again before release.
        }
    });
    Check(consumer.Wait(15000), "consumer makes forward progress with a full ring");
    Check(LibCdStream.FramesTaken == 12, "each accepted frame taken once");
    // Hold an actual producer read across reset. Reset must not return while
    // that worker can still access the old disc/memory or rejoin a new boot.
    disc.BlockRead = true;
    Check(disc.ReadEntered.Wait(3000), "producer read held across reset");
    var streamType = typeof(LibCdStream);
    var oldWorker = (Thread)streamType.GetField("_thread", BindingFlags.NonPublic | BindingFlags.Static)!.GetValue(null)!;
    var resetStarted = new ManualResetEventSlim();
    var reset = Task.Run(() =>
    {
        resetStarted.Set();
        streamType.GetMethod("Reset", BindingFlags.NonPublic | BindingFlags.Static)!.Invoke(null, null);
    });
    resetStarted.Wait();
    bool returnedDuringRead = reset.Wait(100);
    disc.ReadRelease.Set();
    Check(reset.Wait(3000), "reset completes after read release");
    bool workerJoined = !oldWorker.IsAlive;
    oldWorker.Join(3000); // Also keep a failing pre-fix test safe to exit.
    Check(!returnedDuringRead, "reset waits for in-flight producer before clearing state");
    Check(workerJoined, "reset returns with old worker terminated");
    cpu.A0 = ring; cpu.A1 = 1;
    LibCdStream.StSetRing(cpu, memory);
    Check(!oldWorker.IsAlive, "starting a new ring cannot revive the old worker");
    LibCdStream.StSetStream(cpu, memory);
    cpu.A0 = 2; cpu.A1 = parameters; cpu.A2 = 0;
    memory.WriteU8(parameters, 0); memory.WriteU8(parameters + 1, 2); memory.WriteU8(parameters + 2, 0);
    LibCd.CdControl(cpu, memory);
    cpu.A0 = 0x1B; cpu.A1 = 0;
    LibCd.CdControl(cpu, memory);
    Check(SpinWait.SpinUntil(() => Interlocked.Read(ref LibCdStream.FramesQueued) == 1, 3000), "new boot queues a fresh frame");
    cpu.A0 = parameters; cpu.A1 = parameters + 4;
    LibCdStream.StGetNext(cpu, memory);
    Check(cpu.V0 == 0 && memory.ReadU32(memory.ReadU32(parameters)) == 0, "new boot starts at requested frame without stale payload");
    disc.ReadRelease.Reset();
    disc.ReadEntered.Reset();
    Check(disc.ReadEntered.Wait(3000), "producer read held across movie teardown");
    var teardownWorker = (Thread)streamType.GetField("_thread", BindingFlags.NonPublic | BindingFlags.Static)!.GetValue(null)!;
    using var teardownStarted = new ManualResetEventSlim();
    var teardown = Task.Run(() =>
    {
        teardownStarted.Set();
        LibCdStream.StUnSetRing(cpu, memory);
    });
    teardownStarted.Wait();
    bool teardownReturnedDuringRead = teardown.Wait(100);
    disc.ReadRelease.Set();
    Check(teardown.Wait(3000), "movie teardown completes after read release");
    bool teardownJoined = !teardownWorker.IsAlive;
    // Stop the pre-fix idle worker before reporting a failing assertion.
    streamType.GetMethod("Reset", BindingFlags.NonPublic | BindingFlags.Static)!.Invoke(null, null);
    Check(!teardownReturnedDuringRead, "movie teardown waits for producer before returning ring memory");
    Check(teardownJoined, "movie teardown leaves no live producer");
    foreach (string operation in new[] { "replace", "clear", "stream" })
    {
        // Hold the collection read after the worker has selected the old ring.
        // A transition must retire that owner before resetting/replacing memory.
        disc.BlockRead = false;
        disc.ReadEntered.Reset();
        disc.ReadRelease.Reset();
        disc.BlockAtRead = Volatile.Read(ref disc.Reads) + 2;
        cpu.A0 = ring; cpu.A1 = 1;
        LibCdStream.StSetRing(cpu, memory);
        LibCdStream.StSetStream(cpu, memory);
        memory.WriteU8(parameters, 0); memory.WriteU8(parameters + 1, 2); memory.WriteU8(parameters + 2, 0);
        cpu.A0 = 2; cpu.A1 = parameters; cpu.A2 = 0;
        LibCd.CdControl(cpu, memory);
        cpu.A0 = 0x1B; cpu.A1 = 0;
        LibCd.CdControl(cpu, memory);
        Check(disc.ReadEntered.Wait(3000), $"{operation}: collection read held");
        var transitionWorker = (Thread)streamType.GetField("_thread", BindingFlags.NonPublic | BindingFlags.Static)!.GetValue(null)!;
        using var transitionStarted = new ManualResetEventSlim();
        var transition = Task.Run(() =>
        {
            var transitionCpu = new CpuContext { A0 = ring + 0x4000, A1 = 2 };
            transitionStarted.Set();
            if (operation == "replace") LibCdStream.StSetRing(transitionCpu, memory);
            else if (operation == "clear") LibCdStream.StClearRing(transitionCpu, memory);
            else LibCdStream.StSetStream(transitionCpu, memory);
        });
        transitionStarted.Wait();
        bool returnedDuringCollection = transition.Wait(100);
        disc.ReadRelease.Set();
        Check(transition.Wait(3000), $"{operation}: transition finishes after read release");
        bool retired = !transitionWorker.IsAlive;
        if (returnedDuringCollection || !retired)
        {
            LibCdStream.StUnSetRing(cpu, memory);
            Check(false, $"{operation}: transition must retire the in-flight ring owner");
        }
        Check(retired, $"{operation}: old producer retired");
        Check(SpinWait.SpinUntil(() =>
        {
            cpu.A0 = parameters; cpu.A1 = parameters + 4;
            LibCdStream.StGetNext(cpu, memory);
            return cpu.V0 == 0;
        }, 3000), $"{operation}: replacement producer delivers a frame");
        uint expectedRing = operation == "replace" ? ring + 0x4000 : ring;
        uint expectedData = expectedRing + (operation == "replace" ? 64u : 32u);
        Check(memory.ReadU32(parameters) == expectedData, $"{operation}: frame uses current ring layout");
        Check(memory.ReadU32(parameters + 4) == expectedRing, $"{operation}: header uses current ring");
        Check(memory.ReadU32(expectedData) == 0 && memory.ReadU32(expectedRing + 8) == 0,
            $"{operation}: aborted frame retried with matching header and payload");
        LibCdStream.StUnSetRing(cpu, memory);
        streamType.GetMethod("Reset", BindingFlags.NonPublic | BindingFlags.Static)!.Invoke(null, null);
    }
    Console.WriteLine($"PASS: {checks} movie streaming assertions; maximum StGetNext call {maximum:F3} ms");
}
finally
{
    disc.ReadRelease.Set();
    LibCdStream.StUnSetRing(cpu, memory);
    var type = typeof(LibCdStream);
    var worker = (Thread?)type.GetField("_thread", BindingFlags.NonPublic | BindingFlags.Static)!.GetValue(null);
    type.GetMethod("Reset", BindingFlags.NonPublic | BindingFlags.Static)!.Invoke(null, null);
    worker?.Join(3000);
    Runtime.Cd = null;
}

sealed class MovieDisc : IDiscImage
{
    public int Reads;
    public volatile bool BlockRead;
    public int BlockAtRead = int.MaxValue;
    public readonly ManualResetEventSlim ReadEntered = new();
    public readonly ManualResetEventSlim ReadRelease = new();
    public string Format => "synthetic video";
    public int FirstTrack => 1;
    public int LastTrack => 1;
    public bool HasTracks => true;
    public int LeadoutLba => 100000;
    public int DataSectors => LeadoutLba;
    public IReadOnlyList<DiscTrack> Tracks => [new(1, DiscTrackKind.Data, 0, 2352)];
    public bool TrackStartLba(int track, out int lba) { lba = 0; return track == 1; }
    public byte[] ReadSectorData(int lba, int size)
    {
        int read = Interlocked.Increment(ref Reads);
        if (BlockRead || read == Volatile.Read(ref BlockAtRead))
        {
            ReadEntered.Set();
            if (!ReadRelease.Wait(5000)) throw new TimeoutException("test read gate");
        }
        var sector = new byte[size];
        BinaryPrimitives.WriteUInt16LittleEndian(sector.AsSpan(8), 0x0160);
        BinaryPrimitives.WriteUInt16LittleEndian(sector.AsSpan(14), 1);
        BinaryPrimitives.WriteInt32LittleEndian(sector.AsSpan(16), lba);
        BinaryPrimitives.WriteInt32LittleEndian(sector.AsSpan(40), lba);
        return sector;
    }
    public void Dispose() { }
}
