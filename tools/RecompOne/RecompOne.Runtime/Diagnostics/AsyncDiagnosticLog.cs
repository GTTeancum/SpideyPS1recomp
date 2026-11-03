using System.Threading.Channels;

namespace RecompOne.Runtime.Diagnostics;

/// <summary>
/// Bounded general logging. The file and the console each have their own queue and
/// worker, so a console nobody reads (a full stdout pipe) only drops console lines
/// and never stops the file log.
/// </summary>
public sealed class AsyncDiagnosticLog
{
    public const int MaxTextLength = 16384;
    sealed record Entry(string Text, string Prefix, TextWriter? Echo);
    readonly Channel<Entry> _queue;
    readonly Channel<(string Text, TextWriter Echo)> _echoQueue;
    readonly TextWriter _file;
    readonly Thread _worker, _echoWorker;
    Entry? _critical;
    long _dropped, _echoDropped, _truncated, _sinkErrors;
    public long Dropped => Interlocked.Read(ref _dropped);
    public long EchoDropped => Interlocked.Read(ref _echoDropped);
    public long Truncated => Interlocked.Read(ref _truncated);
    public long SinkErrors => Interlocked.Read(ref _sinkErrors);

    public AsyncDiagnosticLog(TextWriter file, int capacity = 1024)
    {
        _file = file;
        _queue = Channel.CreateBounded<Entry>(new BoundedChannelOptions(capacity)
        { SingleReader = true, SingleWriter = false, FullMode = BoundedChannelFullMode.Wait });
        _echoQueue = Channel.CreateBounded<(string, TextWriter)>(new BoundedChannelOptions(capacity)
        { SingleReader = true, SingleWriter = true, FullMode = BoundedChannelFullMode.Wait });
        _worker = new Thread(Run) { IsBackground = true, Name = "DiagnosticLog" };
        _echoWorker = new Thread(RunEcho) { IsBackground = true, Name = "DiagnosticConsole" };
        _worker.Start();
        _echoWorker.Start();
    }

    public void Write(string text, string prefix, TextWriter? echo = null, bool critical = false)
    {
        if (text.Length > MaxTextLength)
        {
            text = text[..MaxTextLength] + " [diagnostic text truncated]";
            Interlocked.Increment(ref _truncated);
        }
        var entry = new Entry(text, prefix, echo);
        if (critical)
        {
            // Preserve the latest fatal report even when the ordinary queue is full.
            if (Interlocked.Exchange(ref _critical, entry) != null)
                Interlocked.Increment(ref _dropped);
            _queue.Writer.TryWrite(new Entry("", "", null)); // Wake an idle reader.
        }
        else if (!_queue.Writer.TryWrite(entry)) Interlocked.Increment(ref _dropped);
    }

    // Only shutdown waits, and only for a bounded time. A blocked external sink
    // must never make gameplay, the watchdog, or process exit wait indefinitely.
    public bool Complete(int milliseconds = 2000)
    {
        _queue.Writer.TryComplete();
        var deadline = Environment.TickCount64 + milliseconds;
        bool file = _worker.Join(milliseconds);
        return _echoWorker.Join((int)Math.Max(0, deadline - Environment.TickCount64)) && file;
    }

    void Emit(Entry entry)
    {
        if (entry.Text.Length == 0) return;
        try
        {
            foreach (string line in entry.Text.Split('\n'))
                _file.WriteLine(entry.Prefix + line.TrimEnd('\r'));
            _file.Flush();
        }
        catch { Interlocked.Increment(ref _sinkErrors); }
        if (entry.Echo != null && !_echoQueue.Writer.TryWrite((entry.Text, entry.Echo)) &&
            Interlocked.Increment(ref _echoDropped) == 1)
            Emit(new Entry("[diag-log] console output is not being read; dropping console lines, this file continues", entry.Prefix, null));
    }

    void RunEcho()
    {
        while (_echoQueue.Reader.WaitToReadAsync().AsTask().GetAwaiter().GetResult())
            while (_echoQueue.Reader.TryRead(out var line))
            {
                try { line.Echo.WriteLine(line.Text); line.Echo.Flush(); }
                catch { Interlocked.Increment(ref _sinkErrors); }
            }
    }

    void Run()
    {
        try
        {
            while (_queue.Reader.WaitToReadAsync().AsTask().GetAwaiter().GetResult())
            {
                while (_queue.Reader.TryRead(out var entry))
                {
                    var fatal = Interlocked.Exchange(ref _critical, null);
                    if (fatal != null) Emit(fatal);
                    Emit(entry);
                }
            }
            var finalFatal = Interlocked.Exchange(ref _critical, null);
            if (finalFatal != null) Emit(finalFatal);
            if (Dropped != 0 || EchoDropped != 0 || Truncated != 0 || SinkErrors != 0)
                Emit(new Entry($"[diag-log] dropped={Dropped} consoleDropped={EchoDropped} truncated={Truncated} sinkErrors={SinkErrors}", "", null));
        }
        finally
        {
            _echoQueue.Writer.TryComplete();
            try { _file.Dispose(); } catch { }
        }
    }
}
