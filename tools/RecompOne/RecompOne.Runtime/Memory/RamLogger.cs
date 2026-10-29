using System.Numerics;

namespace RecompOne.Runtime.Memory;

//to make a ram map similar do pcsxRedux's
public sealed class RamLogger
{
    public const int Width = 2048;
    public const int Height = 1024; 

    uint[]? _writeTimestamps;
    uint[]? _readTimestamps;
    // Zero is the "never accessed" sentinel, including before the first host frame.
    uint _cycle = 1;

    /// <summary>Allocate access history only while a memory diagnostic is visible.</summary>
    public bool Enabled
    {
        get => _writeTimestamps != null;
        set
        {
            if (value == Enabled) return;
            if (value)
            {
                _readTimestamps = new uint[Width * Height];
                _writeTimestamps = new uint[Width * Height];
            }
            else
            {
                _writeTimestamps = null;
                _readTimestamps = null;
            }
        }
    }

    public float DecayFrames = 90f;
    public Vector4 BackdropColor = new(0.25f, 0.15f, 0.15f, 1f);
    public Vector4 WriteColor = new(1f, 0f, 0f, 0.75f);
    public Vector4 ReadColor = new(0.3f, 0.5f, 1f, 0.75f);
    public bool ShowGreyscale = true;

    public uint Cycle => _cycle;
    public void Tick() => _cycle++;

    public uint GetWriteStamp(int byteIdx) =>
        _writeTimestamps is { } stamps && (uint)byteIdx < (uint)stamps.Length ? stamps[byteIdx] : 0u;

    
    public uint GetReadStamp(int byteIdx) =>
        _readTimestamps is { } stamps && (uint)byteIdx < (uint)stamps.Length ? stamps[byteIdx] : 0u;

    float HeatOf(uint ts)
    {
        if (ts == 0) return 0f;
        uint age = _cycle - ts;
        float half = MathF.Max(1f, DecayFrames);
        if (age > half * 16f) return 0f;
        return MathF.Exp(-age * 0.6931472f / half);
    }

    public float HeatAt(int byteIdx) => HeatOf(GetWriteStamp(byteIdx));

    public float ReadHeatAt(int byteIdx) => HeatOf(GetReadStamp(byteIdx));

    public void RecordWrite(uint physAddr, int bytes)
    {
        var stamps = _writeTimestamps;
        if (stamps == null) return;
        for (int i = 0; i < bytes; i++)
        {
            int idx = (int)((physAddr + (uint)i) & 0x1FFFFF);
            stamps[idx] = _cycle;
        }
    }

    //rd for show
    public void RecordRead(uint physAddr, int bytes)
    {
        var stamps = _readTimestamps;
        if (stamps == null) return;
        for (int i = 0; i < bytes; i++)
        {
            int idx = (int)((physAddr + (uint)i) & 0x1FFFFF);
            stamps[idx] = _cycle;
        }
    }

    public void BuildTexture(ReadOnlySpan<byte> ram, byte[] output)
    {
        int total = Width * Height;
        float br = BackdropColor.X, bg = BackdropColor.Y, bb = BackdropColor.Z;
        float wr = WriteColor.X, wg = WriteColor.Y, wb = WriteColor.Z, wa = WriteColor.W;
        float rr = ReadColor.X, rg = ReadColor.Y, rb = ReadColor.Z, ra = ReadColor.W;

        float half = MathF.Max(1f, DecayFrames);
        float k = -0.6931472f / half;
        float cutoff = half * 16f;
        uint cycle = _cycle;
        // A queued conversion may finish after its panel closes. Retain local
        // references for that conversion even if the main thread disables logging.
        var readTimestamps = _readTimestamps;
        var writeTimestamps = _writeTimestamps;

        for (int i = 0; i < total; i++)
        {
            byte b = i < ram.Length ? ram[i] : (byte)0;
            float shade = ShowGreyscale ? 1f - b / 255f : 1f;

            float r = br * shade, g = bg * shade, bl = bb * shade;

            float rHeat = 0f, wHeat = 0f;
            uint rts = readTimestamps?[i] ?? 0u;
            if (rts != 0)
            {
                uint age = cycle - rts;
                if (age <= cutoff) rHeat = MathF.Exp(age * k);
            }
            uint wts = writeTimestamps?[i] ?? 0u;
            if (wts != 0)
            {
                uint age = cycle - wts;
                if (age <= cutoff) wHeat = MathF.Exp(age * k);
            }

            float totalHeat = rHeat * ra + wHeat * wa;
            if (totalHeat > 0.0001f)
            {
                float inv = 1f / totalHeat;
                float hr = (rHeat * rr * ra + wHeat * wr * wa) * inv;
                float hg = (rHeat * rg * ra + wHeat * wg * wa) * inv;
                float hb = (rHeat * rb * ra + wHeat * wb * wa) * inv;
                float blend = totalHeat > 1f ? 1f : totalHeat;
                r += (hr - r) * blend;
                g += (hg - g) * blend;
                bl += (hb - bl) * blend;
            }

            int o = i << 2;
            output[o] = (byte)(r  * 255);
            output[o + 1] = (byte)(g  * 255);
            output[o + 2] = (byte)(bl * 255);
            output[o + 3] = 255;
        }
    }
}
