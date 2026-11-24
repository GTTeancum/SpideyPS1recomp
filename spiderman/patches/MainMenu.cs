using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;

namespace Recompiled;

public static class MainMenu
{
    static readonly NativeMainMenu Menu = new(new(0x802676F4, 0x800A4DF4,
        0x80019A90, 0x8024E400, 0x80063770));
    public static void Enter(CpuContext c, IMemory m) => Menu.Enter(c, m);
    public static void Leave(CpuContext c, IMemory m) => Menu.Leave(c, m);
    public static void DrawUnselected(CpuContext c, IMemory m) => Menu.DrawUnselected(c, m);
    public static bool DrawSelected(CpuContext c, IMemory m) => Menu.DrawSelected(c, m);
    public static bool Input(CpuContext c, IMemory m) => Menu.Input(c, m);
}
