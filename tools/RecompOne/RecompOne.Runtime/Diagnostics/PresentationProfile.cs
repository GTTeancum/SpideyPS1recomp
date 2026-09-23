using System.Diagnostics;

namespace RecompOne.Runtime.Diagnostics;

/// <summary>Opt-in wall-time breakdown; never GPU or display delivery timing.</summary>
public static class PresentationProfile
{
    public static readonly bool Enabled = Environment.GetEnvironmentVariable("RECOMP_PERF_PHASES") == "1";
    public readonly record struct Stages(double EventsMs, double RenderDispatchMs,
        double RenderCallbackMs, double TitleMs, double AudioAttachMs,
        double EventPumpMs, double InputPollMs, double ControllerEventsMs,
        int RenderCallbacks, double? BeforeRenderCallbackMs, double? AfterRenderCallbackMs);
    public static double EventsMs, RenderDispatchMs, RenderCallbackMs, TitleMs, AudioAttachMs;
    public static double EventPumpMs, InputPollMs, ControllerEventsMs;
    public readonly record struct EventStages(double EventPumpMs, double InputPollMs, double ControllerEventsMs);
    static EventStages? _betweenPresentations;
    static int _renderCallbacks;
    static long _firstCallback, _lastCallbackEnd;
    static double? _beforeCallbackMs, _afterCallbackMs;
    public static void EnterRenderCallback(long stamp)
    {
        if (Enabled && _renderCallbacks++ == 0) _firstCallback = stamp;
    }
    public static void ExitRenderCallback(long stamp)
    {
        if (Enabled) _lastCallbackEnd = stamp;
    }
    public static void FinishRenderDispatch(long start, long end)
    {
        if (!Enabled) return;
        RenderDispatchMs = (end - start) * 1000.0 / Stopwatch.Frequency;
        // Null means no complete callback in this dispatch, not zero overhead.
        if (_renderCallbacks > 0 && _firstCallback >= start &&
            _lastCallbackEnd >= _firstCallback && _lastCallbackEnd <= end)
        {
            _beforeCallbackMs = (_firstCallback - start) * 1000.0 / Stopwatch.Frequency;
            _afterCallbackMs = (end - _lastCallbackEnd) * 1000.0 / Stopwatch.Frequency;
        }
    }
    public static long Stamp() => Enabled ? Stopwatch.GetTimestamp() : 0;
    public static double Elapsed(long start) => Enabled ? Stopwatch.GetElapsedTime(start).TotalMilliseconds : 0;
    public static void Reset()
    {
        EventsMs = RenderDispatchMs = RenderCallbackMs = TitleMs = AudioAttachMs =
            EventPumpMs = InputPollMs = ControllerEventsMs = 0;
        _renderCallbacks = 0;
        _firstCallback = _lastCallbackEnd = 0;
        _beforeCallbackMs = _afterCallbackMs = null;
    }
    // ServiceOnly and the non-presenting vblank also pump events. Preserve their
    // accumulated cost before resetting the counters for the current presentation.
    public static void BeginPresentation()
    {
        _betweenPresentations = Enabled ? new(EventPumpMs, InputPollMs, ControllerEventsMs) : null;
        Reset();
    }
    public static EventStages? BetweenPresentations() => _betweenPresentations;
    public static void EndPresentation()
    {
        Reset();
        _betweenPresentations = null;
    }
    public static Stages? Snapshot() => Enabled
        ? new(EventsMs, RenderDispatchMs, RenderCallbackMs, TitleMs, AudioAttachMs,
            EventPumpMs, InputPollMs, ControllerEventsMs,
            _renderCallbacks, _beforeCallbackMs, _afterCallbackMs) : null;
}
