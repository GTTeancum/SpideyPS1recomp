using RecompOne.Runtime.Context;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Memory;

namespace RecompOne.Runtime.Sdk;

public static class LibEtc
{
    static int _vcount;

    /// <summary>Rate instrumentation -- VSync(0) waits vs VSync(-1) polls.</summary>
    public static long WaitCalls, PollCalls;

    const double FrameSeconds = 1.0 / 60.0;
    static readonly VSyncEvent _vsyncEvent = new();
    static readonly VSyncInputEvent _inputEvent = new();
    static readonly System.Diagnostics.Stopwatch _sinceFrame = System.Diagnostics.Stopwatch.StartNew();

    public static void VSync(CpuContext c, IMemory m)
    {
        int mode = (int)c.A0;
        if (Log.SdkOn) Log.Sdk($"VSync({mode})");
        if (mode < 0)
        {
            PollCalls++;
            // VSync(-1) reads the vblank counter without waiting. On hardware that
            // counter advances on its own. Service one individual 60 Hz edge when
            // due; host rendering has its own 30 FPS cap.
            if (_sinceFrame.Elapsed.TotalSeconds >= FrameSeconds) Runtime.IdleTick();
            c.V0 = (uint)_vcount;
            return;
        }
        //if (mode == 1) { c.V0 = 0; return; }
        
        WaitCalls++;
        Pump(c, m);
        c.V0 = 0;
    }

    static readonly System.Diagnostics.Stopwatch _sinceService = System.Diagnostics.Stopwatch.StartNew();

    /// <summary>Yield for a game's interrupt counter instead of spinning on RAM.</summary>
    public static void WaitCounter(CpuContext c, IMemory m, uint address)
    {
        // Match the retail leaf's unsigned comparison, including its wraparound
        // behavior. Each pump sleeps until one real console edge and dispatches
        // the game's IRQ; never write or advance the game counter ourselves.
        uint target = unchecked(m.ReadU32(address) + c.A0);
        while (m.ReadU32(address) < target) Pump(c, m);
        c.V1 = target;
        c.V0 = 0;
    }

    /// <summary>
    /// What the idle breakers call. Delivers a real vblank when one is due, and
    /// otherwise just keeps the host and the hardware serviced -- see
    /// Runtime.ServiceOnly for why those two have to be separate.
    /// </summary>
    public static void Service(CpuContext c, IMemory m)
    {
        if (_sinceFrame.Elapsed.TotalSeconds >= FrameSeconds) { Pump(c, m); return; }
        // Cheap, but not free: rate-limit it so a tight spin does not spend all its
        // time polling window events instead of running the game.
        if (_sinceService.Elapsed.TotalMilliseconds < 4) return;
        _sinceService.Restart();
        Runtime.ServiceOnly();
    }

    /// <summary>
    /// Advance one console vblank, presenting only every second tick. Called by VSync, and
    /// by the idle-loop breaker when the game busy-waits on an interrupt-updated flag
    /// instead of calling VSync itself.
    /// </summary>
    public static void Pump(CpuContext c, IMemory m)
    {
        if (Event.HasAnyListeners<VSyncInputEvent>())
        {
            _inputEvent.Context = c; _inputEvent.Memory = m;
            _inputEvent.Frame = (long)_vcount + 1;
            Event.Dispatch(_inputEvent);
        }
        Runtime.PresentFrame();
        _vcount++;
        _sinceFrame.Restart();

        if (Event.HasAnyListeners<VSyncEvent>())
        {
            var e = _vsyncEvent;
            e.Context = c; e.Memory = m;
            e.Frame = _vcount;
            Event.Dispatch(e);
        }
    }
}
