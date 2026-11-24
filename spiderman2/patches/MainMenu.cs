using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;

namespace Recompiled;

public static class MainMenu
{
    static readonly NativeMainMenu Menu = new(new(0x802506B0, 0x800B29C8,
        0x8001C838, 0x8023D554, 0x8006E814, Sm2: true));
    public static void Enter(CpuContext c, IMemory m) => Menu.Enter(c, m);
    public static void Leave(CpuContext c, IMemory m) => Menu.Leave(c, m);
    public static void DrawUnselected(CpuContext c, IMemory m) => Menu.DrawUnselected(c, m);
    public static bool DrawSelected(CpuContext c, IMemory m) => Menu.DrawSelected(c, m);
    public static bool Input(CpuContext c, IMemory m) => Menu.Input(c, m);
}
