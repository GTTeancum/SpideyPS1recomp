using System.Runtime.InteropServices;
using RecompOne.Runtime.Host;
using Silk.NET.Core.Loader;
using Silk.NET.GLFW;

// Exercise the application's real native loader. No game assets, GLFW
// initialization, display server, window, or audio device are needed.
if (!OperatingSystem.IsLinux())
{
    Console.WriteLine("SKIP: Linux native-library lookup regression");
    return 0;
}

int failures = 0;
void Check(bool pass, string name)
{
    Console.WriteLine($"{(pass ? "PASS" : "FAIL")}: {name}");
    if (!pass) failures++;
}

string? previousRid = Environment.GetEnvironmentVariable("DOTNET_RUNTIME_ID");
try
{
    // Reproduce the affected distro RID even on Linux CI images whose legacy
    // fallback happens to work. The application must preserve an explicit RID.
    const string UbuntuRid = "ubuntu.24.04-x64";
    Environment.SetEnvironmentVariable("DOTNET_RUNTIME_ID", UbuntuRid);
    var resolver = (DefaultPathResolver)PathResolver.Default;
    var originalList = resolver.Resolvers;
    var originalResolvers = originalList.ToArray();

    BundledNativeRuntime.Initialize();
    Check(ReferenceEquals(resolver.Resolvers, originalList) &&
        resolver.Resolvers.Take(originalResolvers.Length).SequenceEqual(originalResolvers),
        "initialization preserves all existing Silk resolvers in their original order");
    Check(Environment.GetEnvironmentVariable("DOTNET_RUNTIME_ID") == UbuntuRid,
        "initialization preserves the caller's runtime identifier environment variable");

    int resolverCount = resolver.Resolvers.Count;
    BundledNativeRuntime.Initialize();
    Check(resolver.Resolvers.Count == resolverCount,
        "repeated initialization does not register duplicate search paths");

    string bundledGlfw = Path.Combine(AppContext.BaseDirectory, "runtimes",
        RuntimeInformation.RuntimeIdentifier, "native", "libglfw.so.3");
    Check(File.Exists(bundledGlfw), "NuGet supplies the portable Linux GLFW native library");
    // Use the name requested by the installed GLFW binding, including its older
    // minor suffix; Silk must retain its existing version-name fallback too.
    var candidates = resolver.EnumeratePossibleLibraryLoadTargets("libglfw.so.3.3").ToArray();
    Check(candidates.Contains(bundledGlfw),
        "the real GLFW lookup reaches the bundled library despite an unmapped Ubuntu RID");

    try
    {
        using var glfw = Glfw.GetApi();
        glfw.GetVersion(out int major, out int minor, out int revision);
        Check(major >= 3 && minor >= 0 && revision >= 0,
            "GLFW loads and exports its native version without a display");
        Console.WriteLine($"GLFW native version: {major}.{minor}.{revision}");
    }
    catch (Exception e)
    {
        Check(false, $"GLFW native load failed: {e.GetType().Name}: {e.Message}");
    }
}
finally
{
    Environment.SetEnvironmentVariable("DOTNET_RUNTIME_ID", previousRid);
}

return failures == 0 ? 0 : 1;
