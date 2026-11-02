using System.Text;
using RecompOne.Runtime.Host;
using RecompOne.Runtime.Media;

namespace RecompOne.Runtime.Cdrom;

/// <summary>
/// Copies the original Dreamcast movies (.SFD) off a Spider-Man (USA) GD-ROM dump into
/// the folder the movie player reads (DreamcastMovies.Folder). The files are copied
/// byte for byte; nothing is converted. Accepts a Redump-style CUE (single- and
/// high-density areas) or a GDI. Only the movies the game maps to its PS1 movies are
/// copied; any that are missing simply keep playing the PS1 version.
/// </summary>
public static class DreamcastMovieImporter
{
    public const string ProductNumber = "T13008N";
    const int HighDensityStart = 45000;

    sealed record Span(int StartLba, int Sectors, string File, int SectorSize, int DataOffset);

    /// <summary>Null when the image is the Spider-Man (USA) Dreamcast disc, else the problem.</summary>
    public static string? Validate(string path)
    {
        try
        {
            using var disc = GdRom.Open(path);
            return disc.ProductProblem();
        }
        catch (Exception e) when (e is IOException or InvalidDataException or UnauthorizedAccessException or FormatException)
        {
            return $"{e.Message}. Select the Spider-Man (USA) Dreamcast disc [{ProductNumber}] as CUE or GDI.";
        }
    }

    /// <summary>Returns the folder the movies were written to.</summary>
    public static string Import(string path, string directory,
        IProgress<LooseDiscImporter.Progress>? progress, CancellationToken cancellationToken)
    {
        using var disc = GdRom.Open(path);
        string? problem = disc.ProductProblem();
        if (problem != null) throw new InvalidDataException(problem);
        var movies = disc.Files()
            .Where(f => f.Name.EndsWith(".SFD", StringComparison.OrdinalIgnoreCase) &&
                MovieResolver.IsMapped(f.Name[..^4]))
            .ToList();
        if (movies.Count == 0) throw new InvalidDataException("The Dreamcast disc has no movie files.");
        Directory.CreateDirectory(directory);
        long total = movies.Sum(m => (long)m.Size), done = 0;
        for (int i = 0; i < movies.Count; i++)
        {
            var movie = movies[i];
            string destination = Path.Combine(directory, movie.Name.ToUpperInvariant());
            progress?.Report(new("Copying Dreamcast movies", movie.Name, i, movies.Count, done, total));
            string temporary = destination + ".partial";
            using (var stream = File.Create(temporary))
            {
                int sectors = checked((int)((movie.Size + 2047) / 2048));
                for (int s = 0; s < sectors; s++)
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    byte[] data = disc.Read(movie.Lba + s);
                    int count = (int)Math.Min(2048L, movie.Size - (long)s * 2048L);
                    stream.Write(data, 0, count);
                    done += count;
                    if ((s & 255) == 0)
                        progress?.Report(new("Copying Dreamcast movies", movie.Name, i, movies.Count, done, total));
                }
            }
            File.Move(temporary, destination, overwrite: true);
            Console.WriteLine($"[import] Dreamcast movie {movie.Name} -> {destination}");
        }
        progress?.Report(new("Copying Dreamcast movies", "", movies.Count, movies.Count, total, total));
        return directory;
    }

    sealed class GdRom : IDisposable
    {
        readonly List<Span> _spans;
        readonly Dictionary<string, FileStream> _files = new(StringComparer.OrdinalIgnoreCase);

        GdRom(List<Span> spans) => _spans = spans;

        public static GdRom Open(string path)
        {
            string extension = Path.GetExtension(path).ToLowerInvariant();
            var spans = extension switch
            {
                ".cue" => FromCue(path),
                ".gdi" => FromGdi(path),
                _ => throw new InvalidDataException("A Dreamcast disc must be opened through its CUE or GDI file"),
            };
            if (!spans.Any(s => s.StartLba == HighDensityStart))
                throw new InvalidDataException("This is not a Dreamcast GD-ROM dump (no high-density area)");
            return new GdRom(spans);
        }

        // Redump: FILE entries in order; the high-density area restarts at LBA 45000.
        // Each file's sectors follow on from the previous file's, pregaps included.
        static List<Span> FromCue(string cue)
        {
            var spans = new List<Span>();
            string folder = Path.GetDirectoryName(Path.GetFullPath(cue))!;
            int lba = 0;
            string? file = null;
            foreach (string raw in File.ReadLines(cue))
            {
                string line = raw.Trim();
                if (line.StartsWith("REM HIGH-DENSITY AREA", StringComparison.OrdinalIgnoreCase)) lba = HighDensityStart;
                else if (line.StartsWith("FILE ", StringComparison.OrdinalIgnoreCase))
                {
                    int open = line.IndexOf('"'), close = line.LastIndexOf('"');
                    if (open < 0 || close <= open) throw new InvalidDataException($"Unreadable CUE line: {line}");
                    file = Path.Combine(folder, line[(open + 1)..close]);
                }
                else if (line.StartsWith("TRACK ", StringComparison.OrdinalIgnoreCase) && file != null)
                {
                    bool data = line.Contains("MODE1", StringComparison.OrdinalIgnoreCase);
                    int sectors = checked((int)(new FileInfo(file).Length / 2352));
                    if (data) spans.Add(new Span(lba, sectors, file, 2352, 16));
                    lba += sectors;
                    file = null;
                }
            }
            return spans;
        }

        // GDI: "number lba type sector-size file offset", type 4 for data.
        static List<Span> FromGdi(string gdi)
        {
            var spans = new List<Span>();
            string folder = Path.GetDirectoryName(Path.GetFullPath(gdi))!;
            foreach (string raw in File.ReadLines(gdi).Skip(1))
            {
                string line = raw.Trim();
                if (line.Length == 0) continue;
                string? name = null;
                int quote = line.IndexOf('"');
                if (quote >= 0)
                {
                    int end = line.IndexOf('"', quote + 1);
                    name = line[(quote + 1)..end];
                    line = line[..quote] + "X" + line[(end + 1)..];
                }
                string[] parts = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length < 5) throw new InvalidDataException($"Unreadable GDI line: {raw}");
                if (parts[2] != "4") continue;
                int size = int.Parse(parts[3]);
                string file = Path.Combine(folder, name ?? parts[4]);
                int sectors = checked((int)(new FileInfo(file).Length / size));
                spans.Add(new Span(int.Parse(parts[1]), sectors, file, size, size == 2352 ? 16 : 0));
            }
            return spans;
        }

        public byte[] Read(int lba)
        {
            var span = _spans.FirstOrDefault(s => lba >= s.StartLba && lba < s.StartLba + s.Sectors)
                ?? throw new InvalidDataException($"Sector {lba} is outside the dump");
            if (!_files.TryGetValue(span.File, out var stream))
                _files[span.File] = stream = new FileStream(span.File, FileMode.Open, FileAccess.Read, FileShare.Read);
            var data = new byte[2048];
            stream.Position = (long)(lba - span.StartLba) * span.SectorSize + span.DataOffset;
            stream.ReadExactly(data);
            return data;
        }

        public string? ProductProblem()
        {
            byte[] header = Read(HighDensityStart);
            string hardware = Encoding.ASCII.GetString(header, 0, 16);
            string product = Encoding.ASCII.GetString(header, 0x40, 10).Trim();
            string area = Encoding.ASCII.GetString(header, 0x30, 8);
            if (hardware != "SEGA SEGAKATANA ")
                return "This is not a Dreamcast disc.";
            if (product != ProductNumber || !area.Contains('U'))
                return $"This Dreamcast disc is {product}, not Spider-Man (USA) [{ProductNumber}].";
            return null;
        }

        public readonly record struct Entry(string Name, int Lba, uint Size);

        public IEnumerable<Entry> Files()
        {
            byte[] volume = Read(HighDensityStart + 16);
            if (Encoding.ASCII.GetString(volume, 1, 5) != "CD001")
                throw new InvalidDataException("The Dreamcast disc has no ISO9660 file system");
            return Listing(BitConverter.ToInt32(volume, 156 + 2), BitConverter.ToUInt32(volume, 156 + 10), 0);
        }

        IEnumerable<Entry> Listing(int lba, uint size, int depth)
        {
            if (depth > 8) yield break;
            for (int s = 0; s < (size + 2047) / 2048; s++)
            {
                byte[] sector = Read(lba + s);
                for (int i = 0; i < 2048 && sector[i] != 0; i += sector[i])
                {
                    int length = sector[i];
                    int extent = BitConverter.ToInt32(sector, i + 2);
                    uint bytes = BitConverter.ToUInt32(sector, i + 10);
                    bool folder = (sector[i + 25] & 2) != 0;
                    int nameLength = sector[i + 32];
                    if (nameLength == 1 && sector[i + 33] <= 1) continue;
                    string name = Encoding.ASCII.GetString(sector, i + 33, nameLength).Split(';')[0];
                    if (folder)
                        foreach (var entry in Listing(extent, bytes, depth + 1)) yield return entry;
                    else
                        yield return new Entry(name, extent, bytes);
                    if (length == 0) break;
                }
            }
        }

        public void Dispose()
        {
            foreach (var stream in _files.Values) stream.Dispose();
        }
    }
}
