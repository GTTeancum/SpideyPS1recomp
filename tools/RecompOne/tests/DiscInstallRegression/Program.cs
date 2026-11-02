using System.Text;
using System.Text.Json;
using RecompOne.Runtime.Cdrom;

var sm1 = new DiscInstallProfile("Spider-Man", "Spider-Man (USA)", "SLUS-00875", "SLUS_008.75", "unused");
var sm2 = new DiscInstallProfile("Spider-Man 2", "Spider-Man 2 (USA)", "SLUS-01378", "SLUS_013.78", "unused");
int checks = 0;
void Check(bool condition, string name) { if (!condition) throw new Exception(name); checks++; Console.WriteLine("PASS " + name); }
string root = Path.Combine(Path.GetTempPath(), "OpenSpidey-DiscInstallRegression-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
// Small, retained fixtures only; never touch retail media or existing installations.
foreach (var profile in new[] { sm1, sm2 })
{
    string dir = Path.Combine(root, profile.DiscId); Directory.CreateDirectory(dir);
    File.WriteAllText(Path.Combine(dir, profile.BootFile), "different USA boot bytes; intentionally no known hash or size");
    File.WriteAllText(Path.Combine(dir, "SYSTEM.CNF"), $"BOOT = cdrom:\\{profile.BootFile};1\r\nTCB = 4\r\n");
    var manifest = new LooseDiscImage.Manifest { Files = [new() { Path = profile.BootFile, Size = 61 }, new() { Path = "SYSTEM.CNF", Lba = 2, Size = 60 }] };
    File.WriteAllText(Path.Combine(dir, LooseDiscImage.ManifestName), JsonSerializer.Serialize(manifest));
    Check(DiscRevisionValidator.Validate(dir, profile) == null, profile.DiscId + " accepts USA without hash/size/leadout");
    Check(DiscRevisionValidator.Validate(dir, profile == sm1 ? sm2 : sm1) != null, "rejects wrong game");
    File.WriteAllText(Path.Combine(dir, "SYSTEM.CNF"), "BOOT = cdrom:\\SLES_028.86;1\r\n");
    Check(DiscRevisionValidator.Validate(dir, profile) != null, "rejects other region despite USA filename present");
    File.WriteAllText(Path.Combine(dir, "SYSTEM.CNF"), $" boot\t= cdrom:/{profile.BootFile.ToLowerInvariant()};1\n");
    Check(DiscRevisionValidator.Validate(dir, profile) == null, "accepts config case/spacing changes");
}
foreach (var (sectorSize, offset) in new[] { (2048, 0), (2352, 24), (2352, 16), (2336, 8) })
{
    byte[] bytes = new byte[sectorSize * 24];
    new byte[] { 1, 67, 68, 48, 48, 49, 1 }.CopyTo(bytes, 16 * sectorSize + offset);
    for (int i = 0; i < 2048; i++) bytes[20 * sectorSize + offset + i] = (byte)(i % 251);
    string path = Path.Combine(root, $"layout-{sectorSize}-{offset}.iso"); File.WriteAllBytes(path, bytes);
    using var image = DiscImage.Open(path);
    Check(image.ReadSectorData(20, 2048).SequenceEqual(Enumerable.Range(0, 2048).Select(i => (byte)(i % 251))), $"ISO sector layout {sectorSize}/{offset}");
    if (sectorSize == 2352 && offset == 24)
        Check(image.ReadSectorData(20, 2336).SequenceEqual(bytes.AsSpan(20 * sectorSize + 16, 2336).ToArray()), "XA subheader and full payload retained");
    Check(image.ReadSectorData(24, 2048).All(b => b == 0), "out of range never crosses track");
}
// Dreamcast movie import: a cooked-sector GDI with the high-density data track at LBA
// 45000 holding IP.BIN, an ISO9660 volume and two movies, one the game maps to a PS1
// movie and one it does not.
string MakeGdRom(string name, string product, bool dreamcast)
{
    string folder = Path.Combine(root, name); Directory.CreateDirectory(folder);
    var track = new byte[40 * 2048];
    void Text(int sector, int offset, string text) => Encoding.ASCII.GetBytes(text).CopyTo(track, sector * 2048 + offset);
    Text(0, 0, dreamcast ? "SEGA SEGAKATANA " : "SONY COMPUTER EN");
    Text(0, 0x30, "U       "); Text(0, 0x40, product.PadRight(10));
    track[16 * 2048] = 1; Text(16, 1, "CD001");
    void Record(int sector, int offset, string id, int lba, int size, bool folder)
    {
        int at = sector * 2048 + offset, length = 33 + id.Length + (id.Length % 2 == 0 ? 1 : 0);
        track[at] = (byte)length;
        BitConverter.GetBytes(lba).CopyTo(track, at + 2); BitConverter.GetBytes(size).CopyTo(track, at + 10);
        track[at + 25] = (byte)(folder ? 2 : 0); track[at + 32] = (byte)id.Length;
        Encoding.ASCII.GetBytes(id).CopyTo(track, at + 33);
    }
    Record(16, 156, "\0", 45020, 2048, true);
    Record(20, 0, "\0", 45020, 2048, true); Record(20, 34, "\u0001", 45020, 2048, true);
    Record(20, 68, "L1M1.SFD;1", 45021, 3000, false);
    Record(20, 112, "LEGAL.SFD;1", 45023, 100, false);
    for (int i = 0; i < 3000; i++) track[21 * 2048 + i] = (byte)(i % 253);
    File.WriteAllBytes(Path.Combine(folder, "track03.bin"), track);
    File.WriteAllBytes(Path.Combine(folder, "track01.bin"), new byte[2048]);
    string gdi = Path.Combine(folder, "disc.gdi");
    File.WriteAllText(gdi, "2\n1 0 4 2048 track01.bin 0\n3 45000 4 2048 \"track03.bin\" 0\n");
    return gdi;
}
string movieGdi = MakeGdRom("dc-ok", DreamcastMovieImporter.ProductNumber, true);
Check(DreamcastMovieImporter.Validate(movieGdi) == null, "Spider-Man (USA) Dreamcast disc accepted");
Check(DreamcastMovieImporter.Validate(MakeGdRom("dc-other", "T9999N", true))?.Contains("T9999N") == true, "other Dreamcast game rejected");
Check(DreamcastMovieImporter.Validate(MakeGdRom("dc-sony", DreamcastMovieImporter.ProductNumber, false)) != null, "non-Dreamcast data track rejected");
Check(DreamcastMovieImporter.Validate(Path.Combine(root, "layout-2352-16.iso")) != null, "plain ISO is not a Dreamcast disc");
string movies = Path.Combine(root, "movies");
DreamcastMovieImporter.Import(movieGdi, movies, null, CancellationToken.None);
Check(File.ReadAllBytes(Path.Combine(movies, "L1M1.SFD")).SequenceEqual(Enumerable.Range(0, 3000).Select(i => (byte)(i % 253))),
    "mapped movie copied byte for byte");
Check(!File.Exists(Path.Combine(movies, "LEGAL.SFD")) && Directory.GetFiles(movies).Length == 1,
    "unmapped extras and partial files are not left behind");

foreach (string path in args)
{
    var profile = Path.GetFileName(path).Contains("2") ? sm2 : sm1;
    Check(DiscRevisionValidator.Validate(path, profile) == null, "real disc " + Path.GetFileName(path));
}
Console.WriteLine($"{checks} checks passed. Tiny fixtures: {root}");

