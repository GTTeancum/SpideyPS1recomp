using System;
using System.Collections.Generic;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Runtime compatibility fixes for higher-detail Dreamcast character meshes.
/// </summary>
public static class DcModelCompatibility
{
    const int RetailHeadVertexCount = 38;
    const int BagmanHeadFaceCount = 179;
    const int BagmanInnerFaceCount = 86;
    const ushort BagmanInnerDepthMarker = 0x6000;
    const uint BagmanDepthAddress = 0x1F800344u;
    const short DefaultBagmanDepthBias = 128;

    static readonly bool Trace =
        !string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("RECOMP_TRACE_MODEL_STITCHES"));
    static readonly short BagmanDepthBias = ReadBagmanDepthBias();
    static readonly HashSet<uint> _reportedHeads = [];
    static bool _bagmanDepthActive;
    static ushort _savedBagmanDepth;

    // The rotated rigid-object path borrows GTE rotation registers 0..4 to
    // compose its light matrix. S4 points to the model-to-camera matrix the
    // game built immediately before lighting, including the object's scale.
    public static void RestoreRigidModelRotation(CpuContext c, IMemory m)
    {
        for (int i = 0; i < 5; i++)
            RecompOne.Runtime.Gte.WriteControl(i, m.ReadU32(c.S4 + (uint)i * 4));
    }

    static short ReadBagmanDepthBias()
    {
        string? configured = Environment.GetEnvironmentVariable("RECOMP_BAGMAN_DEPTH_BIAS");
        if (string.IsNullOrWhiteSpace(configured))
            return DefaultBagmanDepthBias;
        if (!short.TryParse(configured, out short value) || value <= 0)
            throw new InvalidOperationException(
                "RECOMP_BAGMAN_DEPTH_BIAS must be a positive signed 16-bit integer");
        return value;
    }

    /// <summary>
    /// SM1's menu expression routine writes two hard-coded 38-vertex PS1 head
    /// targets directly into mesh 7.  Dreamcast heads have different topology and
    /// vertex counts. Skip the geometry loop before it reads either target or backup.
    /// The native backup is allocated only once and can belong to an earlier costume;
    /// restoring from it corrupts joint references after changing models. The native
    /// expression flags and blend state are still updated before this branch.
    /// </summary>
    public static bool SkipIncompatibleHeadMorph(CpuContext c, IMemory m)
    {
        if (c.S0 == RetailHeadVertexCount)
            return false;
        uint vertices = c.A3;
        if (Trace && _reportedHeads.Add(vertices))
        {
            Console.WriteLine(
                $"[dc-model] bypassed retail {RetailHeadVertexCount}-vertex head morph " +
                $"for mesh with {c.S0} vertices at 0x{vertices:X8}");
        }
        return true;
    }

    /// <summary>
    /// Translate Bag-Man's Dreamcast nested-shell depth relationship into SM1's
    /// native per-face ordering-table offset. The converter marks all 86 original
    /// inner head/neck faces with 0x6000; this hook recognizes the complete,
    /// topology-checked 179-face batch before supplying the selected offset.
    /// </summary>
    public static void ApplyBagmanNestedShellDepth(CpuContext c, IMemory m)
    {
        _bagmanDepthActive = false;
        if (c.A2 != BagmanHeadFaceCount)
            return;

        uint face = c.A0;
        int inner = 0;
        int outer = 0;
        for (int index = 0; index < BagmanHeadFaceCount; index++)
        {
            ushort flags = m.ReadU16(face);
            ushort length = m.ReadU16(face + 2u);
            if (length < 20 || length > 64)
                return;

            if ((flags & BagmanInnerDepthMarker) == BagmanInnerDepthMarker)
                inner++;
            else
                outer++;
            face += length;
        }
        if (inner != BagmanInnerFaceCount || outer != BagmanHeadFaceCount - BagmanInnerFaceCount)
            return;

        _savedBagmanDepth = m.ReadU16(BagmanDepthAddress);
        m.WriteU16(BagmanDepthAddress, unchecked((ushort)BagmanDepthBias));
        _bagmanDepthActive = true;
        if (Trace)
            Console.WriteLine(
                $"[dc-model] Bag-Man nested shell: inner={inner} outer={outer} " +
                $"ot-bias={BagmanDepthBias}");
    }

    public static void RestoreBagmanNestedShellDepth(CpuContext c, IMemory m)
    {
        if (!_bagmanDepthActive)
            return;
        m.WriteU16(BagmanDepthAddress, _savedBagmanDepth);
        _bagmanDepthActive = false;
    }
}
