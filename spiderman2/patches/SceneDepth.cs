using System;
using System.Collections.Generic;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hle;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>Give SM2 scenery and both native actor precision modes comparable depth.</summary>
public static class SceneDepth
{
    static readonly Stack<float> Scales = new();
    static readonly bool Enabled = Environment.GetEnvironmentVariable("SPIDEY_SCENE_DEPTH") != "0";

    public static void Install() => GpuHle.SceneDepthEnabled = Enabled;

    public static void ActorTransformEnter(CpuContext c, IMemory m)
    {
        Scales.Push(Gte.ProjectionDepthScale);
        // 800837C4 selects the near path at camera Z < 1000. It multiplies the
        // translation by 16 and uses unshifted model vertices; the far routines
        // shift vertices right by 4. Normalize only host depth, never native SZ,
        // ordering-table indices, SXY, clipping flags or texture coordinates.
        Gte.ProjectionDepthScale = Enabled && m.ReadU32(c.GP + 0x1358u) != 0 ? 1f / 16f : 1f;
    }

    public static void ActorTransformExit(CpuContext c, IMemory m)
    {
        Gte.ProjectionDepthScale = Scales.Pop();
    }
}
