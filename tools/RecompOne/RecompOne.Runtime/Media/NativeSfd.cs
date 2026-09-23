using System.Runtime.InteropServices;

namespace RecompOne.Runtime.Media;

/// <summary>
/// Serial, in-process decoder of ORIGINAL Sofdec MPEG-1/ADX files. No transcoding,
/// cache, subprocess or seek-to-another-file. Native state is bounded and the
/// callback always reads from this one read-only handle. Do not share across threads.
/// </summary>
public sealed unsafe class NativeSfd : IDisposable
{
    const uint Abi = 0x00010000;
    public const int MaximumFrames = 108000;
    [StructLayout(LayoutKind.Sequential)]
    struct Info
    {
        public uint Width, Height, FpsNum, FpsDen, SarNum, SarDen;
        public uint SampleRate, Channels, AudioFrames, Reserved;
    }
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate int ReadAt(nint user, ulong offset, byte* destination, uint length);
    static class Core
    {
        const string Library = "OpenSpideySfd";
        [DllImport(Library, CallingConvention=CallingConvention.Cdecl)] internal static extern uint sfd_abi();
        [DllImport(Library, CallingConvention=CallingConvention.Cdecl)] internal static extern uint sfd_workspace_size();
        [DllImport(Library, CallingConvention=CallingConvention.Cdecl)] internal static extern int sfd_open(void* state, uint bytes, ReadAt read, nint user, ulong size, out Info info);
        [DllImport(Library, CallingConvention=CallingConvention.Cdecl)] internal static extern int sfd_video(void* state, byte* yuv, uint bytes, out uint frameIndex);
        [DllImport(Library, CallingConvention=CallingConvention.Cdecl)] internal static extern int sfd_audio(void* state, short* pcm, uint frames);
        [DllImport(Library, CallingConvention=CallingConvention.Cdecl)] internal static extern int sfd_rgba(byte* yuv, uint bytes, uint width, uint height, byte* rgba, uint capacity);
        [DllImport(Library, CallingConvention=CallingConvention.Cdecl)] internal static extern void sfd_close(void* state);
    }
    FileStream? _file;
    ReadAt? _read;
    void* _state;
    byte[] _yuv = [];
    Exception? _readFailure;
    bool _disposed;
    public int Width { get; private set; }
    public int Height { get; private set; }
    public uint FpsNumerator { get; private set; }
    public uint FpsDenominator { get; private set; }
    public int SampleRate { get; private set; }
    public int SourceChannels { get; private set; }
    public long AudioFrames { get; private set; }
    public long AudioFramesRead { get; private set; }
    public int DecodedFrames { get; private set; }
    public bool VideoEnded { get; private set; }
    public float DisplayAspect { get; private set; }
    public double AudioDuration => (double)AudioFrames / SampleRate;
    public double DecodedVideoDuration => (double)DecodedFrames * FpsDenominator / FpsNumerator;
    public long SourceLength { get; private set; }

    public NativeSfd(string path)
    {
        try
        {
            // A held file is deliberately never reopened by name between video/audio.
            _file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1, FileOptions.RandomAccess);
            SourceLength = _file.Length;
            if (SourceLength < 2048 || SourceLength > 512L*1024*1024)
                throw new InvalidDataException("SFD is outside the supported file bounds");
            if (Core.sfd_abi() != Abi) throw new InvalidDataException("SFD decoder ABI mismatch; rebuild matching runtime/native files");
            uint size = Core.sfd_workspace_size();
            if (size < 1024 || size > 4*1024*1024 || (size & 15) != 0)
                throw new InvalidDataException("invalid SFD decoder workspace size");
            _state = NativeMemory.AlignedAlloc(size, 16);
            if (_state == null) throw new OutOfMemoryException("SFD decoder workspace");
            _read = ReadOriginal;
            int result = Core.sfd_open(_state, size, _read, 0, (ulong)SourceLength, out Info info);
            GC.KeepAlive(_read);
            Check(result, "open");
            if (info.Width == 0 || info.Width > 720 || info.Height == 0 || info.Height > 576 ||
                ((info.Width | info.Height) & 1) != 0 || info.FpsNum == 0 || info.FpsDen == 0 ||
                info.SarNum == 0 || info.SarDen == 0 || info.SampleRate < 8000 || info.SampleRate > 48000 ||
                info.Channels is not (1 or 2) || info.AudioFrames == 0 || info.Reserved != 0)
                throw new InvalidDataException("invalid SFD decoder metadata");
            Width = (int)info.Width; Height = (int)info.Height;
            FpsNumerator = info.FpsNum; FpsDenominator = info.FpsDen;
            SampleRate = (int)info.SampleRate; SourceChannels = (int)info.Channels; AudioFrames = info.AudioFrames;
            DisplayAspect = (float)((double)Width * info.SarNum / (Height * (double)info.SarDen));
            _yuv = new byte[checked(Width * Height * 3 / 2)];
        }
        catch { Dispose(); throw; }
    }

    int ReadOriginal(nint unused, ulong offset, byte* destination, uint length)
    {
        // No managed exception may unwind through the C decoder. A short read is
        // returned as such; native code detects early EOF/file truncation.
        try
        {
            if (_disposed || _file == null || offset > (ulong)SourceLength || length > (ulong)SourceLength-offset || length > 32768)
                return -1;
            return RandomAccess.Read(_file.SafeFileHandle, new Span<byte>(destination, (int)length), checked((long)offset));
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or ObjectDisposedException or ArgumentException)
        { _readFailure = e; return -1; }
    }

    void Check(int result, string operation)
    {
        if (result >= 0) return;
        string reason = result switch
        {
            -1 => "invalid decoder call", -2 => "source I/O failure", -3 => "invalid program stream",
            -4 => "unsupported/encrypted Sofdec format", -5 => "truncated stream", -6 => "invalid MPEG-1 picture",
            -7 => "invalid ADX stream", -8 => "decoder bound exceeded", _ => "decoder failure"
        };
        throw new InvalidDataException($"SFD {operation}: {reason} ({result})", _readFailure);
    }

    /// <summary>Next display-order frame only; no hidden predecoded movie buffer.</summary>
    public bool ReadFrameRgba(Span<byte> rgba)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (rgba.Length != checked(Width*Height*4)) throw new ArgumentException("RGBA frame size", nameof(rgba));
        if (VideoEnded) return false;
        fixed (byte* y = _yuv)
        fixed (byte* output = rgba)
        {
            int result = Core.sfd_video(_state, y, (uint)_yuv.Length, out uint index);
            GC.KeepAlive(_read); Check(result, "video");
            if (result == 0) { VideoEnded = true; return false; }
            if (result != 1 || index != (uint)DecodedFrames || DecodedFrames >= MaximumFrames)
                throw new InvalidDataException("SFD frame sequence changed");
            Check(Core.sfd_rgba(y, (uint)_yuv.Length, (uint)Width, (uint)Height, output, (uint)rgba.Length), "color conversion");
            DecodedFrames++;
            return true;
        }
    }

    /// <summary>Original-rate stereo PCM. The final ADX block is trimmed to its declared sample count.</summary>
    public int ReadAudio(Span<short> pcm, int frames)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (frames <= 0 || frames > 65536 || pcm.Length < checked(frames*2)) throw new ArgumentOutOfRangeException(nameof(frames));
        fixed (short* p = pcm)
        {
            int count = Core.sfd_audio(_state, p, (uint)frames);
            GC.KeepAlive(_read); Check(count, "audio");
            if (count > frames || AudioFramesRead + count > AudioFrames || (count == 0 && AudioFramesRead != AudioFrames))
                throw new InvalidDataException("SFD audio length changed");
            AudioFramesRead += count; return count;
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        try
        {
            if (_state != null)
            {
                void* state = _state; _state = null;
                try { Core.sfd_close(state); } finally { NativeMemory.AlignedFree(state); }
            }
        }
        finally { _file?.Dispose(); _file = null; _read = null; GC.SuppressFinalize(this); }
    }
    ~NativeSfd()
    {
        // No callback or decode is asynchronous. KeepAlive protects in-flight calls.
        // Do not call into a potentially unloading native DLL from the finalizer.
        if (_state != null) { NativeMemory.AlignedFree(_state); _state = null; }
        _file?.Dispose();
    }
}
