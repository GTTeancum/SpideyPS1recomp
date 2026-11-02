using System.Diagnostics;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace RecompOne.Runtime.Host;

internal static class FrameClock
{
    const double VBlankMs = 1000.0 / 60.0;
    const double SpinMs = 1.5;

    /// <summary>
    /// Console ticks per host presentation, at least two for a maximum of 30 FPS.
    /// Console IRQ delivery remains at 60 Hz independently of the presentation cap.
    /// </summary>
    public static int VBlanksPerFrame = 2;

    static double FrameMs => VBlankMs * Math.Max(2, VBlanksPerFrame);

    static readonly Stopwatch _clock = Stopwatch.StartNew();
    static double _nextFrameMs;

    public static bool VSync { get; set; }

    public static double LastFrameMs { get; private set; }
    public static double LastWaitMs { get; private set; }

    static double _lastStart;
    static double _lastVBlank;
    static readonly SafeWaitHandle? _timer = CreateTimer();
    public static string WaitBackend => _timer is { IsClosed: false } ? "windows-high-resolution" : "sleep-fallback";

    static SafeWaitHandle? CreateTimer()
    {
        if (!OperatingSystem.IsWindowsVersionAtLeast(10, 0, 17134)) return null;
        // A private high-resolution timer avoids the 15.6 ms Thread.Sleep quantum
        // without changing the timer resolution for other applications.
        var timer = CreateWaitableTimerExW(IntPtr.Zero, IntPtr.Zero, 2, 0x00100002);
        if (!timer.IsInvalid) return timer;
        timer.Dispose();
        return null;
    }

    [DllImport("kernel32.dll", ExactSpelling = true)]
    static extern SafeWaitHandle CreateWaitableTimerExW(IntPtr attributes, IntPtr name, uint flags, uint access);

    [DllImport("kernel32.dll", ExactSpelling = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool SetWaitableTimer(SafeWaitHandle timer, ref long dueTime, int period,
        IntPtr completionRoutine, IntPtr argument, [MarshalAs(UnmanagedType.Bool)] bool resume);

    [DllImport("kernel32.dll", ExactSpelling = true)]
    static extern uint WaitForSingleObject(SafeWaitHandle handle, uint milliseconds);

    public static void WaitForVBlank()
    {
        double deadline = _lastVBlank + VBlankMs;
        double now = _clock.Elapsed.TotalMilliseconds;
        // Preserve the console phase across a late edge. Reanchoring every edge
        // to delivery time made 20 ms of work plus two waits take 36.7 ms.
        // Keep at most the two edges belonging to one normal update. Dropping
        // both as soon as work exceeded 33.3 ms added another 16.7 ms wait and
        // abruptly reduced a barely overloaded game to about 20 FPS. A long
        // stall still cannot accumulate more than one update of IRQ debt, and
        // Throttle separately prevents presentation catch-up bursts.
        if (now - deadline >= 2 * VBlankMs) deadline = now - VBlankMs;
        WaitUntil(deadline);
        _lastVBlank = deadline;
    }

    static void WaitUntil(double deadline)
    {
        double sleep = deadline - _clock.Elapsed.TotalMilliseconds - SpinMs;
        if (sleep >= 1)
        {
            long dueTime = -(long)(sleep * 10000); // Relative time in 100 ns units.
            if (_timer == null || _timer.IsClosed ||
                !SetWaitableTimer(_timer, ref dueTime, 0, IntPtr.Zero, IntPtr.Zero, false) ||
                WaitForSingleObject(_timer, uint.MaxValue) != 0)
                Thread.Sleep((int)sleep);
        }
        while (_clock.Elapsed.TotalMilliseconds < deadline) Thread.SpinWait(48);
    }


    public static void Throttle()
    {
        double now = _clock.Elapsed.TotalMilliseconds;
        // Pace actual presentation starts. Never repay a stall with a burst of
        // fast frames, and never let a monitor's refresh rate bypass the 30 FPS cap.
        _nextFrameMs = _lastStart + FrameMs;
        WaitUntil(_nextFrameMs);

        double started = _clock.Elapsed.TotalMilliseconds;
        LastWaitMs = Math.Max(0, started - now);
        LastFrameMs = started - _lastStart;
        _lastStart = started;
    }

    public static void Resync() => _lastStart = _clock.Elapsed.TotalMilliseconds;
    public static void Close() => _timer?.Dispose();
}
