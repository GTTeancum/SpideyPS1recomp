using System.Text;
using RecompOne.Runtime.Diagnostics;

int assertions = 0;
void Check(bool value, string name)
{
    if (!value) throw new Exception(name);
    assertions++;
}

// A stalled file sink must not hold the producer or prevent bounded shutdown.
var sink = new GateWriter();
var log = new AsyncDiagnosticLog(sink, 2);
log.Write("first", "P ");
Check(sink.Entered.Wait(3000), "worker reached blocked file sink");
var producer = Task.Run(() =>
{
    for (int i = 0; i < 10000; i++) log.Write($"ordinary-{i}", "P ");
    log.Write("fatal-reserved", "F ", critical: true);
});
Check(producer.Wait(3000), "producer completes while file sink remains blocked");
Check(log.Dropped == 9998, "capacity strictly bounds pending ordinary entries");
Check(!log.Complete(20), "shutdown returns without waiting indefinitely on file sink");
sink.Release.Set();
Check(log.Complete(3000), "worker drains after sink recovers");
Check(sink.Text.Contains("F fatal-reserved"), "fatal survives saturated ordinary queue");
Check(sink.Text.Contains("[diag-log] dropped=9998"), "loss is visible in the retained log");
Check(sink.Text.IndexOf("ordinary-0") < sink.Text.IndexOf("ordinary-1"), "accepted ordinary order preserved");

// A blocked stdout/stderr pipe is also isolated from the game and watchdog.
var file = new StringWriter();
var console = new GateWriter();
var consoleLog = new AsyncDiagnosticLog(file, 2);
consoleLog.Write("echo", "E ", console);
Check(console.Entered.Wait(3000), "worker reached blocked console sink");
Check(Task.Run(() => { for (int i = 0; i < 10000; i++) consoleLog.Write("next", ""); }).Wait(3000),
    "producer completes while console sink remains blocked");
Check(!consoleLog.Complete(20), "bounded shutdown also covers console sink");
console.Release.Set();
Check(consoleLog.Complete(3000), "console drain completes after release");
Check(file.ToString().Contains("E echo"), "file received mirrored text");

// The file keeps logging while nobody reads the console (issue #19).
var unreadFile = new LockedWriter();
var unread = new GateWriter();
var unreadLog = new AsyncDiagnosticLog(unreadFile, 16);
for (int i = 0; i < 200; i++)
{
    unreadLog.Write($"line-{i}", "", unread);
    for (int wait = 0; wait < 300 && !unreadFile.Text.Contains($"line-{i}\n"); wait++) Thread.Sleep(1);
}
Check(unreadFile.Text.Contains("line-199\n"), "file log continues while console sink is blocked");
Check(unreadLog.Dropped == 0 && unreadLog.EchoDropped > 0, "only console lines are dropped");
Check(unreadFile.Text.Split("console output is not being read").Length == 2, "console loss is noted once in the file");
unread.Release.Set();
Check(unreadLog.Complete(3000) && unreadFile.Text.Contains("consoleDropped="), "console loss is counted at shutdown");

var boundedText = new StringWriter();
var large = new AsyncDiagnosticLog(boundedText);
large.Write(new string('x', 1000000), "");
Check(large.Complete(3000), "oversize entry drains");
Check(large.Truncated == 1 && boundedText.ToString().Length < 17000, "individual text allocation retained by queue is bounded");

var broken = new AsyncDiagnosticLog(new BrokenWriter());
var echo = new StringWriter();
broken.Write("survives", "", echo);
Check(broken.Complete(3000), "sink exception does not kill process or drain");
Check(broken.SinkErrors > 0 && echo.ToString().Contains("survives"), "file error does not suppress independent console sink");

var concurrentFile = new StringWriter();
var concurrent = new AsyncDiagnosticLog(concurrentFile, 256);
var writers = Enumerable.Range(0, 4).Select(thread => Task.Run(() =>
{
    for (int i = 0; i < 32; i++) concurrent.Write($"thread-{thread}-{i}", "");
})).ToArray();
Check(Task.WaitAll(writers, 3000) && concurrent.Complete(3000), "multiple producers and worker finish");
var lines = concurrentFile.ToString().Split('\n', StringSplitOptions.RemoveEmptyEntries);
Check(lines.Length == 128 && lines.Distinct().Count() == 128 && concurrent.Dropped == 0,
    "concurrent watchdog/game producers retain every accepted record once");
var idleFile = new StringWriter();
var idle = new AsyncDiagnosticLog(idleFile);
idle.Write("idle-fatal", "", critical: true);
Check(idle.Complete(3000) && idleFile.ToString().Contains("idle-fatal"), "fatal-only submission wakes idle reader");
Console.WriteLine($"PASS: {assertions} diagnostic logging assertions");

sealed class LockedWriter : TextWriter
{
    readonly StringBuilder _text = new();
    public string Text { get { lock (_text) return _text.ToString(); } }
    public override Encoding Encoding => Encoding.UTF8;
    public override void Write(char value) { lock (_text) _text.Append(value); }
    public override void WriteLine(string? value) { lock (_text) _text.Append(value).Append('\n'); }
}
sealed class GateWriter : TextWriter
{
    public readonly ManualResetEventSlim Entered = new(false), Release = new(false);
    readonly StringBuilder _text = new();
    public string Text => _text.ToString();
    public override Encoding Encoding => Encoding.UTF8;
    public override void WriteLine(string? value)
    {
        Entered.Set();
        Release.Wait();
        _text.AppendLine(value);
    }
}
sealed class BrokenWriter : TextWriter
{
    public override Encoding Encoding => Encoding.UTF8;
    public override void WriteLine(string? value) => throw new IOException("injected sink failure");
}
