using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;
using RecompOne.Runtime.Sdk;

namespace Recompiled;

/// <summary>
/// The retail main loop runs one update and then waits for two vblanks, each wait
/// counting from the moment it is called. An update the console could not finish
/// inside one vblank therefore took three or more, and nothing in the game reads the
/// elapsed time: the simulation simply ran slower. The port finishes every update
/// almost at once, so without help it always runs at the full 30 updates/s.
///
/// Chase Venom (l5a1) is held to the console's pace, measured on the original disc
/// and set per stretch in 5 FPS steps (docs/chase-original-timing.md): 20 updates/s,
/// except 15 from Venom's third stop through the building sequence and 25 from his
/// sixth stop to the hideout. Other levels keep the full rate.
/// </summary>
public static class Timing
{
    static readonly bool Trace = System.Environment.GetEnvironmentVariable("SPIDEY_TRACE_TIMING") == "1";
    static bool _chaseLevel;

    // Chase stretch tracking: how many times the player script has started since the
    // level loaded (1 intro, 2 building, 3 ending), whether it is running now, the
    // furthest route point Venom has headed for on each half of the route, and the
    // tenths of a vblank owed so far (25 updates/s alternates 2 and 3).
    static int _scripts, _venomPoint;
    static bool _scriptActive, _pastLastStop;
    static int _owedTenths;

    // LoadTriggers pre-hook. A reload/retry must not inherit another scene's rate.
    public static void LoadTriggers(CpuContext c, IMemory m)
    {
        const string name = "l5a1_t";
        _chaseLevel = c.A0 != 0;
        for (int i = 0; _chaseLevel && i <= name.Length; i++)
            _chaseLevel = m.ReadU8(c.A0 + (uint)i) == (i == name.Length ? 0 : name[i]);
        _scripts = _venomPoint = 0;
        _scriptActive = _pastLastStop = false;
        _owedTenths = 0;
    }

    // SPIDEY_CADENCE_FILE=path: "game_counter,vblanks" rows (a recorded console run)
    // that set each l5a1 update's length instead of ChasePace, so the port reaches
    // the same game states at the same counters as that run.
    static readonly System.Collections.Generic.Dictionary<uint, int>? _cadence = LoadCadence();

    static System.Collections.Generic.Dictionary<uint, int>? LoadCadence()
    {
        string? path = System.Environment.GetEnvironmentVariable("SPIDEY_CADENCE_FILE");
        if (string.IsNullOrWhiteSpace(path)) return null;
        var map = new System.Collections.Generic.Dictionary<uint, int>();
        foreach (string line in System.IO.File.ReadLines(path))
        {
            var parts = line.Split(',');
            if (parts.Length >= 2 && uint.TryParse(parts[0], out uint gc) && int.TryParse(parts[1], out int ticks))
                map[gc] = ticks;
        }
        System.Console.WriteLine($"[timing] cadence file: {map.Count} updates");
        return map;
    }

    // Retail 8005E748 waits on the counter incremented by the game's vblank IRQ.
    public static void WaitVBlanks(CpuContext c, IMemory m)
    {
        // The main loop calls this twice with A0=1; only its second wait is extended.
        // Presentation stays capped and the console clock stays at 60 Hz.
        if (_chaseLevel && c.RA == 0x8002C294u && c.A0 == 1)
        {
            int vblanks;
            if (_cadence != null)
                vblanks = _cadence.TryGetValue(m.ReadU32(0x800B4F38u), out int ticks) ? ticks : 2;
            else
            {
                _owedTenths += ChaseTenths(m) - 20;
                vblanks = 2 + _owedTenths / 10;
                _owedTenths %= 10;
            }
            if (vblanks > 2) c.A0 = (uint)(vblanks - 1);
        }
        LibEtc.WaitCounter(c, m, c.GP + 0xC74u);
    }

    // Venom's route points: his third stop is 35, the building entry 54, his sixth
    // stop 227, the last on that stretch 241. Past it the numbering restarts at the
    // hideout (11); values outside the route are other scripts' data.
    const int ThirdStop = 35, BuildingEntry = 54, SixthStop = 227, LastRoutePoint = 241;

    // Tenths of a vblank per update for the current stretch of the chase.
    static int ChaseTenths(IMemory m)
    {
        uint player = m.ReadU32(0x800B5268u);
        if (player < 0x80000000u || player >= 0x80200000u) return 20;
        bool script = m.ReadU32(player + 0x1A8u) != 0;
        if (script && !_scriptActive)
        {
            _scripts++;
            _venomPoint = 0;
            if (Trace) System.Console.WriteLine($"[timing] chase: player script {_scripts} started");
        }
        _scriptActive = script;
        // The building script measures 21.5 updates/s on the disc, but it never runs
        // there at a steady 20: held at three vblanks per update, Spider-Man's scripted
        // run misses the cutscene region and the sequence stalls (as it does at 25).
        // Two and four both reach it at the disc's timing; four is the nearer step.
        if (script) return _scripts == 2 ? 40 : 30;

        int point = VenomPoint(m);
        if (_scripts <= 1)
        {
            if (point > 0 && point <= BuildingEntry && point > _venomPoint) _venomPoint = point;
            return _venomPoint >= ThirdStop ? 40 : 30;
        }
        if (_scripts == 2)
        {
            if (point >= SixthStop && point <= LastRoutePoint && !_pastLastStop)
            {
                _pastLastStop = true;
                if (Trace) System.Console.WriteLine("[timing] chase: past the sixth stop, 25 updates/s");
            }
            return _pastLastStop ? 24 : 30;
        }
        return 30;
    }

    // The route point Venom is heading for, or 0 when he is not in the level.
    static int VenomPoint(IMemory m)
    {
        uint actor = m.ReadU32(0x800B5234u);
        for (int i = 0; i < 1024 && actor >= 0x80000000u && actor < 0x80200000u; i++)
        {
            if (m.ReadU16(actor + 0x34u) == 0x139)
            {
                uint script = m.ReadU32(actor + 0x31Cu);
                return script >= 0x80000000u && script < 0x80200000u ? m.ReadU16(script + 4) : 0;
            }
            actor = m.ReadU32(actor + 0x1Cu);
        }
        return 0;
    }
}
