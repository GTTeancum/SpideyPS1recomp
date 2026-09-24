using System;
using RecompOne.Runtime.Assets.Suits;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// SM2 instruction hook 80032E7C: adjusts projection inputs for web lines whose
/// native endpoint belongs to the current preserved-rig hand. Rope state is unchanged.
/// </summary>
public static class SuitWebAttachment
{
    const uint Scratch = 0x1F800000;
    const long OwnershipRadiusQ12 = 256L * 4096;
    static readonly int[] Tip = new int[3], Target = new int[3], Start = new int[3], End = new int[3], Result = new int[6];
    static readonly bool Trace = Environment.GetEnvironmentVariable("SPIDEY_RETARGET_TRACE") == "1";
    static uint _line, _actor, _count;
    static NativeRetargetRig _rig, _reported;
    static bool _valid;

    static bool Ram(uint p, uint n) => n <= 0x800000 && p >= 0x80000000 && p <= 0x80800000 - n;
    static int I32(IMemory m, uint p) => unchecked((int)m.ReadU32(p));
    static void Vector(IMemory m, uint p, int[] v) { for (uint i = 0; i < 3; i++) v[i] = I32(m, p + i * 4); }

    public static void ProjectSwingSegment(CpuContext c, IMemory m)
    {
        uint line = c.S1, segment = c.FP;
        if (segment == 0) _valid = false;
        int selected = Costume.LoadedCostume;
        if (selected != SuitMods.Active || !SuitMods.IsMod(selected)) return;
        NativeRetargetRig rig = SuitMods.At(selected).CustomModel?.RetargetRig;
        if (rig == null || !Ram(line, 0x58)) return;
        uint count = m.ReadU32(line + 0x3C), points = m.ReadU32(line + 0x40);
        if (count == 0 || count > 4096 || segment >= count || !Ram(points, count * 16)) return;

        if (segment == 0)
        {
            if (!SuitRetargeting.TryCurrentWebTarget(m, rig, Target, out uint actor)) return;
            Vector(m, points + (count - 1) * 16, Tip);
            long distanceSquared = 0;
            for (int i = 0; i < 3; i++)
            {
                long d = (long)Target[i] - Tip[i];
                distanceSquared += d * d;
            }
            if (distanceSquared > OwnershipRadiusQ12 * OwnershipRadiusQ12) return;
            _line = line; _actor = actor; _rig = rig; _count = count; _valid = true;
            if (Trace && !ReferenceEquals(_reported, rig))
            {
                _reported = rig;
                Console.WriteLine($"[web-retarget-sm2] live render: actor {actor:X8}, line {line:X8}, vertical correction {(Target[1] - (double)Tip[1]) / 4096.0:F4} world units; physics/anchor untouched");
            }
        }
        if (!_valid || _line != line || _actor == 0 || !ReferenceEquals(_rig, rig) || _count != count) return;
        Vector(m, segment == 0 ? line + 0x44 : points + (segment - 1) * 16, Start);
        Vector(m, points + segment * 16, End);
        if (!NativeRetargetRig.TryWarpWebSegment(Start, End, Tip, Target, segment, count, Result)) return;
        for (uint i = 0; i < 6; i++) m.WriteU32(Scratch + i * 4, unchecked((uint)Result[i]));
    }
}
