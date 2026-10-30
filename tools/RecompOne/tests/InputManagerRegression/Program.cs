using System.Reflection;
using RecompOne.Runtime.Config;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Host;
using Silk.NET.Input;
using Silk.NET.SDL;

// Exercise the actual host poll without a window, physical controller, SDL
// initialization, or retail game. The keyboard double only supplies key state;
// analog resets, button merging and replay precedence run in production code.
var input = typeof(GameConfig).Assembly.GetType("RecompOne.Runtime.Host.InputManager", throwOnError: true)!;
var poll = input.GetMethod("Poll", BindingFlags.Public | BindingFlags.Static)!;
var keyboardField = input.GetField("_keyboard", BindingFlags.NonPublic | BindingFlags.Static)!;
void Poll() => poll.Invoke(null, null);

int checks = 0, failures = 0;
void Check(bool condition, string name)
{
    checks++;
    if (!condition) failures++;
    Console.WriteLine($"{(condition ? "PASS" : "FAIL")}: {name}");
}
static bool FirstNeutral() => Controller.LeftX == 0x80 && Controller.LeftY == 0x80 &&
    Controller.RightX == 0x80 && Controller.RightY == 0x80;
static bool SecondNeutral() => Controller.LeftX2 == 0x80 && Controller.LeftY2 == 0x80 &&
    Controller.RightX2 == 0x80 && Controller.RightY2 == 0x80;
static void SeedOldAxes()
{
    Controller.LeftX = 0;
    Controller.LeftY = 255;
    Controller.RightX = 17;
    Controller.RightY = 231;
    Controller.LeftX2 = 239;
    Controller.LeftY2 = 3;
    Controller.RightX2 = 211;
    Controller.RightY2 = 19;
}

var originalKeys = ConfigManager.Game.Keys;
var originalKeys2 = ConfigManager.Game.Keys2;
try
{
    ConfigManager.Game.Keys = new KeyBindings();
    ConfigManager.Game.Keys2 = KeyBindings.Empty();
    Controller.ScriptHeld = 0;
    Controller.ScriptExclusive = false;
    Controller.ReplayActive = false;
    Controller.State = Controller.State2 = 0;
    Controller.Connected2 = true;
    SeedOldAxes();
    Poll();
    Check(FirstNeutral(), "absent host input releases all first-player analog axes");
    Check(SecondNeutral(), "absent host input releases all second-player analog axes");
    Check(Controller.State == 0xFFFF && Controller.State2 == 0xFFFF,
        "absent keyboard and gamepads release digital buttons");
    Check(!Controller.Connected2, "empty second-player bindings do not create a connected controller");

    var keyboard = DispatchProxy.Create<IKeyboard, TestKeyboard>();
    var keys = (TestKeyboard)(object)keyboard;
    keys.Pressed.UnionWith([Key.Z, Key.Left, Key.V]);
    keyboardField.SetValue(null, keyboard);
    ConfigManager.Game.Keys2.Cross = "V";
    SeedOldAxes();
    Poll();
    Check(Controller.State == (ushort)(0xFFFF & ~(Controller.Cross | Controller.Left)),
        "analog release preserves first-player keyboard buttons");
    Check(Controller.Connected2 && Controller.State2 == (ushort)(0xFFFF & ~Controller.Cross),
        "second-player keyboard bindings remain independent and connected");
    Check(FirstNeutral() && SecondNeutral(), "keyboard-only polling leaves both sets of sticks centered");

    Controller.ScriptHeld = Controller.Square;
    Poll();
    Check(Controller.State == (ushort)(0xFFFF & ~(Controller.Cross | Controller.Left | Controller.Square)),
        "script-held digital buttons still merge with keyboard state");
    Controller.ScriptHeld = 0;

    Controller.ReplayActive = true;
    Controller.ReplayState = (ushort)(0xFFFF & ~Controller.Circle);
    Controller.ReplayLeftX = 7;
    Controller.ReplayLeftY = 31;
    Controller.ReplayRightX = 230;
    Controller.ReplayRightY = 99;
    Poll();
    Check(Controller.State == Controller.ReplayState && Controller.LeftX == 7 &&
        Controller.LeftY == 31 && Controller.RightX == 230 && Controller.RightY == 99,
        "active replay overrides host analog reset and keyboard state");
    Controller.ReplayActive = false;
    keys.Pressed.Clear();
    Poll();
    Check(FirstNeutral(), "ending an analog replay without a gamepad releases its stick positions");
    Check(Controller.State == 0xFFFF, "ending replay releases its digital buttons");

    keys.Pressed.UnionWith([Key.Z, Key.V]);
    Controller.ScriptExclusive = true;
    Controller.ScriptHeld = Controller.Square;
    SeedOldAxes();
    Poll();
    Check(Controller.State == (ushort)(0xFFFF & ~Controller.Square) &&
        Controller.State2 == 0xFFFF && !Controller.Connected2,
        "exclusive script still suppresses host keyboard controls for both players");
    Check(FirstNeutral() && SecondNeutral(), "exclusive script leaves both players' sticks centered");
}
finally
{
    keyboardField.SetValue(null, null);
    ConfigManager.Game.Keys = originalKeys;
    ConfigManager.Game.Keys2 = originalKeys2;
    Controller.ScriptHeld = 0;
    Controller.ScriptExclusive = false;
    Controller.ReplayActive = false;
}

if (args.Contains("--sdl-virtual", StringComparer.Ordinal))
    RunVirtualGamepad(input, Poll, Check);
else
    Console.WriteLine("SKIP: optional SDL virtual-controller check (enable with --sdl-virtual)");

Console.WriteLine($"{checks - failures}/{checks} input checks passed; {failures} failed.");
return failures == 0 ? 0 : 1;

static unsafe void RunVirtualGamepad(Type input, Action poll, Action<bool, string> check)
{
    // An SDL virtual device exercises the real live-device and removal paths.
    // It requires the native SDL library, but no physical input device or display.
    BundledNativeRuntime.Initialize();
    using var sdl = Sdl.GetApi();
    var sdlField = input.GetField("_sdl", BindingFlags.NonPublic | BindingFlags.Static)!;
    var refresh = input.GetMethod("RefreshDevices", BindingFlags.Public | BindingFlags.Static)!;
    var close = input.GetMethod("CloseControllers", BindingFlags.NonPublic | BindingFlags.Static)!;
    var connected = input.GetProperty("IsConnected", BindingFlags.Public | BindingFlags.Static)!;
    Joystick* joystick = null;
    int index = -1;
    bool attached = false;
    try
    {
        if (sdl.InitSubSystem(Sdl.InitGamecontroller) != 0)
            throw new InvalidOperationException("SDL controller initialization failed: " + sdl.GetErrorS());
        if (sdl.NumJoysticks() != 0)
            throw new InvalidOperationException("virtual-controller fixture requires no other joysticks");
        index = sdl.JoystickAttachVirtual(JoystickType.Gamecontroller, 6, 15, 0);
        if (index < 0)
            throw new InvalidOperationException("virtual joystick attach failed: " + sdl.GetErrorS());
        attached = true;
        joystick = sdl.JoystickOpen(index);
        if (joystick == null || sdl.IsGameController(index) != SdlBool.True)
            throw new InvalidOperationException("SDL did not expose the virtual device as a game controller");
        sdlField.SetValue(null, sdl);
        refresh.Invoke(null, null);
        sdl.JoystickSetVirtualAxis(joystick, 0, short.MaxValue);
        sdl.JoystickSetVirtualAxis(joystick, 1, short.MinValue);
        sdl.JoystickSetVirtualAxis(joystick, 2, short.MinValue);
        sdl.JoystickSetVirtualAxis(joystick, 3, short.MaxValue);
        sdl.JoystickSetVirtualButton(joystick, 0, 1);
        sdl.JoystickUpdate();
        poll();
        check((bool)connected.GetValue(null)!, "SDL virtual gamepad connects through production device discovery");
        check(Controller.LeftX == 255 && Controller.LeftY == 0 &&
            Controller.RightX == 0 && Controller.RightY == 255,
            "live gamepad axes replace neutral defaults during the same poll");
        check((Controller.State & Controller.Cross) == 0, "live gamepad button remains pressed");

        if (sdl.JoystickDetachVirtual(index) != 0)
            throw new InvalidOperationException("virtual joystick detach failed: " + sdl.GetErrorS());
        attached = false;
        sdl.JoystickClose(joystick);
        joystick = null;
        poll();
        check(!(bool)connected.GetValue(null)!, "SDL removal event disconnects the virtual gamepad");
        check(FirstNeutral(), "removing a tilted gamepad releases all first-player analog axes");
        check(Controller.State == 0xFFFF, "removing the gamepad releases its digital button");
    }
    catch (Exception e)
    {
        check(false, $"SDL virtual-controller fixture: {e.GetType().Name}: {e.Message}");
    }
    finally
    {
        close.Invoke(null, null);
        sdlField.SetValue(null, null);
        if (joystick != null) sdl.JoystickClose(joystick);
        if (attached) sdl.JoystickDetachVirtual(index);
        sdl.QuitSubSystem(Sdl.InitGamecontroller);
    }
}

public class TestKeyboard : DispatchProxy
{
    public HashSet<Key> Pressed { get; } = [];

    protected override object? Invoke(MethodInfo? targetMethod, object?[]? args)
    {
        if (targetMethod?.Name == nameof(IKeyboard.IsKeyPressed))
            return Pressed.Contains((Key)args![0]!);
        throw new NotSupportedException($"Unexpected keyboard operation: {targetMethod?.Name}");
    }
}
