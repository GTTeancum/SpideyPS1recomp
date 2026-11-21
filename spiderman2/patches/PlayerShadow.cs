using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>Enter Electro bindings for the shared N64 player silhouette.</summary>
public static partial class PlayerShadow
{
    const uint PlayerPointer = 0x800C21F4, ModelTable = 0x800ACED8;
    // Ground query returns at 8003C528. State and attachment are read by
    // the matching physics code at 8003B938 and 8003B8C8.
    const uint GroundQueryOffset = 0x40, PlayerStateOffset = 0xFB4, AttachmentOffset = 0xC10;
    const uint CameraOffset = 0x1310, PrecisionOffset = 0x1358;
    const uint FrameOffset = 0xD18, CursorOffset = 0xD20;

    static uint ViewRotation(uint camera) => camera + 0x74;
    static bool ScriptHidden => false;
    static void ResetScript() { }
    static ShadowScene ShadowSceneForLevel(IMemory m)
    {
        // The retail descriptor loader writes this four-character stage prefix.
        // TR03 and its WR03 variant are the zip-line training courses.
        const uint currentLevel = 0x800B3184;
        uint prefix = m.ReadU32(currentLevel) | 0x2020u;
        const uint tr03 = 't' | ('r' << 8) | ('0' << 16) | ('3' << 24);
        const uint wr03 = 'w' | ('r' << 8) | ('0' << 16) | ('3' << 24);
        return prefix is tr03 or wr03 ? ShadowScene.ZipLineTraining : ShadowScene.Ordinary;
    }

    static void InitRay(CpuContext c, IMemory m) => SpiderMan2.func_8007F2D4(c, m);
    static void CastRay(CpuContext c, IMemory m) => SpiderMan2.func_800818F4(c, m);

    public static bool UpdateBlob(CpuContext c, IMemory m)
    {
        if (Mode == "off" || c.A0 == 0 || c.A0 != m.ReadU32(PlayerPointer)) return true;
        SpiderMan2.func_8006335C(c, m);
        return false;
    }

    // After the original prologue, take the retail cleanup branch. It clears
    // visibility and disposes both owned quads using their virtual destructors.
    public static bool ClearPlayerQuads(CpuContext c, IMemory m)
    {
        if (Mode == "off" || c.S2 == 0 || c.S2 != m.ReadU32(PlayerPointer)) return false;
        c.S4 = 0;
        return true;
    }

    static uint PacketLimit(CpuContext c, IMemory m) => m.ReadU32(c.GP + 0x8D0);
}
