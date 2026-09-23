using System;
using System.IO;
using System.Reflection;
using RecompOne.Runtime.Memory;
using Recompiled;
using Capture = Recompiled.Capture;

namespace SpiderMan;

public static class Program
{
    const string Title = "Spider-Man";
    const string BootFile = "SLUS_008.75";
    static readonly RecompOne.Runtime.Cdrom.DiscInstallProfile InstallProfile = new(
        Title,
        "Spider-Man (USA)",
        "SLUS-00875",
        BootFile,
        "OpenSpidey.BundledAssets.zip");

    /// <summary>
    /// 8 MB rather than the retail 2 MB. The extra space is not for the game -- its own
    /// allocator never looks past 2 MB -- it is where the code overlays are pinned so a
    /// static recompile can emit them at fixed addresses. See patches/OverlayPatches.cs.
    /// </summary>
    const uint RamSize = 0x00800000;
    static System.Threading.Mutex RuntimeMutex;

    public static int Main(string[] args)
    {
        // A relative disc argument belongs to the caller's directory. Resolve it
        // before anchoring portable saves/mods/logs beside the actual game binary.
        try
        {
            args = RecompOne.Runtime.Host.RuntimePaths.PrepareLaunchArguments(args);
            Directory.SetCurrentDirectory(ExeDirectory());
        }
        catch (Exception e)
        {
            Console.Error.WriteLine("[SpiderMan] launch paths: " + e.Message);
            return 2;
        }
        RecompOne.Runtime.Host.BundledNativeRuntime.Initialize();
        if (!AcquireRuntimeLease()) return 4;

        string installRoot = ExeDirectory() ?? AppContext.BaseDirectory;
        string installOutput = InstallOutput(installRoot);
        string gameData;
        try
        {
            gameData = RecompOne.Runtime.Cdrom.FirstRunDiscInstaller.EnsureInstalled(
                InstallProfile,
                installOutput,
                ResolveExistingGameData(args),
                RequestedImage(args),
                Assembly.GetExecutingAssembly());
        }
        catch (Exception e)
        {
            Console.Error.WriteLine("[SpiderMan] setup failed: " + e.Message);
            RecompOne.Runtime.Runtime.Shutdown();
            return 2;
        }
        SeedSettings(gameData);
        RecompOne.Runtime.Config.ConfigManager.Game.CdPath = gameData;
        if (string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("SPIDEY_ASSET_DIR")))
            Environment.SetEnvironmentVariable(
                "SPIDEY_ASSET_DIR",
                Path.Combine(Path.GetDirectoryName(installOutput)!, "assets", "builtin"));
        RecompOne.Runtime.Assets.LooseWadOverrides.Initialize(gameData, maxUnpackedAnimationBones: 30);
        if (string.Equals(
            Environment.GetEnvironmentVariable("RECOMP_INSTALL_ONLY"), "1",
            StringComparison.Ordinal))
        {
            RecompOne.Runtime.Runtime.Shutdown();
            return 0;
        }

        // A generated loose-actor batch may carry its matching host-resolution
        // texture pack beside the PSX compatibility assets. Keep the batch
        // self-contained unless an explicit pack root was supplied.
        if (string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("RECOMP_ASSET_PACK_DIR")))
        {
            string actorRoot = Environment.GetEnvironmentVariable("SPIDEY_ASSET_DIR");
            string actorPacks = string.IsNullOrWhiteSpace(actorRoot)
                ? null
                : Path.Combine(Path.GetFullPath(actorRoot), "packs");
            if (actorPacks != null && Directory.Exists(actorPacks))
                Environment.SetEnvironmentVariable("RECOMP_ASSET_PACK_DIR", actorPacks);
        }

        // Non-interactive texture-pack authoring. This only observes the emulated
        // game's own draws and writes decoded uploads; it never drives the host UI.
        var dumpTextures = Environment.GetEnvironmentVariable("SPIDEY_DUMP_TEXTURES");
        if (string.Equals(dumpTextures, "pages", StringComparison.OrdinalIgnoreCase))
            RecompOne.Runtime.Assets.Textures.TextureDumper.SetPages(true);
        else if (string.Equals(dumpTextures, "tiles", StringComparison.OrdinalIgnoreCase))
            RecompOne.Runtime.Assets.Textures.TextureDumper.SetTiles(true);
        else if (string.Equals(dumpTextures, "all", StringComparison.OrdinalIgnoreCase))
            RecompOne.Runtime.Assets.Textures.TextureDumper.SetEnabled(true);

        // SPIDEY_GUARD=8009c5d4 -- report whatever writes rubbish into this address.
        var guard = Environment.GetEnvironmentVariable("SPIDEY_GUARD");
        if (!string.IsNullOrEmpty(guard))
            RecompOne.Runtime.Diagnostics.MemGuard.Address =
                Convert.ToUInt32(guard.Replace("0x", ""), 16) & 0x1FFFFFFFu;

        // SPIDEY_GUARD_VALUE=00a402c7 -- report wherever this exact word gets stored.
        var gv = Environment.GetEnvironmentVariable("SPIDEY_GUARD_VALUE");
        if (!string.IsNullOrEmpty(gv))
        {
            RecompOne.Runtime.Diagnostics.MemGuard.Value = Convert.ToUInt32(gv.Replace("0x", ""), 16);
            RecompOne.Runtime.Diagnostics.MemGuard.WatchValue = true;
        }

        RecompOne.Runtime.Diagnostics.MemGuard.Lenient =
            !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("SPIDEY_LENIENT"));

        // Preserve individual 60 Hz console interrupts while presenting at 30 FPS.
        // Batching two IRQs into one 30 Hz service pass doubles sequential vblank
        // waits in the game loop and cuts gameplay updates to 15 Hz.
        // Native counter waits already pace gameplay. A second 33 ms GPU timer
        // can miss their deadline and force an unnecessary third vblank.
        RecompOne.Runtime.GpuBusy.FrameBudgetMs = 0;
        RecompOne.Runtime.Runtime.VBlanksPerFrame = 2;
        RecompOne.Runtime.Sdk.LibCdStream.YieldEmptyPolls = true;
        Console.WriteLine("[timing] 30 FPS maximum; individual 60 Hz vblank interrupts");

        Diag.Install();
        GameTrace.Install();
        RecompOne.Runtime.Runtime.DiscValidator = ValidateDisc;
        EnableLogs(Environment.GetEnvironmentVariable("SPIDEY_LOG"));
        Capture.Install();
        Cheats.Install();
        RamSnap.Install();
        LevelSwitch.Install();
        ModelAlias.Install();
        ModelGuard.Install();
        Costume.Install();
        Rates.Install();
        RecompOne.Runtime.Diagnostics.PerformanceLog.GameCounterAddress = 0x800B4F38;

        // Precise projected geometry supplies the wide view. Repeating pixels
        // from the old 4:3 boundary invents stretched surfaces in uncovered areas.
        Wide.Install();
        Replay.Install();
        Harness.Install();

        AppDomain.CurrentDomain.UnhandledException += (_, e) => Diag.Fatal(e.ExceptionObject as Exception);

        try
        {
            RecompOne.Runtime.Runtime.Run(() =>
            {
                var mem = new PSMemory(RamSize);
                Entry.Run(mem, gameData, Title);
            });
        }
        catch (Exception ex)
        {
            Diag.Fatal(ex);
            RecompOne.Runtime.Runtime.Shutdown();
            return 3;
        }

        RecompOne.Runtime.Runtime.Shutdown();
        return 0;
    }

    static bool AcquireRuntimeLease()
    {
        RuntimeMutex = new System.Threading.Mutex(
            false, RecompOne.Runtime.Host.RuntimePaths.RuntimeMutexName);
        try
        {
            if (RuntimeMutex.WaitOne(0)) return true;
        }
        catch (System.Threading.AbandonedMutexException)
        {
            return true;
        }

        Console.Error.WriteLine(
            "[SpiderMan] another OpenSpidey game process is already running; refusing a second instance");
        RuntimeMutex.Dispose();
        RuntimeMutex = null;
        return false;
    }

    // SPIDEY_LOG=bios,sdk,cd,gpu,dma,spu,mdec
    static void EnableLogs(string spec)
    {
        if (string.IsNullOrWhiteSpace(spec)) return;
        foreach (var raw in spec.Split(','))
        {
            switch (raw.Trim().ToLowerInvariant())
            {
                case "bios": RecompOne.Runtime.Log.BiosOn = true; break;
                case "sdk": RecompOne.Runtime.Log.SdkOn = true; break;
                case "cd": RecompOne.Runtime.Log.CdOn = true; break;
                case "gpu": RecompOne.Runtime.Log.GpuOn = true; break;
                case "dma": RecompOne.Runtime.Log.DmaOn = true; break;
                case "spu": RecompOne.Runtime.Log.SpuOn = true; break;
                case "mdec": RecompOne.Runtime.Log.MdecOn = true; break;
                case "all":
                    RecompOne.Runtime.Log.BiosOn = RecompOne.Runtime.Log.SdkOn =
                    RecompOne.Runtime.Log.CdOn = RecompOne.Runtime.Log.GpuOn =
                    RecompOne.Runtime.Log.DmaOn = RecompOne.Runtime.Log.SpuOn =
                    RecompOne.Runtime.Log.MdecOn = true;
                    break;
            }
        }
        Console.WriteLine($"[SpiderMan] logging: {spec}");
    }

    static string ExeDirectory() => RecompOne.Runtime.Host.RuntimePaths.ApplicationDirectory;

    // Development builds may reuse a known loose tree. Shipped builds only fall
    // through to the executable-relative game directory managed by the installer.
    static string ResolveExistingGameData(string[] args)
    {
        if (args.Length > 0)
        {
            string requested = Path.GetFullPath(args[0]);
            if (RecompOne.Runtime.Cdrom.LooseDiscImage.IsLooseDirectory(requested)) return requested;
            return null;
        }

#if !STANDALONE_DISTRIBUTION
        foreach (var dir in CandidateDirs())
        {
            foreach (var candidate in new[]
            {
                Path.Combine(dir, "spiderman", "extracted"),
                Path.Combine(dir, "extracted"),
                Path.Combine(dir, "game"),
                dir,
            })
                if (RecompOne.Runtime.Cdrom.LooseDiscImage.IsLooseDirectory(candidate))
                    return Path.GetFullPath(candidate);
        }

#endif
        return null;
    }

    static string RequestedImage(string[] args)
    {
        if (args.Length == 0) return null;
        string path = Path.GetFullPath(args[0]);
        return File.Exists(path) ? path : null;
    }

    static string InstallOutput(string installRoot)
    {
        string output = Environment.GetEnvironmentVariable("SPIDEY_DATA");
        return string.IsNullOrWhiteSpace(output)
            ? Path.Combine(installRoot, "game")
            : Path.GetFullPath(output);
    }

    static System.Collections.Generic.IEnumerable<string> CandidateDirs()
    {
        var here = ExeDirectory() ?? AppContext.BaseDirectory;
        yield return here;
        var d = new DirectoryInfo(here);
        for (int i = 0; i < 8 && d != null; i++)
        {
            yield return d.FullName;
            d = d.Parent;
        }
        yield return Directory.GetCurrentDirectory();
    }

    // Store the loose directory, never the import image, as the persistent runtime path.
    static void SeedSettings(string gameData)
    {
        try
        {
            const string path = "settings.json";
            var options = new System.Text.Json.JsonSerializerOptions
            {
                WriteIndented = true,
                PropertyNameCaseInsensitive = true,
            };
            var config = File.Exists(path)
                ? System.Text.Json.JsonSerializer.Deserialize<RecompOne.Runtime.Config.GameConfig>(File.ReadAllText(path), options)
                : null;
            config ??= new RecompOne.Runtime.Config.GameConfig();
            config.CdPath = gameData;
            var json = System.Text.Json.JsonSerializer.Serialize(config, options);
            File.WriteAllText(path, json);
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"[SpiderMan] could not seed settings.json: {e.Message}");
        }
    }

    /// <summary>
    /// Spider-Man's primary volume descriptor carries a blank volume identifier, so
    /// the disc is recognised by its boot executable instead.
    /// </summary>
    static string ValidateDisc(string path)
    {
        return RecompOne.Runtime.Cdrom.DiscRevisionValidator.Validate(path, InstallProfile);
    }
}
