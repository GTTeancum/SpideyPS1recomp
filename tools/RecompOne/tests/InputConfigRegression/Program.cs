using System.Reflection;
using System.Runtime.ExceptionServices;
using System.Text.Json;
using RecompOne.Runtime.Config;
using RecompOne.Runtime.Hardware;

// Exercise the actual settings loader and input consumers. No window, game data,
// physical devices, SDL initialization, or replacement InputManager is involved.
var input = typeof(ConfigManager).Assembly.GetType("RecompOne.Runtime.Host.InputManager", throwOnError: true)!;
var poll = input.GetMethod("Poll", BindingFlags.Static | BindingFlags.Public)!;
var padState = input.GetMethod("PadState", BindingFlags.Static | BindingFlags.NonPublic)!;
var oldDirectory = Environment.CurrentDirectory;
var directory = Path.Combine(Path.GetTempPath(), "spidey-input-config-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(directory);
var cases = new List<(string Name, Action Run)>();

cases.Add(("null player-two keyboard object permits polling", () =>
{
    Load("""{"Keys2":null}""");
    Poll();
    Check(!Controller.Connected2, "missing player-two keyboard must stay unbound");
}));
cases.Add(("null player-two key permits polling", () =>
{
    Load("""{"Keys2":{"Cross":null}}""");
    Poll();
    Check(!Controller.Connected2, "null/omitted player-two keys must not enable a second controller");
}));
cases.Add(("null gamepad binding permits actual button-state evaluation", () =>
{
    Load("""{"Pad":{"Cross":null}}""");
    Check(PadState(ConfigManager.Game.Pad) == 0xffff, "no device may press a button");
    Check(ConfigManager.Game.Pad.Cross.SequenceEqual([0]), "null P1 Cross should restore its default");
}));
cases.Add(("partial player-two objects retain only explicit mappings", () =>
{
    Load("""{"Keys2":{"Cross":"P","Circle":""},"Pad2":{"Cross":[5],"Circle":[]}}""");
    var config = ConfigManager.Game;
    Check(config.Keys2.Cross == "P" && config.Keys2.Circle == "" && config.Keys2.Up == "", "partial P2 keys inherited P1 controls");
    Check(config.Pad2.Cross.SequenceEqual([5]) && config.Pad2.Circle.Length == 0 && config.Pad2.Up.Length == 0, "partial P2 pad inherited P1 controls or appended defaults");
    Poll();
    Check(Controller.Connected2, "explicit P2 key should enable the second controller");
    Check(PadState(config.Pad2) == 0xffff, "unavailable P2 device should remain released");
}));
cases.Add(("empty player-two objects stay unbound", () =>
{
    Load("""{"Keys2":{},"Pad2":{}}""");
    CheckAllEmpty(ConfigManager.Game.Keys2, ConfigManager.Game.Pad2);
    Poll();
    Check(!Controller.Connected2, "empty P2 object enabled a controller");
}));
cases.Add(("null input objects restore per-player defaults", () =>
{
    Load("""{"Keys":null,"Keys2":null,"Pad":null,"Pad2":null,"PadDevice":null,"PadDevice2":null}""");
    var config = ConfigManager.Game;
    Check(config.Keys.Cross == "Z" && config.Keys.Start == "Enter", "P1 key defaults lost");
    Check(config.Pad.Cross.SequenceEqual([0]) && config.Pad.Up.SequenceEqual([11, 104]), "P1 pad defaults lost");
    Check(config.PadDevice == "" && config.PadDevice2 == "", "null device IDs not normalized");
    CheckAllEmpty(config.Keys2, config.Pad2);
    Poll();
    Check(PadState(config.Pad) == 0xffff && PadState(config.Pad2) == 0xffff, "default pad state is not released");
}));
cases.Add(("every null binding is repaired without erasing valid settings", () =>
{
    var keyNulls = typeof(KeyBindings).GetProperties().ToDictionary(p => p.Name, _ => (object?)null);
    var padNulls = typeof(GamepadBindings).GetProperties().ToDictionary(p => p.Name, _ => (object?)null);
    Load(JsonSerializer.Serialize(new { Keys = keyNulls, Keys2 = keyNulls, Pad = padNulls, Pad2 = padNulls,
        MasterVolume = 0.25f, Muted = true, Widescreen = false, CardAEnabled = false,
        CdPath = "synthetic-disc", CardAPath = "synthetic-card.sav", ActiveMods = new[] { "synthetic-suit" }, PadDevice = "chosen-pad" }));
    var config = ConfigManager.Game;
    foreach (var property in typeof(KeyBindings).GetProperties())
        Check(property.GetValue(config.Keys) is string && property.GetValue(config.Keys2) is string, property.Name + " is still null");
    foreach (var property in typeof(GamepadBindings).GetProperties())
        Check(property.GetValue(config.Pad) is int[] && property.GetValue(config.Pad2) is int[], property.Name + " is still null");
    CheckAllEmpty(config.Keys2, config.Pad2);
    Check(config.MasterVolume == 0.25f && config.Muted && config.Widescreen == false && !config.CardAEnabled, "valid game settings changed");
    Check(config.CdPath == "synthetic-disc" && config.CardAPath == "synthetic-card.sav" && config.PadDevice == "chosen-pad", "valid paths/device changed");
    Check(config.ActiveMods.SequenceEqual(["synthetic-suit"]), "valid mod selection changed");
    Poll();
    Check(PadState(config.Pad) == 0xffff && PadState(config.Pad2) == 0xffff, "normalized pads cannot be consumed");
}));
cases.Add(("explicit empty bindings survive load and save/reload", () =>
{
    Load(JsonSerializer.Serialize(new { Keys = KeyBindings.Empty(), Keys2 = KeyBindings.Empty(), Pad = GamepadBindings.Empty(), Pad2 = GamepadBindings.Empty() }));
    CheckAllEmpty(ConfigManager.Game.Keys, ConfigManager.Game.Pad);
    CheckAllEmpty(ConfigManager.Game.Keys2, ConfigManager.Game.Pad2);
    ConfigManager.SaveGame();
    ConfigManager.Load();
    CheckAllEmpty(ConfigManager.Game.Keys, ConfigManager.Game.Pad);
    CheckAllEmpty(ConfigManager.Game.Keys2, ConfigManager.Game.Pad2);
    Poll();
    Check(!Controller.Connected2, "saved unbound P2 enabled a controller");
}));
cases.Add(("custom arrays replace defaults and round-trip intact", () =>
{
    Load("""{"Keys":{"Cross":"B","Circle":"","Square":null},"Pad":{"Cross":[3,7],"Circle":[],"Square":null},"Pad2":{"Up":[15,106],"Down":null}}""");
    var config = ConfigManager.Game;
    Check(config.Keys.Cross == "B" && config.Keys.Circle == "" && config.Keys.Square == "A", "custom keys not preserved");
    Check(config.Pad.Cross.SequenceEqual([3, 7]) && config.Pad.Circle.Length == 0 && config.Pad.Square.SequenceEqual([2]), "custom pad arrays were appended or replaced");
    Check(config.Pad2.Up.SequenceEqual([15, 106]) && config.Pad2.Down.Length == 0 && config.Pad2.Cross.Length == 0, "P2 partial mapping not preserved");
    var expected = JsonSerializer.Serialize(config);
    ConfigManager.SaveGame();
    ConfigManager.Load();
    Check(JsonSerializer.Serialize(ConfigManager.Game) == expected, "save/reload altered mappings");
}));
cases.Add(("default and invalid-document recovery remain compatible", () =>
{
    foreach (var json in new[] { "{}", "null", "{broken-json" })
    {
        Load(json);
        Check(ConfigManager.Game.Keys.Cross == "Z" && ConfigManager.Game.Pad.Cross.SequenceEqual([0]), "default P1 controls changed");
        CheckAllEmpty(ConfigManager.Game.Keys2, ConfigManager.Game.Pad2);
        Poll();
        Check(!Controller.Connected2, "fallback enabled player two");
    }
}));

int passed = 0, failed = 0;
try
{
    Environment.CurrentDirectory = directory;
    foreach (var test in args.Contains("--repro") ? cases.Take(3) : cases)
    {
        try { test.Run(); passed++; Console.WriteLine("PASS " + test.Name); }
        catch (Exception e) { failed++; Console.WriteLine($"FAIL {test.Name}: {e.GetType().Name}: {e.Message}"); }
    }
    Console.WriteLine($"Input configuration: {passed} passed, {failed} failed");
}
finally
{
    Environment.CurrentDirectory = oldDirectory;
    Directory.Delete(directory, recursive: true);
}
return failed == 0 ? 0 : 1;

static void Check(bool value, string message)
{
    if (!value) throw new InvalidOperationException(message);
}

static void Load(string json)
{
    File.WriteAllText("settings.json", json);
    ConfigManager.Load();
    Check(File.ReadAllText("settings.json") == json, "loading rewrote the user's settings file");
}

void Poll() => Invoke(poll, []);

unsafe ushort PadState(GamepadBindings config) => (ushort)Invoke(padState,
    [Pointer.Box((void*)0, padState.GetParameters()[0].ParameterType), config, (ushort)0xffff])!;

static object? Invoke(MethodInfo method, object?[] arguments)
{
    try { return method.Invoke(null, arguments); }
    catch (TargetInvocationException e) when (e.InnerException != null)
    {
        ExceptionDispatchInfo.Capture(e.InnerException).Throw();
        throw;
    }
}

static void CheckAllEmpty(KeyBindings keys, GamepadBindings pad)
{
    foreach (var property in typeof(KeyBindings).GetProperties())
        Check(property.GetValue(keys) is "", property.Name + " should remain an unbound key");
    foreach (var property in typeof(GamepadBindings).GetProperties())
        Check(property.GetValue(pad) is int[] { Length: 0 }, property.Name + " should remain an unbound button");
}
