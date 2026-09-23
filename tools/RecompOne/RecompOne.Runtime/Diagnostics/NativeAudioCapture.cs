using System.Runtime.InteropServices;
using System.Text;

namespace RecompOne.Runtime.Diagnostics;

/// <summary>Opt-in capture of the exact PCM buffers submitted to OpenAL, never desktop audio.</summary>
internal static class NativeAudioCapture
{
    static BinaryWriter? _writer;
    static long _bytes;
    static bool _checked;
    const long MaximumBytes = 44100L * 4 * 180;

    public static void Write(short[] samples)
    {
        if (!_checked)
        {
            _checked = true;
            string? path = Environment.GetEnvironmentVariable("RECOMP_AUDIO_CAPTURE");
            if (string.IsNullOrWhiteSpace(path)) return;
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
            _writer = new BinaryWriter(File.Create(path));
            _writer.Write(new byte[44]);
        }
        if (_writer == null || _bytes >= MaximumBytes) return;
        var bytes = MemoryMarshal.AsBytes(samples.AsSpan());
        _writer.Write(bytes);
        _bytes += bytes.Length;
    }

    public static void Close()
    {
        if (_writer == null) return;
        _writer.BaseStream.Position = 0;
        _writer.Write(Encoding.ASCII.GetBytes("RIFF"));
        _writer.Write((uint)(_bytes + 36));
        _writer.Write(Encoding.ASCII.GetBytes("WAVEfmt "));
        _writer.Write(16u);
        _writer.Write((ushort)1); _writer.Write((ushort)2);
        _writer.Write(44100u); _writer.Write(176400u);
        _writer.Write((ushort)4); _writer.Write((ushort)16);
        _writer.Write(Encoding.ASCII.GetBytes("data"));
        _writer.Write((uint)_bytes);
        _writer.Dispose(); _writer = null;
    }
}
