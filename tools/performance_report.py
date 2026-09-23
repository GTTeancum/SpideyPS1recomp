"""Summarize local performance logs from either game; Python standard library only."""
import argparse
import collections
import json
from pathlib import Path


def percentile(values, fraction):
    if not values:
        return None
    values = sorted(values)
    return round(values[min(len(values) - 1, int((len(values) - 1) * fraction))], 4)


def summarize(folder, start=0, end=float("inf")):
    system = json.loads((folder / "performance-system.json").read_text(encoding="utf-8-sig"))
    frequency = system["stopwatchFrequency"]
    records = []
    partial = 0
    for path in folder.glob("performance-frames-*.jsonl"):
        for line in path.read_text(encoding="utf-8-sig").splitlines():
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError:
                partial += 1
    records.sort(key=lambda r: r["Timestamp"])
    frames = [r for r in records if r["Phase"] != "gpu-batch"]
    if not frames:
        return {"system": system, "error": "No presentation records", "partial_lines": partial}
    origin = frames[0]["Timestamp"]
    selected = lambda r: start <= (r["Timestamp"] - origin) / frequency <= end
    frames = [r for r in frames if selected(r)]
    groups = collections.defaultdict(list)
    for frame in frames:
        groups[(frame["Phase"], frame["CounterEpoch"])].append(frame)
    results = []
    for (phase, epoch), rows in groups.items():
        intervals = [r["IntervalMs"] for r in rows if r["IntervalMs"] > 0]
        seconds = (rows[-1]["Timestamp"] - rows[0]["Timestamp"]) / frequency
        # Counter difference is valid within one epoch. Phase groups can have gaps;
        # report the span, rather than pretending these are contiguous gameplay.
        results.append({"phase": phase, "counter_epoch": epoch, "records": len(rows),
            "span_seconds": round(seconds, 3),
            "native_updates_per_span_second": round((rows[-1]["GameCounter"] - rows[0]["GameCounter"]) / seconds, 3) if seconds else None,
            "interval_ms": {"p50": percentile(intervals, .5), "p95": percentile(intervals, .95),
                "p99": percentile(intervals, .99), "max": max(intervals, default=None)},
            "intervals_over_50ms": sum(v > 50 for v in intervals),
            "intervals_over_100ms": sum(v > 100 for v in intervals),
            "mean_wall_ms": {key: round(sum(r[key] for r in rows) / len(rows), 4)
                for key in ("IrqWaitMs", "PresentWaitMs", "PresentCallMs", "OutsidePresentWallMs", "VertexFenceWaitMs")},
            "uploaded_bytes": sum(r["UploadedBytes"] for r in rows)})
    gpu = [r for r in records if r["Phase"] == "gpu-batch" and selected(r)]
    process = []
    for path in folder.glob("performance-process-*.jsonl"):
        for line in path.read_text(encoding="utf-8-sig").splitlines():
            try:
                row = json.loads(line)
                if selected(row):
                    process.append(row)
            except json.JSONDecodeError:
                partial += 1
    process.sort(key=lambda r: r["Timestamp"])
    cpu = {"priority_class_samples": dict(collections.Counter(
        row.get("priorityClass") or "unavailable" for row in process))}
    if len(process) > 1:
        seconds = (process[-1]["Timestamp"] - process[0]["Timestamp"]) / frequency
        if seconds > 0:
            for key in ("cpuSeconds", "mainThreadCpuSeconds"):
                if process[0].get(key) is not None and process[-1].get(key) is not None:
                    cpu[key + "_per_wall_second"] = round((process[-1][key] - process[0][key]) / seconds, 4)
        cpu["private_bytes_first_last_peak"] = [process[0]["privateBytes"], process[-1]["privateBytes"], max(r["privateBytes"] for r in process)]
        cpu["gpu_queries_skipped"] = process[-1].get("GpuQueriesSkipped")
    return {"system": system, "selection_seconds_from_first_retained_frame": [start, None if end == float("inf") else end],
        "groups": results, "process": cpu, "dropped_records": max((r["Dropped"] for r in records), default=0),
        "partial_lines": partial, "gpu_query_records": len(gpu),
        "available_gpu_batch_ms_total": round(sum(r.get("GpuElapsedMs") or 0 for r in gpu), 4),
        "note": "Intervals are host presentation starts, not measured display delivery. Outside-present is wall time. GPU query records are separate, optional, and can be incomplete. Rotated logs retain only the recent bounded window."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--start", type=float, default=0)
    parser.add_argument("--end", type=float, default=float("inf"))
    args = parser.parse_args()
    print(json.dumps(summarize(args.folder, args.start, args.end), indent=2))
