using System.Buffers.Binary;
using System.IO.Compression;
using System.Reflection;
using System.Text;
using RecompOne.Runtime.Cdrom;

// Enter the actual private session used by the installer window at its Start
// button, then wait for its real import worker. No ImGui, host window, retail
// image or replacement installer is used. The payload assembly supplies only a
// tiny synthetic ZIP; normal DiscFs, LooseDiscImporter and BundledAssets run.
static class InstallerRetryChecks
{
    const string ResourceName = "DiscInstallRegression.Payload";
    const string AssetText = "synthetic bundled asset";
    static readonly Type SessionType = typeof(FirstRunDiscInstaller)
        .GetNestedType("Session", BindingFlags.NonPublic)!;
    static readonly MethodInfo StartMethod = SessionType.GetMethod("Start", BindingFlags.Instance | BindingFlags.NonPublic)!;

    public static void Run(string root, DiscInstallProfile[] profiles, Action<bool, string> check)
    {
        foreach (var original in profiles)
        {
            var profile = original with { BundledAssetResource = ResourceName };
            string image = Path.Combine(root, profile.DiscId + "-synthetic.iso");
            byte[] boot = Encoding.ASCII.GetBytes("synthetic executable; never executed");
            WriteImage(image, profile, boot);
            check(DiscRevisionValidator.Validate(image, profile) == null,
                profile.DiscId + " synthetic ISO passes actual disc validation");

            foreach (bool failBundledAssets in new[] { false, true })
            {
                string scenario = profile.DiscId + (failBundledAssets ? " bundled retry" : " disc retry");
                string installRoot = Path.Combine(root, scenario.Replace(' ', '-'));
                string output = Path.Combine(installRoot, "data");
                string bundle = Path.Combine(installRoot, "assets", "builtin");
                string blocker = failBundledAssets ? bundle : output;
                Directory.CreateDirectory(Path.GetDirectoryName(blocker)!);
                File.WriteAllText(blocker, "preserve the blocker until the test removes it");
                object session = NewSession(profile, output, image);
                WaitForWorker(session);
                check(!Finished(session) && Read<string>(session, "Error") is { Length: > 0 }
                    && Read<string>(session, "Result") == null && Field<Task>(session, "_task") == null,
                    scenario + " failed worker reports error and exposes retry without completing");
                check(File.ReadAllText(blocker) == "preserve the blocker until the test removes it",
                    scenario + " failed attempt preserves blocking file");
                check(File.Exists(Path.Combine(output, LooseDiscImage.ManifestName)) == failBundledAssets,
                    scenario + " disc commit marker reflects the completed extraction stage");

                // Retry uses the same session and image after repairing the actual
                // filesystem cause. In the bundle case, disc files are already
                // committed, so the importer must resume the bundled stage.
                File.Delete(blocker);
                StartMethod.Invoke(session, null);
                WaitForWorker(session);
                check(Finished(session) && Read<string>(session, "Result") == output,
                    scenario + " successful retry publishes the installation path");
                check(Read<string>(session, "Error") == null,
                    scenario + " successful retry clears the error consumed by EnsureInstalled");
                check(File.ReadAllBytes(Path.Combine(output, profile.BootFile)).SequenceEqual(boot)
                    && DiscRevisionValidator.Validate(output, profile) == null,
                    scenario + " retry installs the expected disc bytes");
                check(File.ReadAllText(Path.Combine(bundle, "fixture.txt")) == AssetText,
                    scenario + " retry installs the expected bundled bytes");
            }

            string cleanOutput = Path.Combine(root, profile.DiscId + "-clean", "data");
            object clean = NewSession(profile, cleanOutput, null);
            SetPath(clean, image + ".missing");
            StartMethod.Invoke(clean, null);
            check(!Finished(clean) && Read<string>(clean, "Result") == null
                && Field<Task>(clean, "_task") == null && Field<string>(clean, "_error") is { Length: > 0 },
                profile.DiscId + " invalid selection remains available for correction");
            SetPath(clean, image);
            StartMethod.Invoke(clean, null);
            WaitForWorker(clean);
            check(Finished(clean) && Read<string>(clean, "Result") == cleanOutput
                && Read<string>(clean, "Error") == null,
                profile.DiscId + " corrected selection completes without a stale error");
        }
    }

    static object NewSession(DiscInstallProfile profile, string output, string? image) =>
        Activator.CreateInstance(SessionType,
            [profile, output, new PayloadAssembly(), image])!;

    static bool Finished(object session) => Read<bool>(session, "Finished");
    static T? Read<T>(object session, string property) => (T?)SessionType.GetProperty(property)!.GetValue(session);
    static T? Field<T>(object session, string name) =>
        (T?)SessionType.GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(session);
    static void SetPath(object session, string path) =>
        SessionType.GetField("_path", BindingFlags.Instance | BindingFlags.NonPublic)!.SetValue(session, path);

    static void WaitForWorker(object session)
    {
        Task? worker = Field<Task>(session, "_task");
        if (worker != null && !worker.Wait(TimeSpan.FromSeconds(10)))
            throw new TimeoutException("Synthetic installer worker exceeded ten seconds");
        if (!Finished(session) && Read<string>(session, "Error") == null)
            throw new InvalidOperationException("Installer worker produced neither a result nor an error");
    }

    sealed class PayloadAssembly : Assembly
    {
        public override Stream? GetManifestResourceStream(string name)
        {
            if (name != ResourceName) return null;
            var payload = new MemoryStream();
            using (var archive = new ZipArchive(payload, ZipArchiveMode.Create, leaveOpen: true))
            using (var writer = new StreamWriter(archive.CreateEntry("fixture.txt").Open()))
                writer.Write(AssetText);
            payload.Position = 0;
            return payload;
        }
    }

    static void WriteImage(string path, DiscInstallProfile profile, byte[] boot)
    {
        const int sectorSize = 2048, rootLba = 20;
        byte[] bytes = new byte[24 * sectorSize];
        byte[] config = Encoding.ASCII.GetBytes($"BOOT = cdrom:\\{profile.BootFile};1\r\n");
        byte[] signature = [67, 68, 48, 48, 49, 1];
        bytes[16 * sectorSize] = 1;
        signature.CopyTo(bytes, 16 * sectorSize + 1);
        bytes[17 * sectorSize] = 255;
        signature.CopyTo(bytes, 17 * sectorSize + 1);
        WriteRecord(bytes.AsSpan(16 * sectorSize + 156), [0], rootLba, sectorSize, isDirectory: true);
        int offset = rootLba * sectorSize;
        offset += WriteRecord(bytes.AsSpan(offset), [0], rootLba, sectorSize, isDirectory: true);
        offset += WriteRecord(bytes.AsSpan(offset), [1], rootLba, sectorSize, isDirectory: true);
        offset += WriteRecord(bytes.AsSpan(offset), Encoding.ASCII.GetBytes("SYSTEM.CNF;1"), 21, config.Length);
        WriteRecord(bytes.AsSpan(offset), Encoding.ASCII.GetBytes(profile.BootFile + ";1"), 22, boot.Length);
        config.CopyTo(bytes, 21 * sectorSize);
        boot.CopyTo(bytes, 22 * sectorSize);
        File.WriteAllBytes(path, bytes);
    }

    static int WriteRecord(Span<byte> record, byte[] name, int lba, int size, bool isDirectory = false)
    {
        int length = (33 + name.Length + 1) & ~1;
        record[0] = (byte)length;
        BinaryPrimitives.WriteInt32LittleEndian(record[2..], lba);
        BinaryPrimitives.WriteInt32BigEndian(record[6..], lba);
        BinaryPrimitives.WriteInt32LittleEndian(record[10..], size);
        BinaryPrimitives.WriteInt32BigEndian(record[14..], size);
        record[25] = isDirectory ? (byte)2 : (byte)0;
        record[28] = record[31] = 1;
        record[32] = (byte)name.Length;
        name.CopyTo(record[33..]);
        return length;
    }
}
