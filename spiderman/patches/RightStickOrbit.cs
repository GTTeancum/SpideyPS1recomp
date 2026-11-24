using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;

namespace Recompiled;

public static class RightStickOrbit
{
    static readonly StickOrbit GameplayStick = new(180), ViewerStick = new();

    // 80071864: before the follow camera builds its offset from yaw. Collision,
    // look-at, movement-relative heading and rendering still run through retail code.
    public static void Gameplay(CpuContext c, IMemory m)
    {
        uint camera = c.S1, player = m.ReadU32(0x800B5268);
        if (!Valid(camera) || !Valid(player) || camera != m.ReadU32(0x800B5834) ||
            m.ReadU32(0x800B4990) != 0 || m.ReadU32(camera + 0xF4) == 0 ||
            m.ReadU32(camera + 0x2A0) != 3 || m.ReadU32(camera + 0xFC) != player ||
            m.ReadU32(player + 0x1A8) != 0)
        {
            GameplayStick.Reset();
            return;
        }
        ViewerStick.Reset();
        if (!GameplayStick.Update(Controller.RightX, Diag.Frame, camera,
                m.ReadU16(camera + 0x236), out ushort yaw)) return;
        m.WriteU16(camera + 0x236, yaw);
        // The native auto-follow may request a new heading each update. While the
        // stick is held, use the accumulated manual heading and cancel that tween.
        m.WriteU32(c.GP + 0x136C, 0);
    }

    // Costume viewer, 80262568 -> 802625B4: the stick takes priority over native
    // rotation input while held; neutral leaves the original buttons available.
    public static bool Viewer(CpuContext c, IMemory m)
    {
        GameplayStick.Reset();
        const uint yawAddress = 0x800A5786;
        if (!ViewerStick.Update(Controller.RightX, Diag.Frame, yawAddress,
                m.ReadU16(yawAddress), out ushort yaw)) return false;
        m.WriteU16(yawAddress, yaw);
        c.SetDerived(5, 0u - c.S6, 0, 22); // Radius normally set by the skipped branch.
        return true;
    }

    static bool Valid(uint address) => address >= 0x80000000 && address < 0x807FF000;
}
