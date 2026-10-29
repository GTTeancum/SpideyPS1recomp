using System.Runtime.InteropServices;
using Silk.NET.Core.Loader;

namespace RecompOne.Runtime.Host;

/// <summary>Resolve bundled native dependencies, including the Windows CRT payload.</summary>
public static class BundledNativeRuntime
{
    static readonly List<nint> Handles = [];
    static readonly Func<string, IEnumerable<string>> LinuxNativeResolver = name =>
        !string.IsNullOrEmpty(name) && string.IsNullOrEmpty(Path.GetDirectoryName(name))
            ? [Path.Combine(AppContext.BaseDirectory, "runtimes",
                RuntimeInformation.RuntimeIdentifier, "native", name)]
            : [];

    public static void Initialize()
    {
        if (OperatingSystem.IsLinux())
        {
            lock (Handles)
            {
                // Silk's legacy distro RID lookup misses Ubuntu in modern .NET
                // runtime graphs. Add the portable RID without replacing its
                // existing filename/version resolvers or the caller's environment.
                var resolver = (DefaultPathResolver)PathResolver.Default;
                if (!resolver.Resolvers.Contains(LinuxNativeResolver))
                    resolver.Resolvers.Add(LinuxNativeResolver);
            }
            return;
        }

        if (!OperatingSystem.IsWindows()) return;
        lock (Handles)
        {
            if (Handles.Count != 0) return;
            foreach (string name in new[] { "vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll" })
                Handles.Add(NativeLibrary.Load(Path.Combine(AppContext.BaseDirectory, name)));
        }
    }
}
