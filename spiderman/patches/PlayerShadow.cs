using System;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>Retail SM1 shadow bindings and script hooks.</summary>
public static partial class PlayerShadow
{
    const uint PlayerPointer = 0x800B5268, ModelTable = 0x800A0904;
    const uint GroundQueryOffset = 0x70, PlayerStateOffset = 0xF3C, AttachmentOffset = 0xBC4;
    const uint CameraOffset = 0x1128, PrecisionOffset = 0x1170;
    const uint FrameOffset = 0xCB4, CursorOffset = 0xCBC, PacketLimitAddress = 0x800B4FE8;

    static uint PacketLimit(CpuContext c, IMemory m) => m.ReadU32(PacketLimitAddress);
    static uint ViewRotation(uint camera) => 0x800A57CC;
    static uint _cinematic;
    static bool _scriptHidden;
    static bool ScriptHidden => _scriptHidden;
    static void ResetScript() { _scriptHidden = false; _cinematic = 0; }
    static ShadowScene ShadowSceneForLevel(IMemory m)
    {
        var (level, demo) = N64ShadowVisibility.Level(m);
        return N64ShadowVisibility.Scene(level, demo);
    }
    static void InitRay(CpuContext c, IMemory m) => SpiderMan.func_80072CF4(c, m);
    static void CastRay(CpuContext c, IMemory m) => SpiderMan.func_80075310(c, m);

    public static void ScriptCinematic(CpuContext c, IMemory m)
    {
        var (level, _) = N64ShadowVisibility.Level(m);
        // N64 bank script 16E retains the previous cinematic value (800A6914).
        if (!(level==3 && c.FP==0x16E)) _cinematic=m.ReadU16(c.S1);
        bool previous=_scriptHidden;
        _scriptHidden=N64ShadowVisibility.Script(_scriptHidden,level,c.FP,0xC3,_cinematic);
        if (Trace && previous!=_scriptHidden)
            Console.WriteLine($"[shadow-script] frame={Diag.Frame} level={level} script={c.FP:X} hidden={_scriptHidden}");
    }

    public static void ScriptPlayerAction(CpuContext c, IMemory m)
    {
        var (level, _) = N64ShadowVisibility.Level(m);
        _scriptHidden=N64ShadowVisibility.Script(_scriptHidden,level,c.FP,0xC7,_cinematic);
    }

    // Remove only the player's original CQuadBit, by ownership. NPC blobs keep
    // their native lifecycle. This also handles an already-allocated title blob.
    public static bool UpdateBlob(CpuContext c, IMemory m)
    {
        if (Mode == "off" || c.A0 == 0 || c.A0 != m.ReadU32(PlayerPointer)) return true;
        SpiderMan.func_800594AC(c, m);
        return false;
    }

    public static bool ShadowCheck(CpuContext c, IMemory m)
    {
        if (Mode == "off" || c.A0 == 0 || c.A0 != m.ReadU32(PlayerPointer)) return true;
        uint player = c.A0;
        for (uint offset = 0xBB0; offset <= 0xBB4; offset += 4)
        {
            uint quad = m.ReadU32(player + offset);
            if (quad == 0) continue;
            c.A0 = quad; c.A1 = 3;
            SpiderMan.func_8002E7A0(c, m);
            m.WriteU32(player + offset, 0);
        }
        m.WriteU8(player + 0x154, 0);
        return false;
    }
}
