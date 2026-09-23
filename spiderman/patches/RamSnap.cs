using System;
using System.Collections.Generic;
using System.IO;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Writes RAM snapshots on named frames or on a crash.
///
/// This exists to find variables by differencing rather than by reading disassembly.
/// Driving the menus from a timed button script does not work -- the frame a screen
/// appears on moves by hundreds between runs -- so the port needs to read the menu's
/// own selection instead of guessing when to press. Snapshot with one item highlighted,
/// press, snapshot again, and the selection is whichever small integer moved by one.
///
///     SPIDEY_SNAP=2560,2680     write ram_02560.bin and ram_02680.bin
///
/// Named-frame snapshots include up to 3 MB (retail RAM and fixed overlays).
/// Crash snapshots include all RAM, including the stack near 0x807FFFF0.
/// SPIDEY_SNAP_EXTENDED=1 also includes all RAM in named-frame snapshots.
/// </summary>
public static class RamSnap
{
    const uint Base = 0x80000000;

    static readonly HashSet<long> _frames = new();
    static readonly List<(string Anchor, long Offset)> _anchors = new();
    static string _dir = "snaps";
    static bool _onCrash;
    static IMemory _mem;
    static long _lastFrame;

    public static void Install()
    {
        var spec = Environment.GetEnvironmentVariable("SPIDEY_SNAP");
        if (string.IsNullOrWhiteSpace(spec)) return;

        foreach (var raw in spec.Split(',', StringSplitOptions.RemoveEmptyEntries))
        {
            var t = raw.Trim();
            // A frame number is only reachable if the run gets that far, and a run that
            // dies gets there at a different frame every time. "crash" catches it where
            // it actually matters.
            if (t.Equals("crash", StringComparison.OrdinalIgnoreCase)) _onCrash = true;
            else if (long.TryParse(t, out var f)) _frames.Add(f);
            else
            {
                int plus = t.LastIndexOf('+');
                if (plus <= 0 || !long.TryParse(t[(plus + 1)..], out var offset) || offset <= 0)
                    throw new ArgumentException($"Invalid SPIDEY_SNAP checkpoint: {t}");
                _anchors.Add((t[..plus], offset));
            }
        }
        if (_frames.Count == 0 && _anchors.Count == 0 && !_onCrash) return;

        _dir = Environment.GetEnvironmentVariable("SPIDEY_SNAP_DIR") ?? "snaps";
        Directory.CreateDirectory(_dir);
        Event.AddListener<VSyncEvent>(OnFrame);
        Console.WriteLine($"[snap] armed for {_frames.Count} absolute and {_anchors.Count} anchored frame(s)" +
                          (_onCrash ? " and on crash" : "") + $" -> {_dir}");
    }

    // Match Capture's first archive-load anchor. Resolving only removes a pending
    // diagnostic request; it never changes input, IRQ delivery or game memory.
    public static void NoteWadLoad(string name, long frame)
    {
        for (int i = _anchors.Count - 1; i >= 0; i--)
        {
            var request = _anchors[i];
            if (!string.Equals(request.Anchor, name, StringComparison.OrdinalIgnoreCase)) continue;
            long target = checked(frame + request.Offset);
            _frames.Add(target);
            _anchors.RemoveAt(i);
            Console.WriteLine($"[snap] '{name}' at frame {frame}: offset {request.Offset} resolved to {target}");
        }
    }

    /// <summary>Write a snapshot now, named for why. Safe to call from a crash handler.</summary>
    public static void DumpNow(string tag)
    {
        if (!_onCrash || _mem == null) return;
        try { Write(_mem, $"ram_{tag}.bin", true); }
        catch (Exception e) { Console.Error.WriteLine($"[snap] {tag} failed: {e.Message}"); }
    }

    static void OnFrame(VSyncEvent e)
    {
        _mem = e.Memory;
        _lastFrame = e.Frame;
        if (!_frames.Remove(e.Frame)) return;

        Write(e.Memory, $"ram_{e.Frame:D5}.bin");
    }

    static void Write(IMemory m, string name, bool full = false)
    {
        Directory.CreateDirectory(_dir);
        uint size = full || Environment.GetEnvironmentVariable("SPIDEY_SNAP_EXTENDED") == "1"
            ? RecompOne.Runtime.Runtime.RamSize : Math.Min(0x00300000u, RecompOne.Runtime.Runtime.RamSize);
        byte[] buf;
        if (m is PSMemory ps)
        {
            // Raw copying avoids the normal read idle breaker: a diagnostic
            // snapshot must not dispatch interrupts or mutate the captured game.
            buf = ps.Ram[..(int)size].ToArray();
        }
        else
        {
            buf = new byte[size];
            for (uint o = 0; o < size; o += 4)
            {
                uint w = m.ReadU32(Base + o);
                buf[o] = (byte)w; buf[o + 1] = (byte)(w >> 8);
                buf[o + 2] = (byte)(w >> 16); buf[o + 3] = (byte)(w >> 24);
            }
        }

        string path = Path.Combine(_dir, name);
        File.WriteAllBytes(path, buf);
        Console.WriteLine($"[snap] {path} (frame {_lastFrame})");
    }
}
