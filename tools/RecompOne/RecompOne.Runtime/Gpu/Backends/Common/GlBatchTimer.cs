using System.Diagnostics;
using Silk.NET.OpenGL;

namespace RecompOne.Runtime.Hle;

/// <summary>Optional bounded GL elapsed queries. Never wait for a result.</summary>
internal sealed class GlBatchTimer(GL gl) : IDisposable
{
    readonly uint[] _queries = new uint[128];
    readonly long[] _starts = new long[128], _presents = new long[128];
    int _read, _write, _pending;
    bool _active;

    public void Begin()
    {
        while (_pending > 0)
        {
            gl.GetQueryObject(_queries[_read], QueryObjectParameterName.ResultAvailable, out uint available);
            if (available == 0) break;
            gl.GetQueryObject(_queries[_read], QueryObjectParameterName.Result, out ulong ns);
            Diagnostics.PerformanceLog.GpuBatch(_starts[_read], _presents[_read], ns / 1_000_000.0);
            _read = (_read + 1) % _queries.Length; _pending--;
        }
        if (_pending == _queries.Length) { Diagnostics.PerformanceLog.GpuQueriesSkipped++; return; }
        if (_queries[_write] == 0) _queries[_write] = gl.GenQuery();
        _starts[_write] = Stopwatch.GetTimestamp(); _presents[_write] = Runtime.Presents;
        gl.BeginQuery(QueryTarget.TimeElapsed, _queries[_write]); _active = true;
    }

    public void End()
    {
        if (!_active) return;
        gl.EndQuery(QueryTarget.TimeElapsed); _active = false;
        _write = (_write + 1) % _queries.Length; _pending++;
    }

    public void Dispose()
    {
        End();
        foreach (uint query in _queries) if (query != 0) gl.DeleteQuery(query);
    }
}
