using System.Diagnostics;
using Silk.NET.OpenGL;

namespace RecompOne.Runtime.Hle;

/// <summary>Three fixed regions, appended without overwriting pending draws. GL 3.3+.</summary>
internal sealed unsafe class GlVertexStream : IDisposable
{
    readonly GL _gl;
    readonly int _capacity, _stride;
    readonly nint[] _fences = new nint[3];
    int _region, _used;
    public uint Buffer { get; }

    public GlVertexStream(GL gl, int capacity, int stride)
    {
        _gl = gl; _capacity = capacity; _stride = stride;
        Buffer = gl.GenBuffer();
        gl.BindBuffer(BufferTargetARB.ArrayBuffer, Buffer);
        gl.BufferData(BufferTargetARB.ArrayBuffer, checked((nuint)(3L * capacity * stride)), null, BufferUsageARB.StreamDraw);
    }

    public int Upload<T>(ReadOnlySpan<T> vertices) where T : unmanaged
    {
        if (sizeof(T) != _stride || vertices.Length > _capacity) throw new ArgumentOutOfRangeException(nameof(vertices));
        if (_used + vertices.Length > _capacity)
        {
            // Called only after all passes for the previous upload were submitted.
            _fences[_region] = _gl.FenceSync(GLEnum.SyncGpuCommandsComplete, 0u);
            if (_fences[_region] == 0) throw new InvalidOperationException("Could not fence vertex storage");
            _region = (_region + 1) % 3;
            WaitForRegion();
            _used = 0;
        }
        int first = _region * _capacity + _used;
        nuint bytes = (nuint)(vertices.Length * _stride);
        _gl.BindBuffer(BufferTargetARB.ArrayBuffer, Buffer);
        // Invalidating this range, not the entire buffer, preserves earlier draws.
        // UNSYNCHRONIZED is safe because fences protect reuse of each region.
        void* target = _gl.MapBufferRange(BufferTargetARB.ArrayBuffer, (nint)((long)first * _stride), bytes,
            MapBufferAccessMask.WriteBit | MapBufferAccessMask.InvalidateRangeBit | MapBufferAccessMask.UnsynchronizedBit);
        if (target == null) throw new InvalidOperationException("Could not map vertex stream");
        fixed (T* source = vertices) System.Buffer.MemoryCopy(source, target, (long)bytes, (long)bytes);
        if (!_gl.UnmapBuffer(BufferTargetARB.ArrayBuffer)) throw new InvalidOperationException("Vertex stream contents invalidated by driver");
        _used += vertices.Length;
        Diagnostics.PerformanceLog.UploadedBytes += (long)bytes;
        Diagnostics.PerformanceLog.Batches++;
        return first;
    }

    void WaitForRegion()
    {
        nint fence = _fences[_region];
        if (fence == 0) return;
        long start = Stopwatch.GetTimestamp();
        while (true)
        {
            var result = _gl.ClientWaitSync(fence, SyncObjectMask.Bit, 1_000_000);
            if (result == GLEnum.AlreadySignaled || result == GLEnum.ConditionSatisfied) break;
            if (result == GLEnum.WaitFailed || Stopwatch.GetElapsedTime(start).TotalSeconds > 5)
                throw new InvalidOperationException("GPU did not release bounded vertex storage");
        }
        Diagnostics.PerformanceLog.VertexFenceWaitMs += Stopwatch.GetElapsedTime(start).TotalMilliseconds;
        _gl.DeleteSync(fence); _fences[_region] = 0;
    }

    public void Dispose()
    {
        foreach (nint fence in _fences) if (fence != 0) _gl.DeleteSync(fence);
        _gl.DeleteBuffer(Buffer);
    }
}
