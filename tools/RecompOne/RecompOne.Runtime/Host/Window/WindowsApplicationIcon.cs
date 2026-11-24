using System.Runtime.InteropServices;

namespace RecompOne.Runtime.Host.Window;

internal static class WindowsApplicationIcon
{
    // The C# compiler assigns this group ID to the project's ApplicationIcon.
    const int ApplicationIconId = 32512;
    const uint ImageIcon = 1, LoadShared = 0x8000, WmSetIcon = 0x0080;
    const int ClassIcon = -14, ClassSmallIcon = -34;

    internal static void Apply(nint hwnd)
    {
        if (RuntimePaths.IsDotnetHost(Environment.ProcessPath)) return;

        nint module = GetModuleHandleW(null);
        uint dpi = GetDpiForWindow(hwnd);
        Install(ClassIcon, 1, 11, 12);      // ICON_BIG, SM_CXICON, SM_CYICON
        Install(ClassSmallIcon, 0, 49, 50); // ICON_SMALL, SM_CXSMICON, SM_CYSMICON

        void Install(int classSlot, int windowSlot, int widthMetric, int heightMetric)
        {
            nint icon = LoadImageW(module, ApplicationIconId, ImageIcon,
                GetSystemMetricsForDpi(widthMetric, dpi), GetSystemMetricsForDpi(heightMetric, dpi), LoadShared);
            if (icon == 0) return;

            // GLFW's default class icon is the generic Windows application icon.
            // Shared resource handles remain valid for the lifetime of the process.
            if (IntPtr.Size == 8) SetClassLongPtrW(hwnd, classSlot, icon);
            else SetClassLongW(hwnd, classSlot, icon);
            SendMessageW(hwnd, WmSetIcon, windowSlot, icon);
        }
    }

    [DllImport("kernel32.dll", ExactSpelling = true, CharSet = CharSet.Unicode)]
    static extern nint GetModuleHandleW(string? moduleName);
    [DllImport("user32.dll", ExactSpelling = true)]
    static extern uint GetDpiForWindow(nint hwnd);
    [DllImport("user32.dll", ExactSpelling = true)]
    static extern int GetSystemMetricsForDpi(int index, uint dpi);
    [DllImport("user32.dll", ExactSpelling = true)]
    static extern nint LoadImageW(nint module, nint name, uint type, int width, int height, uint flags);
    [DllImport("user32.dll", ExactSpelling = true)]
    static extern nint SetClassLongPtrW(nint hwnd, int index, nint value);
    [DllImport("user32.dll", ExactSpelling = true)]
    static extern uint SetClassLongW(nint hwnd, int index, nint value);
    [DllImport("user32.dll", ExactSpelling = true)]
    static extern nint SendMessageW(nint hwnd, uint message, nint wParam, nint lParam);
}
