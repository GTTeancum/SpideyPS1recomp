using System.Text;
using RecompOne.Runtime.Assets;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Dispatch;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;

namespace RecompOne.Runtime.Host.Window;

/// <summary>Adds a top-center exit choice using the retail menu's fonts and highlight.</summary>
public sealed class NativeMainMenu(NativeMainMenu.Bindings bindings)
{
    public sealed record Bindings(uint Table, uint Pad, uint Text, uint Highlight,
        uint Sound, bool Sm2 = false);
    readonly Bindings _b = bindings;
    const uint ExitY = 12; // Fixed top margin; leave room for the jumping preview.
    uint _text;
    bool _exitSelected;
    bool _waitForRelease;
    int _held;

    public void Enter(CpuContext c, IMemory m)
    {
        Leave(c, m);
        Console.WriteLine("[main-menu] EXIT GAME available above Spider-Man");
    }

    public void Leave(CpuContext c, IMemory m)
    {
        if (_text != 0) LooseWadOverrides.TryFree(_text);
        _text = 0;
        _exitSelected = false;
        _waitForRelease = false;
        _held = 0;
    }

    uint Selected(CpuContext c) => _b.Sm2 ? c.S5 : c.S6;
    void Select(CpuContext c, uint value)
    {
        if (_b.Sm2) c.S5 = value; else c.S6 = value;
    }

    void Label(IMemory m)
    {
        if (_text != 0) return;
        _text = LooseWadOverrides.AllocateScratch(16, "main menu exit label");
        byte[] label = Encoding.ASCII.GetBytes("exit game\0");
        for (int i = 0; i < label.Length; i++) m.WriteU8(_text + (uint)i, label[i]);
    }

    void DrawText(CpuContext c, IMemory m, uint x, uint y, uint text, bool highlight)
    {
        c.A0 = x; c.A1 = y; c.A2 = text; c.A3 = 0;
        m.WriteU32(c.SP + 0x10, highlight ? 184u : 0x1000u);
        if (highlight)
        {
            m.WriteU32(c.SP + 0x14, 20);
            m.WriteU32(c.SP + 0x18, unchecked((uint)-20));
            m.WriteU32(c.SP + 0x1C, 29);
        }
        Dispatcher.Call(c, m, highlight ? _b.Highlight : _b.Text);
    }

    // Runs after the eight unselected entries, while their font/color is active.
    public void DrawUnselected(CpuContext c, IMemory m)
    {
        Label(m);
        var saved = c.Snapshot();
        try
        {
            c.SP -= 128;
            if (_exitSelected)
            {
                uint row = _b.Table + 40 * Selected(c);
                DrawText(c, m, m.ReadU32(row), m.ReadU32(row + 4), m.ReadU32(row + 8), false);
            }
            else DrawText(c, m, 256, ExitY, _text, false);
        }
        finally { c.Restore(saved); }
    }

    // When exit owns focus, draw it through the same animated highlight function.
    // Its band is bounded to the middle column, clear of Continue and Training.
    public bool DrawSelected(CpuContext c, IMemory m)
    {
        if (!_exitSelected) return false;
        Label(m);
        var saved = c.Snapshot();
        try
        {
            c.SP -= 128;
            DrawText(c, m, 256, ExitY, _text, true);
        }
        finally { c.Restore(saved); }
        return true;
    }

    // No ninth entry is indexed into the retail eight-entry model/preview table.
    // The original selection stays valid while the new choice owns input focus.
    public bool Input(CpuContext c, IMemory m)
    {
        uint selected = Selected(c);
        bool up = m.ReadU8(_b.Pad + 0xA0) != 0 || Controller.LeftY < 103;
        bool down = m.ReadU8(_b.Pad + 0xB0) != 0 || Controller.LeftY > 152;
        bool left = m.ReadU8(_b.Pad + 0x80) != 0 || Controller.LeftX < 103;
        bool right = m.ReadU8(_b.Pad + 0x90) != 0 || Controller.LeftX > 152;
        int keys = (up ? 1 : 0) | (down ? 2 : 0) | (left ? 4 : 0) | (right ? 8 : 0);
        int pressed = keys & ~_held;
        _held = keys;
        if (_waitForRelease)
        {
            if (keys != 0) return ConsumeNavigation(c, m, selected);
            _waitForRelease = false;
        }
        if (!_exitSelected)
        {
            if (!(((pressed & 1) != 0 && selected % 4 == 0) ||
                  ((pressed & 2) != 0 && selected % 4 == 3))) return false;
            _exitSelected = true;
            Sound(c, m);
            Console.WriteLine("[main-menu] EXIT GAME selected");
        }
        else
        {
            if (m.ReadU8(_b.Pad + 0x31) != 0 || m.ReadU8(_b.Pad + 0xE1) != 0)
            {
                Console.WriteLine("[main-menu] EXIT GAME confirmed");
                Runtime.Shutdown();
                Environment.Exit(0);
            }
            if (pressed != 0 || m.ReadU8(_b.Pad + 1) != 0)
            {
                uint column = selected < 4 ? 0u : 4u;
                Select(c, (pressed & 4) != 0 ? 0u : (pressed & 8) != 0 ? 4u :
                    column + ((pressed & 1) != 0 ? 3u : 0u));
                _exitSelected = false;
                _waitForRelease = true;
                if (Selected(c) == selected) Sound(c, m);
                Console.WriteLine("[main-menu] returned to retail choices");
            }
        }
        return ConsumeNavigation(c, m, selected);
    }

    bool ConsumeNavigation(CpuContext c, IMemory m, uint selected)
    {
        foreach (uint offset in new uint[] { 1, 0x31, 0x81, 0x91, 0xA1, 0xB1, 0xE1 })
            m.WriteU8(_b.Pad + offset, 0);
        m.WriteU32(c.SP + 0x4C, 0); // Reset native repeat delay while our choice owns input.
        // Restore the old-selection register expected at the native resume label.
        if (_b.Sm2) c.S0 = selected; else c.A0 = selected;
        return true;
    }

    void Sound(CpuContext c, IMemory m)
    {
        var saved = c.Snapshot();
        try
        {
            c.SP -= 128; c.A0 = 0x29; c.A1 = 0x3FFF; c.A2 = 0;
            Dispatcher.Call(c, m, _b.Sound);
        }
        finally { c.Restore(saved); }
    }
}
