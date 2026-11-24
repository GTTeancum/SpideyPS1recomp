using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;

namespace Recompiled;

public static class RightStickOrbit
{
    static readonly StickOrbit GameplayStick = new(180), ViewerStick = new();

    // 8007D9B4: before the follow camera builds its offset from yaw. Collision,
    // look-at, movement-relative heading and rendering still run through retail code.
    public static void Gameplay(CpuContext c, IMemory m)
    {
        uint camera = c.S1, player = m.ReadU32(0x800C21F4);
        if (!Valid(camera) || !Valid(player) || camera != m.ReadU32(0x800C2994) ||
            m.ReadU32(0x800C1880) != 0 || m.ReadU32(camera + 0xDC) == 0 ||
            m.ReadU32(camera + 0x288) != 3 || m.ReadU32(camera + 0xE4) != player ||
            m.ReadU32(player + 0x190) != 0)
        {
            GameplayStick.Reset();
            return;
        }
        ViewerStick.Reset();
        if (!GameplayStick.Update(Controller.RightX, Diag.Frame, camera,
                m.ReadU16(camera + 0x21E), out ushort yaw)) return;
        m.WriteU16(camera + 0x21E, yaw);
        m.WriteU32(c.GP + 0x1540, 0);
    }

    // Costume viewer, 80249B90 -> 80249BDC: avoid adding the game's digital/analog
    // rotation to the new proportional stick input. Neutral retains native controls.
    public static bool Viewer(CpuContext c, IMemory m)
    {
        GameplayStick.Reset();
        const uint yawAddress = 0x800B327E;
        if (!ViewerStick.Update(Controller.RightX, Diag.Frame, yawAddress,
                m.ReadU16(yawAddress), out ushort yaw)) return false;
        m.WriteU16(yawAddress, yaw);
        c.SetDerived(16, 0u - c.S6, 0, 22); // Radius normally set by the skipped branch.
        return true;
    }

    static bool Valid(uint address) => address >= 0x80000000 && address < 0x807FF000;
}
