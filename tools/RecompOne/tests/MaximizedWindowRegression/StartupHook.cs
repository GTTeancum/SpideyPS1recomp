using System.ComponentModel;
using System.Reflection;
using System.Runtime.InteropServices;
using RecompOne.Runtime.Config;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Host;
using RecompOne.Runtime.Hle;
using Silk.NET.Maths;
using Silk.NET.Windowing;

// DOTNET_STARTUP_HOOKS loads this fixture inside the hidden game process.
// Mark only its own hidden window maximized; asking Windows to show it
// maximized would display the game and could steal desktop focus.
public static class StartupHook
{
    const int StyleIndex = -16;
    const int MaximizedStyle = 0x01000000;
    static IWindow? window;
    static Vector2D<int> menuSize, menuFramebuffer, restoreSize;
    static long menuFrame, lastFrame;
    static bool sawWide, failed;

    [DllImport("user32.dll", EntryPoint = "GetWindowLongW", SetLastError = true)]
    static extern int GetWindowStyle(nint handle, int index);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongW", SetLastError = true)]
    static extern int SetWindowStyle(nint handle, int index, int style);

    public static void Initialize()
    {
        if (!OperatingSystem.IsWindows() || Environment.GetEnvironmentVariable("RECOMP_CAPTURE_HIDDEN") != "1" ||
            !long.TryParse(Environment.GetEnvironmentVariable("RECOMP_TEST_MAXIMIZE_AT"), out menuFrame) || menuFrame <= 0)
            throw new InvalidOperationException("The window regression requires a hidden Windows game and a menu frame.");
        Event.AddListener<VSyncEvent>(e =>
        {
            if (failed) return;
            try { Check(e); lastFrame = e.Frame; }
            catch (Exception ex)
            {
                failed = true;
                Console.Error.WriteLine("[window-test] FAIL " + ex.Message);
            }
        });
        AppDomain.CurrentDomain.ProcessExit += (_, _) =>
        {
            if (failed || !sawWide)
            {
                Console.Error.WriteLine("[window-test] FAIL transition did not pass");
                Environment.ExitCode = 1;
            }
            else Console.WriteLine($"[window-test] passed through frame {lastFrame}");
        };
    }

    static void Check(VSyncEvent e)
    {
        if (e.Frame < menuFrame) return;
        if (window == null)
        {
            window = (IWindow)typeof(HostWindow).GetField("_window", BindingFlags.NonPublic | BindingFlags.Static)!.GetValue(null)!;
            if (window.IsVisible || window.WindowState != WindowState.Normal || GpuHle.SourceAspect > 1.4f)
                throw new InvalidOperationException("Expected a hidden, normal window in the 4:3 menu.");
            var handle = window.Native!.Win32!.Value.Hwnd;
            int style = GetWindowStyle(handle, StyleIndex);
            if (style == 0 || SetWindowStyle(handle, StyleIndex, style | MaximizedStyle) == 0)
                throw new Win32Exception(Marshal.GetLastWin32Error());
            menuSize = window.Size;
            menuFramebuffer = window.FramebufferSize;
            restoreSize = new(ConfigManager.View.WindowWidth, ConfigManager.View.WindowHeight);
            Console.WriteLine($"[window-test] menu frame={e.Frame} state={window.WindowState} client={menuSize} framebuffer={menuFramebuffer} restore={restoreSize}");
        }

        if (window.IsVisible || window.WindowState != WindowState.Maximized || window.Size != menuSize ||
            window.FramebufferSize != menuFramebuffer || restoreSize != new Vector2D<int>(ConfigManager.View.WindowWidth, ConfigManager.View.WindowHeight))
            throw new InvalidOperationException($"window changed at frame {e.Frame}: state={window.WindowState} client={window.Size} framebuffer={window.FramebufferSize} restore={ConfigManager.View.WindowWidth}x{ConfigManager.View.WindowHeight}");

        if (!sawWide && GpuHle.SourceAspect > 1.7f)
        {
            sawWide = true;
            Console.WriteLine($"[window-test] gameplay frame={e.Frame} aspect={GpuHle.SourceAspect:F3} state={window.WindowState} client={window.Size} framebuffer={window.FramebufferSize}");
        }
    }
}
