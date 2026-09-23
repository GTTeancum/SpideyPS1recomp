using System.Buffers.Binary;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using System.Threading.Channels;

namespace RecompOne.Runtime.Diagnostics;

/// <summary>Bounded, local performance evidence. No file work on the game thread.</summary>
public static class PerformanceLog
{
    public static uint GameCounterAddress;
    public static string Renderer = "unavailable", Backend = "unavailable";
    public static long Dropped;
    static Channel<Sample>? _queue;
    static Thread? _writer;
    static long _previousStart;
    static uint _previousGameCounter;
    static int _counterEpoch;
    public readonly record struct Sample(long Present, long Timestamp, double IntervalMs,
        double IrqWaitMs, double PresentWaitMs, double PresentCallMs, double OutsidePresentWallMs,
        uint GameCounter, int CounterEpoch, string Phase, long UploadedBytes, long Batches,
        double VertexFenceWaitMs, long Dropped, double? GpuElapsedMs = null,
        PresentationProfile.Stages? PresentationStages = null,
        PresentationProfile.EventStages? BetweenPresentationEvents = null);
    public static long UploadedBytes, Batches;
    public static double VertexFenceWaitMs;
    public static readonly bool GpuQueries = Environment.GetEnvironmentVariable("RECOMP_PERF_GPU") == "1";
    public static long GpuQueriesSkipped;
    static long _gpuPresent = -1, _gpuTimestamp, _gpuBatches;
    static double _gpuElapsed;
    static uint _mainThreadId;

    public static void GpuBatch(long timestamp, long present, double elapsed)
    {
        if (_queue == null) return;
        if (_gpuPresent != present)
        {
            if (_gpuPresent >= 0)
            {
                var sample = new Sample(_gpuPresent, _gpuTimestamp, 0, 0, 0, 0, 0, 0, 0,
                    "gpu-batch", 0, _gpuBatches, 0, Interlocked.Read(ref Dropped), _gpuElapsed);
                if (!_queue.Writer.TryWrite(sample)) Interlocked.Increment(ref Dropped);
            }
            _gpuPresent = present; _gpuTimestamp = timestamp; _gpuBatches = 0; _gpuElapsed = 0;
        }
        _gpuBatches++; _gpuElapsed += elapsed;
    }

    public static void Start()
    {
        if (_queue != null || Environment.GetEnvironmentVariable("RECOMP_PERF_LOG") == "0") return;
        _queue = Channel.CreateBounded<Sample>(new BoundedChannelOptions(4096)
            { SingleWriter = true, SingleReader = true, FullMode = BoundedChannelFullMode.Wait });
        if (OperatingSystem.IsWindows()) _mainThreadId = GetCurrentThreadId();
        string directory = Environment.GetEnvironmentVariable("SPIDEY_LOG_DIR") ??
            Host.RuntimePaths.ApplicationDirectory;
        _writer = new Thread(() => Write(directory)) { IsBackground = true, Name = "PerformanceLog" };
        _writer.Start();
        AppDomain.CurrentDomain.ProcessExit += (_, _) => { _queue.Writer.TryComplete(); _writer.Join(1000); };
    }

    public static void Frame(long start, double irqWait, double presentWait, double presentCall, double outside)
    {
        if (_queue == null) return;
        uint counter = 0;
        if (Runtime.Mem is Memory.PSMemory memory && GameCounterAddress != 0)
        {
            uint physical = GameCounterAddress & 0x1fffffff;
            if (physical + 4 <= memory.Ram.Length)
                counter = BinaryPrimitives.ReadUInt32LittleEndian(memory.Ram.Slice((int)physical, 4));
        }
        if (counter < _previousGameCounter) _counterEpoch++;
        _previousGameCounter = counter;
        double interval = _previousStart == 0 ? 0 : Stopwatch.GetElapsedTime(_previousStart, start).TotalMilliseconds;
        _previousStart = start;
        var sample = new Sample(Runtime.Presents, start, interval, irqWait, presentWait, presentCall,
            outside, counter, _counterEpoch, Sdk.LibCdStream.Active ? "movie" : "game-or-menu",
            UploadedBytes, Batches, VertexFenceWaitMs, Interlocked.Read(ref Dropped),
            PresentationStages: PresentationProfile.Snapshot(),
            BetweenPresentationEvents: PresentationProfile.BetweenPresentations());
        UploadedBytes = Batches = 0; VertexFenceWaitMs = 0;
        if (!_queue.Writer.TryWrite(sample)) Interlocked.Increment(ref Dropped);
    }

    static void Write(string directory)
    {
        try
        {
            Directory.CreateDirectory(directory);
            string cpu = Environment.GetEnvironmentVariable("PROCESSOR_IDENTIFIER") ?? "unavailable";
            if (OperatingSystem.IsWindows())
                cpu = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\System\CentralProcessor\0", "ProcessorNameString", cpu)?.ToString() ?? cpu;
            ulong ramKb = 0;
            if (OperatingSystem.IsWindows()) GetPhysicallyInstalledSystemMemory(out ramKb);
            string? executable = Host.RuntimePaths.ApplicationFile;
            string hash = "unavailable";
            if (executable != null) { using var file = File.OpenRead(executable); hash = Convert.ToHexString(SHA256.HashData(file)); }
            using var process = Process.GetCurrentProcess();
            File.WriteAllText(Path.Combine(directory, "performance-system.json"), JsonSerializer.Serialize(new {
                schema = 1, utc = DateTime.UtcNow, cpu, logicalProcessors = Environment.ProcessorCount, gpuQueries = GpuQueries,
                installedRamBytes = ramKb * 1024, os = RuntimeInformation.OSDescription,
                runtime = RuntimeInformation.FrameworkDescription, executableSha256 = hash,
                Renderer, Backend, renderScale = Runtime.View.RenderScale, fxaa = Runtime.View.Fxaa,
                vsync = Runtime.View.VSync, timer = Host.FrameClock.WaitBackend,
                configuredWindowWidth = Runtime.View.WindowWidth, configuredWindowHeight = Runtime.View.WindowHeight,
                stopwatchFrequency = Stopwatch.Frequency, gameCounterAddress = GameCounterAddress,
                mainThreadId = _mainThreadId == 0 ? (uint?)null : _mainThreadId,
                priorityClass = ReadPriorityClass(process),
                note = "Outside-present is wall time, not CPU time. Priority class is read-only process metadata, not thread scheduling evidence. Optional GPU records sum available batch queries per presentation; skipped/pending queries limit coverage. Display timing and CPU/GPU clock rates are not inferred. Two rotating frame files."
            }, new JsonSerializerOptions { WriteIndented = true }));
            // A new session starts at part zero. Do not leave part one from a
            // previous session for the report reader to mix with this build.
            File.Delete(Path.Combine(directory, "performance-frames-1.jsonl"));
            File.Delete(Path.Combine(directory, "performance-process-1.jsonl"));
            int part = 0, rows = 0, processRows = 0, processPart = 0;
            StreamWriter Open() => new(Path.Combine(directory, $"performance-frames-{part}.jsonl"), false);
            StreamWriter OpenProcess() => new(Path.Combine(directory, $"performance-process-{processPart}.jsonl"), false);
            var system = OpenProcess();
            var output = Open();
            long next = 0;
            try
            {
                while (_queue!.Reader.WaitToReadAsync().AsTask().GetAwaiter().GetResult())
                {
                    while (_queue.Reader.TryRead(out var frame))
                    {
                        output.WriteLine(JsonSerializer.Serialize(frame));
                        if (++rows == 60000) { output.Dispose(); part ^= 1; rows = 0; output = Open(); }
                        long now = Environment.TickCount64;
                        if (now < next) continue;
                        next = now + 1000;
                        process.Refresh();
                        double? mainThreadCpuSeconds = null;
                        try
                        {
                            if (_mainThreadId != 0)
                                foreach (ProcessThread thread in process.Threads)
                                {
                                    using (thread)
                                        if (thread.Id == _mainThreadId) mainThreadCpuSeconds = thread.TotalProcessorTime.TotalSeconds;
                                }
                        }
                        catch (System.ComponentModel.Win32Exception) { /* Thread exited during the sample. */ }
                        catch (InvalidOperationException) { /* Unavailable at shutdown. */ }
                        PowerStatus? power = OperatingSystem.IsWindows() && GetSystemPowerStatus(out var status) ? status : null;
                        system.WriteLine(JsonSerializer.Serialize(new { Timestamp = Stopwatch.GetTimestamp(),
                            cpuSeconds = process.TotalProcessorTime.TotalSeconds, mainThreadCpuSeconds,
                            priorityClass = ReadPriorityClass(process),
                            privateBytes = process.PrivateMemorySize64,
                            workingBytes = process.WorkingSet64, allocatedBytes = GC.GetTotalAllocatedBytes(false),
                            gen0 = GC.CollectionCount(0), gen1 = GC.CollectionCount(1), gen2 = GC.CollectionCount(2), GpuQueriesSkipped,
                            acConnected = power.HasValue && power.Value.AcLine != 255 ? (bool?)(power.Value.AcLine == 1) : null,
                            batteryPercent = power.HasValue && power.Value.BatteryPercent != 255 ? (int?)power.Value.BatteryPercent : null }));
                        if (++processRows == 3600) { system.Dispose(); processPart ^= 1; processRows = 0; system = OpenProcess(); }
                        output.Flush(); system.Flush();
                    }
                }
            }
            finally { output.Dispose(); system.Dispose(); }
        }
        catch (Exception e) { _queue?.Writer.TryComplete(); Console.Error.WriteLine($"[performance] logging stopped: {e.Message}"); }
    }

    static string? ReadPriorityClass(Process process)
    {
        if (!OperatingSystem.IsWindows()) return null;
        try { return process.PriorityClass.ToString(); }
        catch (System.ComponentModel.Win32Exception) { return null; }
        catch (InvalidOperationException) { return null; }
        catch (NotSupportedException) { return null; }
    }

    [DllImport("kernel32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool GetPhysicallyInstalledSystemMemory(out ulong memoryInKilobytes);

    [DllImport("kernel32.dll")]
    static extern uint GetCurrentThreadId();

    [StructLayout(LayoutKind.Sequential)]
    struct PowerStatus
    {
        public byte AcLine, BatteryFlags, BatteryPercent, PowerSaving;
        public uint BatteryLifeSeconds, BatteryFullLifeSeconds;
    }
    [DllImport("kernel32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool GetSystemPowerStatus(out PowerStatus status);
}
