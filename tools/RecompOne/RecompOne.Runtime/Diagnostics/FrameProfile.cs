using System.Diagnostics;

namespace RecompOne.Runtime.Diagnostics;

public static class FrameProfile
{
    static readonly object Gate = new();
    static long _lastEnd, _previousStart;
    static double _irqWait, _present, _throttle, _outside, _windowIrq;
    static int _frames;
    static readonly List<double> Intervals = new(1024);

    public static void NoteIrqWait(long start, long end) => _irqWait += Stopwatch.GetElapsedTime(start, end).TotalMilliseconds;

    public static void Note(long t0, long t1, long t2)
    {
        double wait = Stopwatch.GetElapsedTime(t0, t1).TotalMilliseconds;
        double present = Stopwatch.GetElapsedTime(t1, t2).TotalMilliseconds;
        double outside = _lastEnd == 0 ? 0 : Math.Max(0, Stopwatch.GetElapsedTime(_lastEnd, t0).TotalMilliseconds - _irqWait);
        PerformanceLog.Frame(t1, _irqWait, wait, present, outside);
        lock (Gate)
        {
            _windowIrq += _irqWait; _throttle += wait; _present += present; _outside += outside; _frames++;
            if (_previousStart != 0 && Intervals.Count < 4096)
                Intervals.Add(Stopwatch.GetElapsedTime(_previousStart, t1).TotalMilliseconds);
        }
        _previousStart = t1; _lastEnd = t2; _irqWait = 0;
    }

    public static string Summary()
    {
        lock (Gate)
        {
            if (_frames == 0) return "no presentations";
            Intervals.Sort();
            double P(double p) => Intervals.Count == 0 ? 0 : Intervals[(int)Math.Ceiling(p * (Intervals.Count - 1))];
            string text = $"per presentation: call {_present / _frames:F2}ms, limiter {_throttle / _frames:F2}ms, " +
                $"IRQ wait {_windowIrq / _frames:F2}ms, outside-present wall {_outside / _frames:F2}ms; " +
                $"interval p50/p95/p99/max {P(.5):F2}/{P(.95):F2}/{P(.99):F2}/{P(1):F2}ms (not GPU time)";
            _present = _throttle = _outside = _windowIrq = 0; _frames = 0; Intervals.Clear();
            return text;
        }
    }
}
