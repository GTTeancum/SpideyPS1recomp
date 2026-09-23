using System.Reflection;

namespace RecompOne.Runtime.Host;

/// <summary>
/// Portable installs keep data beside the game, never beside the dotnet driver
/// or inside the single-file extraction cache. These paths also serve suit mods.
/// </summary>
public static class RuntimePaths
{
    public static string ApplicationDirectory => ResolveApplicationDirectory(
        Environment.ProcessPath, Assembly.GetEntryAssembly()?.Location, AppContext.BaseDirectory);

    public static string? ApplicationFile => IsDotnetHost(Environment.ProcessPath)
        ? Assembly.GetEntryAssembly()?.Location : Environment.ProcessPath;

    public static string RuntimeMutexName => OperatingSystem.IsWindows()
        ? @"Local\OpenSpideyPS1.GameRuntime" : "OpenSpideyPS1.GameRuntime";

    public static bool IsDotnetHost(string? processPath)
    {
        string name = Path.GetFileName(processPath) ?? "";
        return name.Equals("dotnet", StringComparison.OrdinalIgnoreCase) ||
               name.Equals("dotnet.exe", StringComparison.OrdinalIgnoreCase);
    }

    // Kept pure for apphost/framework-dependent/single-file regression coverage.
    public static string ResolveApplicationDirectory(string? processPath,
        string? entryLocation, string baseDirectory)
    {
        if (IsDotnetHost(processPath))
            return string.IsNullOrWhiteSpace(entryLocation)
                ? Path.GetFullPath(baseDirectory)
                : Path.GetDirectoryName(Path.GetFullPath(entryLocation))!;
        if (!string.IsNullOrWhiteSpace(processPath))
            return Path.GetDirectoryName(Path.GetFullPath(processPath))!;
        return string.IsNullOrWhiteSpace(entryLocation)
            ? Path.GetFullPath(baseDirectory)
            : Path.GetDirectoryName(Path.GetFullPath(entryLocation))!;
    }

    /// <summary>Resolve caller-supplied paths before changing working directory.</summary>
    public static string[] PrepareLaunchArguments(string[] args)
    {
        string[] result = (string[])args.Clone();
        if (result.Length != 0 && !string.IsNullOrWhiteSpace(result[0]))
            result[0] = Path.GetFullPath(result[0]);
        // Preserve the existing environment-variable contract: relative data,
        // mod and log roots are application-relative after Main anchors the CWD.
        return result;
    }
}
