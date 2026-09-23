using System;
using System.Diagnostics;

namespace RecompOne.Runtime;

/// <summary>
/// How long the GPU is still working on the ordering table it was handed.
///
/// Optional artificial busy period for ports that need it. This is not a GPU
/// fill-rate model. Spider-Man and Spider-Man 2 leave it disabled: their native
/// vblank-counter waits already pace gameplay, and adding another whole frame
/// here can force an extra wait even though the submitted work has completed.
/// </summary>
public static class GpuBusy
{
    static readonly Stopwatch _clock = Stopwatch.StartNew();
    static double _busyUntilMs;

    /// <summary>
    /// How long a submitted ordering table keeps the GPU busy, in milliseconds.
    /// Zero disables the model, and DrawSync answers "idle" as it did before.
    /// </summary>
    public static double FrameBudgetMs;

    /// <summary>Set from the drawing submission -- the GPU has work again.</summary>
    public static void Submit()
    {
        if (FrameBudgetMs <= 0) return;
        // Deliberately not queued behind whatever is outstanding. Stacking budgets would
        // let a burst of submissions -- a loading screen, a cutscene, anything that
        // draws without waiting -- run the backlog up into whole seconds of false stall.
        _busyUntilMs = _clock.Elapsed.TotalMilliseconds + FrameBudgetMs;
    }

    /// <summary>Milliseconds of drawing still outstanding; 0 when the GPU is idle.</summary>
    public static double RemainingMs()
    {
        if (FrameBudgetMs <= 0) return 0;
        double left = _busyUntilMs - _clock.Elapsed.TotalMilliseconds;
        return left > 0 ? left : 0;
    }

    /// <summary>Give up the rest of the budget -- for a reset, or a mode change.</summary>
    public static void Clear() => _busyUntilMs = 0;
}
