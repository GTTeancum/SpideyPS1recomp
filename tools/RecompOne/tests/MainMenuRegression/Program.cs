using RecompOne.Runtime.Assets;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Dispatch;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;

int count = 0;
void Check(bool condition, string message)
{
    if (!condition) throw new Exception(message);
    count++;
    Console.WriteLine("PASS: " + message);
}
LooseWadOverrides.Initialize(Path.Combine(Path.GetFullPath(args[0]), "Spider-Man/game"));
var calls = new List<(uint Function, uint X, uint Y, uint Text)>();
var overlay = new TestOverlay();
foreach (uint address in new uint[] { 0x100, 0x104, 0x108, 0x10C, 0x110 })
    overlay.Map[address] = (c, m) => calls.Add((address, c.A0, c.A1, c.A2));
Dispatcher.Register("menu-test", overlay);
Dispatcher.Load("menu-test");
foreach (bool sm2 in new[] { false, true })
{
    const uint table = 0x80010000, pad = 0x80012000;
    var m = new PSMemory(0x800000);
    var c = new CpuContext { SP = 0x807EF000, S5 = 0, S6 = 0 };
    var menu = new NativeMainMenu(new(table, pad, 0x108, 0x10C, 0x110, sm2));
    for (uint row = 0; row < 8; row++)
    {
        m.WriteU32(table + row * 40, 105 + row);
        m.WriteU32(table + row * 40 + 4, 46 + row * 10);
        m.WriteU32(table + row * 40 + 8, 0x80013000 + row * 16);
    }
    uint Selection() => sm2 ? c.S5 : c.S6;
    void Pad(uint offset = 0, byte value = 0)
    {
        for (uint i = 0; i < 0x180; i++) m.WriteU8(pad + i, 0);
        Controller.LeftX = Controller.LeftY = 128;
        if (value != 0) m.WriteU8(pad + offset, value);
    }
    menu.Enter(c, m);
    Pad();
    Check(!menu.Input(c, m), $"SM{(sm2 ? 2 : 1)} neutral retains retail handling");
    var snapshot = c.Snapshot();
    calls.Clear();
    menu.DrawUnselected(c, m);
    Check(calls.Single().X == 256 && calls.Single().Y == 12, "exit uses the centered top margin");
    Check(c.Snapshot().gpr.SequenceEqual(snapshot.gpr), "drawing preserves guest registers and stack");
    Check(!menu.DrawSelected(c, m), "ordinary entry keeps its original highlight");
    Pad(0xA0, 1);
    Check(menu.Input(c, m) && Selection() == 0, "up from Continue focuses exit without indexing a ninth retail entry");
    calls.Clear();
    menu.DrawUnselected(c, m);
    Check(menu.DrawSelected(c, m), "exit focus bypasses the prior highlight");
    Check(calls.Count(x => x.Function == 0x10C) == 1 && calls.Last().X == 256,
        "exactly one retail highlight draws for exit");
    Check(calls.Any(x => x.Function == 0x108 && x.X == 105), "prior selected entry remains visible without highlight");
    Check(menu.Input(c, m) && Selection() == 0, "holding up cannot bounce out of exit");
    Pad(); menu.Input(c, m);
    Pad(0x90, 1); menu.Input(c, m);
    Check(Selection() == 4, "right from exit returns to Training");
    Check(menu.Input(c, m) && Selection() == 4, "held navigation cannot make an extra retail move");
    Pad();
    Check(!menu.Input(c, m), "release restores ordinary input handling");
    Pad(0xA0, 1); menu.Input(c, m);
    Pad(); menu.Input(c, m);
    Pad(0xB0, 1); menu.Input(c, m);
    Check(Selection() == 4, "down from exit returns to the previous column");
    Pad(); menu.Input(c, m);
    Pad(); Controller.LeftY = 0;
    Check(menu.Input(c, m), "left-stick up also reaches exit");
    Pad(); menu.Input(c, m);
    Pad(1, 1); menu.Input(c, m);
    Check(!menu.DrawSelected(c, m), "Triangle returns to ordinary menu choices");
    menu.Leave(c, m);
    menu.Enter(c, m);
    Check(!menu.DrawSelected(c, m), "returning from another screen clears exit focus");
    menu.Leave(c, m);
}
Console.WriteLine($"{count} menu checks passed; no game window or audio initialized.");

sealed class TestOverlay : IOverlay
{
    public string Name => "menu-test";
    public Dictionary<uint, Action<CpuContext, IMemory>> Map = new();
    public IReadOnlyDictionary<uint, Action<CpuContext, IMemory>> Functions => Map;
}
