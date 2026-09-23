using System.Diagnostics;
using System.Text.Json;
using System.Threading.Channels;

namespace RecompOne.Runtime.Diagnostics;

/// <summary>Opt-in device-queue and SPU voice evidence, without recording desktop audio.</summary>
internal static class NativeAudioStateTrace
{
    static readonly string? Path = Environment.GetEnvironmentVariable("RECOMP_AUDIO_STATE_TRACE");
    static readonly JsonSerializerOptions Options = new() { IncludeFields = true };
    readonly record struct Observation(Spu Spu, double Seconds, DateTimeOffset Utc,
        int State, int Processed, bool Playing, bool Recovered);
    static readonly Channel<Observation> Pending = Channel.CreateBounded<Observation>(
        new BoundedChannelOptions(64) { SingleReader = true, SingleWriter = true,
            FullMode = BoundedChannelFullMode.DropWrite });
    static Task? _worker;
    static long _start, _next;

    public static void Record(Spu spu, int state, int processed, bool playing, bool recovered = false)
    {
        if (string.IsNullOrWhiteSpace(Path)) return;
        long now = Stopwatch.GetTimestamp();
        if (_start == 0)
        {
            _start = now;
            _worker = Task.Run(WriteObservations);
        }
        if (now - _start > Stopwatch.Frequency * 180L) return;
        if (playing && now < _next) return;
        _next = now + Stopwatch.Frequency;
        // Never serialize JSON or write files on the real-time mixer thread.
        Pending.Writer.TryWrite(new(spu, (now - _start) / (double)Stopwatch.Frequency,
            DateTimeOffset.UtcNow, state, processed, playing, recovered));
    }

    static async Task WriteObservations()
    {
        Directory.CreateDirectory(System.IO.Path.GetDirectoryName(System.IO.Path.GetFullPath(Path!))!);
        using var writer = new StreamWriter(Path!) { AutoFlush = true };
        var voices = new Spu.VoiceDebug[24];
        await foreach (var o in Pending.Reader.ReadAllAsync())
        {
            o.Spu.CaptureDebug(voices, out var control);
            writer.WriteLine(JsonSerializer.Serialize(new {
                seconds = o.Seconds, utc = o.Utc, state = o.State,
                processed = o.Processed, playing = o.Playing,
                // State was observed before recovery; the timestamp below is
                // recorded after the queue operation, not the onset of starvation.
                recoveryPerformed = o.Recovered,
                // Voice inspection is asynchronous; retain its separate timestamp.
                voiceUtc = DateTimeOffset.UtcNow, control,
                voices = voices.Select((v, i) => new { index = i, voice = v,
                    // Prefix identifies the original VAB sample after loading.
                    sample = Convert.ToHexString(o.Spu.Ram.AsSpan((v.StartAddr << 3) & (Spu.RamSize - 1),
                        Math.Min(64, Spu.RamSize - ((v.StartAddr << 3) & (Spu.RamSize - 1)))))
                }).Where(v => v.voice.Phase != Spu.AdsrPhase.Off).ToArray()
            }, Options));
        }
    }

    public static void Close()
    {
        Pending.Writer.TryComplete();
        _worker?.GetAwaiter().GetResult();
    }
}
