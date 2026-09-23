using System;
using System.Collections.Generic;

namespace Recompiled;

// Test-only controller schedule. Reads an externally supplied native counter;
// never changes console time, game memory, actors, or trigger state.
public sealed class NativeInputSchedule
{
    readonly List<(uint Start, ulong End, ushort Mask)> _steps = new();
    bool _armed, _started, _finished;
    uint _previous;
    public int Count => _steps.Count;
    public void Add(uint start, uint duration, ushort mask)
    {
        if (duration == 0 || mask == 0) throw new ArgumentException("Native input requires duration and buttons");
        _steps.Add((start, (ulong)start + duration, mask));
    }
    public void Arm() => _armed = true;
    public ushort Sample(uint counter)
    {
        if (!_armed || _finished) return 0;
        // The archive may load while a preceding scene still has a live counter.
        if (!_started)
        {
            if (counter != 0) return 0;
            _started = true;
        }
        if (counter < _previous) { _finished = true; return 0; }
        _previous = counter;
        ushort held = 0;
        foreach (var step in _steps)
            if (counter >= step.Start && (ulong)counter < step.End) held |= step.Mask;
        return held;
    }
}
