using Microsoft.Diagnostics.Tracing;
using System.Text.Json;

if (args.Length != 2 && !(args.Length == 4 && args[2] == "--sample-thread" && int.TryParse(args[3], out _)))
    throw new ArgumentException("Usage: PerformanceTrace INPUT.nettrace OUTPUT.jsonl [--sample-thread THREAD_ID]");
int? sampleThread = args.Length == 4 ? int.Parse(args[3]) : null;
using var source = new EventPipeEventSource(args[0]);
using var output = new StreamWriter(args[1]);
using var system = JsonDocument.Parse(File.ReadAllText(Path.Combine(
    Path.GetDirectoryName(Path.GetFullPath(args[0]))!, "performance-system.json")));
if (!system.RootElement.GetProperty("os").GetString()!.Contains("Windows"))
    throw new NotSupportedException("This cross-process QPC exporter currently supports Windows captures only.");
double frequency = system.RootElement.GetProperty("stopwatchFrequency").GetDouble();
// Absolute QPC is necessary to correlate independent processes; relative event
// time alone cannot be joined to the game's native frame log.
#pragma warning disable CS0618
long count = 0;
double? anchorQpc = null;
DateTime anchorTime = default;
source.Clr.All += e =>
{
    if (anchorQpc == null) { anchorQpc = e.TimeStampQPC / frequency; anchorTime = e.TimeStamp; }
    if (sampleThread != null) return;
    if (!e.EventName.StartsWith("GC/") && !e.EventName.StartsWith("Contention/") &&
        !e.EventName.StartsWith("Method/") && !e.EventName.StartsWith("Loader/")) return;
    var payload = new Dictionary<string, string?>();
    foreach (string name in e.PayloadNames)
        payload[name] = Convert.ToString(e.PayloadByName(name), System.Globalization.CultureInfo.InvariantCulture);
    output.WriteLine(JsonSerializer.Serialize(new {
        name = e.EventName, qpcSeconds = e.TimeStampQPC / frequency,
        utc = e.TimeStamp.ToUniversalTime(), thread = e.ThreadID, payload
    }));
    count++;
};
if (sampleThread != null)
    source.Dynamic.All += e =>
    {
        if (e.ProviderName != "Microsoft-DotNETCore-SampleProfiler" || e.ThreadID != sampleThread) return;
        output.WriteLine(JsonSerializer.Serialize(new {
            kind = "sample", name = e.EventName, qpcSeconds = e.TimeStampQPC / frequency,
            thread = e.ThreadID
        }));
        count++;
    };
source.Process();
output.WriteLine(JsonSerializer.Serialize(new { kind = "trace-summary", lost = source.EventsLost,
    startQpcSeconds = anchorQpc + (source.SessionStartTime - anchorTime).TotalSeconds,
    endQpcSeconds = anchorQpc + (source.SessionEndTime - anchorTime).TotalSeconds }));
Console.WriteLine($"Exported {count} {(sampleThread == null ? "runtime events" : "profiler events for the selected thread")}; lost events: {source.EventsLost}");
