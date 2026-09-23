using System;
using RecompOne.Runtime.Assets.Suits;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// SM1 instruction hook 8002CF80: after a native line segment's two world-Q12
/// positions have been copied to scratch RAM, before func_80080988 projects it.
/// Changes projection inputs ONLY. In particular, do not edit rope+40/+64 or
/// controller+F8: those buffers are also read by native motion/release routines.
/// </summary>
public static class SuitWebAttachment
{
    const uint Scratch = 0x1F800000;
    static readonly float[] Driver = new float[216];
    static readonly short[] Rotation = new short[9];
    static readonly int[] Translation = new int[3], Position = new int[3];
    static readonly int[] Tip = new int[3], Target = new int[3], Start = new int[3], End = new int[3], Result = new int[6];
    static readonly bool Trace = Environment.GetEnvironmentVariable("SPIDEY_RETARGET_TRACE") == "1";
    static uint _line, _actor, _count;
    static NativeRetargetRig _rig, _reported;
    static bool _valid, _warnedPose;
    static bool Ram(uint p, uint n) => n <= 0x800000 && p >= 0x80000000 && p <= 0x80800000 - n;
    static int I32(IMemory m, uint p) => unchecked((int)m.ReadU32(p));
    static void Vector(IMemory m, uint p, int[] v) { for (uint i = 0; i < 3; i++) v[i] = I32(m, p + i * 4); }

    static bool CurrentPose(IMemory m, uint actor, uint entry, out uint pose)
    {
        pose = 0;
        // Same cached-pose branch used by native sockets and the native renderer.
        if ((m.ReadU16(actor) & 4) != 0)
            pose = m.ReadU32(actor + 0x180);
        else
        {
            uint anim = m.ReadU32(entry + 0x18);
            if (!Ram(anim - 8, 12)) return false;
            // Supplied converted actors retain the original 0x2C packed-animation bank.
            // The native hand queries at 80046168/80046178 already populate its full
            // pose cache. Never substitute last frame's pose if it isn't current.
            if (m.ReadU32(anim - 8) != 0x2C ||
                m.ReadU16(actor + 0x136) != m.ReadU16(actor + 0x126) ||
                m.ReadU16(actor + 0x138) != m.ReadU16(actor + 0x124)) return false;
            pose = m.ReadU32(actor + 0x130);
        }
        return Ram(pose, 18 * 24);
    }

    public static void ProjectSwingSegment(CpuContext c, IMemory m)
    {
        uint line = c.S0, segment = c.S6;
        if (segment == 0) _valid = false;
        int selected = Costume.LoadedCostume;
        if (selected != SuitMods.Active || !SuitMods.IsMod(selected)) { _valid = false; return; }
        NativeRetargetRig rig = SuitMods.At(selected).CustomModel?.RetargetRig;
        if (rig == null) { _valid = false; return; }
        uint actor = m.ReadU32(0x800B5268);
        if (!Ram(actor, 0xF88) || !Ram(line, 0x78)) return;
        uint controller = m.ReadU32(actor + 0xF84);
        if (!Ram(controller, 0x18C)) return;
        uint primary = m.ReadU32(controller + 0x178);
        if (!Ram(primary, 0x78)) return;
        // Both the main line and its native doubled-point embellishment are warped.
        // All other effects/NPC lines/zip lines retain the unmodified native path.
        if (line != primary && line != m.ReadU32(primary + 0x68)) return;
        uint count = m.ReadU32(line + 0x3C), points = m.ReadU32(line + 0x40);
        if (count == 0 || count > 4096 || segment >= count || !Ram(points, count * 16)) return;

        if (segment == 0 || !_valid || _line != line || _actor != actor || !ReferenceEquals(_rig, rig) || _count != count)
        {
            _valid = false;
            uint slot = m.ReadU8(actor + 0x1B);
            if (slot >= 40) return;
            uint entry = 0x800A0904 + slot * 64;
            if (m.ReadU32(entry) != 0x64697073 || m.ReadU16(entry + 4) != 0x7965 || m.ReadU8(entry + 6) != 0) return;
            if (!CurrentPose(m, actor, entry, out uint pose))
            {
                if (Trace && !_warnedPose) { _warnedPose = true; Console.WriteLine("[web-retarget] skipped: native pose cache is not current/supported; no stale attachment used"); }
                return;
            }
            for (int i = 0; i < 18; i++)
            {
                uint p = pose + (uint)i * 24;
                for (int r = 0; r < 3; r++)
                {
                    for (int k = 0; k < 3; k++) Driver[i * 12 + r * 4 + k] = unchecked((short)m.ReadU16(p + (uint)(r * 3 + k) * 2)) / 4096f;
                    Driver[i * 12 + r * 4 + 3] = unchecked((short)m.ReadU16(p + 18 + (uint)r * 2));
                }
            }
            for (uint i = 0; i < 9; i++) Rotation[i] = unchecked((short)m.ReadU16(actor + 0x160 + i * 2));
            Vector(m, actor + 0x174, Translation); Vector(m, actor + 4, Position);
            // Literal native hand-selection branch at 80046444: clip 0x118 uses
            // socket 1 (part 5); every other swing clip uses socket 0 (part 10).
            uint part = m.ReadU16(actor + 0x126) == 0x118 ? 5u : 10u;
            rig.EvaluatePose(Driver);
            if (!rig.TryGetWebTarget(part, Rotation, Translation, Position, (m.ReadU32(actor + 0x128) & 2) != 0, Target)) return;
            Vector(m, points + (count - 1) * 16, Tip);
            _line = line; _actor = actor; _rig = rig; _count = count; _valid = true;
            if (Trace && !ReferenceEquals(_reported, rig))
            {
                _reported = rig;
                Console.WriteLine($"[web-retarget] live swing render: actor {actor:X8}, line {line:X8}, part {part}, vertical correction {(Target[1] - (double)Tip[1]) / 4096.0:F4} world units; physics/anchor untouched");
            }
        }
        // Read ORIGINAL points, not the prior segment's modified scratch endpoint.
        // The native loop carries scratch forward; reusing it would double the warp.
        Vector(m, segment == 0 ? line + 0x44 : points + (segment - 1) * 16, Start);
        Vector(m, points + segment * 16, End);
        if (!NativeRetargetRig.TryWarpWebSegment(Start, End, Tip, Target, segment, count, Result)) return;
        for (uint i = 0; i < 6; i++) m.WriteU32(Scratch + i * 4, unchecked((uint)Result[i]));
        // CPU registers, GTE state, original line buffers and actor state unchanged.
    }
}
