"""Correlate native frame stalls with independent monitor gaps and EventPipe pauses.

Windows perf_counter and .NET Stopwatch share QPC. Requires a native run produced
by performance_native_run.py; optional runtime-events.jsonl comes from PerformanceTrace.
Overlapping delays establish correlation, not causation or measured GPU/display latency.
"""
import argparse
import bisect
import json
from pathlib import Path


def read_lines(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8-sig').splitlines()]


def cpu_bracket(begin, end, samples):
    """Use reads wholly outside the stall, never interpolate coarse CPU samples.

    The runner timestamps the entire collection, enclosing its CPU-time read.
    Whole-process CPU can include other threads and time outside this stall;
    it is therefore conservative when testing whether main-thread computation
    alone could account for the wall interval. It cannot identify a wait cause.
    """
    before = bisect.bisect_right([s['qpc_seconds_end'] for s in samples], begin)-1
    after = bisect.bisect_left([s['qpc_seconds_start'] for s in samples], end)
    if before < 0 or after >= len(samples):
        return None
    first, last = samples[before], samples[after]
    cpu_ms = (last['cpu']-first['cpu'])*1000
    if cpu_ms < 0:
        return None
    # Windows cumulative user/kernel times are quantized. Allow two 15.625 ms
    # ticks of uncertainty; report an estimate, not an OS scheduling measurement.
    allowance = 31.25
    return {'start': first['qpc_seconds_start'], 'end': last['qpc_seconds_end'],
            'collection_bracket_ms': round((last['qpc_seconds_end']-first['qpc_seconds_start'])*1000, 3),
            'whole_process_cpu_ms': round(cpu_ms, 3),
            'cpu_quantization_allowance_ms': allowance,
            'non_cpu_wall_lower_estimate_ms': round(max(0, (end-begin)*1000-cpu_ms-allowance), 3)}


def sample_coverage(begin, end, times):
    if not times:
        return None
    first, last = bisect.bisect_left(times, begin), bisect.bisect_right(times, end)
    gaps = [(b-a, a, b) for a, b in zip(times[max(0, first-1):last],
                                      times[max(0, first-1)+1:last+1])
            if a < end and b > begin]
    largest = max(gaps, default=None)
    return {'samples_in_interval': last-first,
            'bounded_by_samples': times[0] <= begin and times[-1] >= end,
            'largest_sample_gap_ms': round(largest[0]*1000, 3) if largest else None,
            'gap_start': largest[1] if largest else None,
            'gap_end': largest[2] if largest else None}


def host_counter_windows(begin, end, samples):
    """Retain coarse collection envelopes, never interpolate a per-frame value.

    Rate counters span two native collections whose exact instants are enclosed
    by the reported bounds. Queue/memory gauges are observations at the latter
    collection, not averages over this envelope and not necessarily in the stall.
    """
    output = []
    for sample in samples:
        start, stop = sample.get('previous_collection_start'), sample['collection_end']
        if start is None or start >= end or stop <= begin:
            continue
        output.append({'collection_envelope_start': start, 'collection_envelope_end': stop,
                       'collection_envelope_ms': round((stop-start)*1000, 3),
                       'last_collection_ms': round((stop-sample['collection_start'])*1000, 3),
                       'values': sample['values'], 'errors': sample['errors']})
    return output


def summarize(folder, threshold=100, start=60):
    system = json.loads((folder / 'performance-system.json').read_text())
    frequency = system['stopwatchFrequency']
    frames = sorted((r for p in folder.glob('performance-frames-*.jsonl')
                     for r in read_lines(p) if r['Phase'] != 'gpu-batch'), key=lambda r: r['Timestamp'])
    origin = frames[0]['Timestamp'] / frequency
    monitor = json.loads((folder / 'result.json').read_text())['samples']
    cpu_samples = [r for r in monitor if 'qpc_seconds_start' in r and
                   'qpc_seconds_end' in r and 'cpu' in r]
    gaps = []
    collections = [{'start': r['qpc_seconds_start'], 'end': r['qpc_seconds_end']}
                   for r in monitor if 'qpc_seconds_end' in r and
                   r['qpc_seconds_end']-r['qpc_seconds_start'] > .05]
    for previous, current in zip(monitor, monitor[1:]):
        if 'qpc_seconds_end' not in previous or 'qpc_seconds_start' not in current:
            continue
        begin, end = previous['qpc_seconds_end'], current['qpc_seconds_start']
        if end - begin > .65:
            gaps.append({'start': begin, 'end': end, 'unsampled_ms': (end-begin)*1000,
                         'game_cpu_ms': (current['cpu']-previous['cpu'])*1000})
    events_path = folder / 'runtime-events.jsonl'
    heartbeat_path = folder / 'heartbeat.jsonl'
    heartbeat = read_lines(heartbeat_path) if heartbeat_path.exists() else []
    host_path = folder / 'host-counters.jsonl'
    host = read_lines(host_path) if host_path.exists() else []
    host_samples = [row for row in host if row['kind'] == 'sample']
    processes_path = folder / 'host-processes.jsonl'
    processes = read_lines(processes_path) if processes_path.exists() else []
    events = read_lines(events_path) if events_path.exists() else []
    samples_path = folder / 'main-thread-samples.jsonl'
    raw_samples = read_lines(samples_path) if samples_path.exists() else []
    sample_threads = sorted({r['thread'] for r in raw_samples if r.get('kind') == 'sample'})
    if len(sample_threads) > 1:
        raise ValueError('main-thread-samples.jsonl must contain exactly one selected thread')
    sample_times = sorted(r['qpcSeconds'] for r in raw_samples if r.get('kind') == 'sample')
    coverage = next((e for e in events if e.get('kind') == 'trace-summary'), None)
    pauses, contention, pending = [], [], None
    for event in events:
        name = event.get('name')
        if name == 'GC/SuspendEEStart':
            pending = event
        elif name == 'GC/RestartEEStop' and pending:
            pauses.append({'start': pending['qpcSeconds'], 'end': event['qpcSeconds'],
                           'reason': pending['payload'].get('Reason')})
            pending = None
        elif name == 'Contention/Stop':
            duration = float(event['payload'].get('DurationNs', 0)) / 1e9
            contention.append({'start': event['qpcSeconds']-duration,
                               'end': event['qpcSeconds'], 'thread': event['thread']})
    output = []
    def overlaps(a, b, rows):
        return [dict(r, overlap_ms=round(max(0, min(b, r['end'])-max(a, r['start']))*1000, 3))
                for r in rows if r['start'] < b and r['end'] > a]
    for previous, frame in zip(frames, frames[1:]):
        end = frame['Timestamp'] / frequency
        if frame['IntervalMs'] <= threshold or end-origin < start:
            continue
        begin = previous['Timestamp'] / frequency
        output.append({'seconds': round(end-origin, 3), 'interval_ms': frame['IntervalMs'],
            'present': frame['Present'], 'phase': frame['Phase'],
            'previous_present_call_ms': previous['PresentCallMs'],
            'previous_present_stages': previous.get('PresentationStages'),
            'between_presentation_events': frame.get('BetweenPresentationEvents'),
            'irq_wait_ms': frame['IrqWaitMs'], 'limiter_wait_ms': frame['PresentWaitMs'],
            'outside_present_ms': frame['OutsidePresentWallMs'],
            'cpu_bracket': cpu_bracket(begin, end, cpu_samples),
            'main_thread_sample_coverage': sample_coverage(begin, end, sample_times),
            'host_counter_windows': host_counter_windows(begin, end, host_samples),
            'host_process_windows': [r for r in processes if r['kind'] == 'sample'
                and r['previous_collection_start'] < end and r['collection_end'] > begin],
            'monitor_gaps': overlaps(begin, end, gaps),
            'monitor_slow_collections': overlaps(begin, end, collections),
            'heartbeat_delays': overlaps(begin, end, [r for r in heartbeat if r['kind'] == 'delay']),
            'managed_suspensions': overlaps(begin, end, pauses),
            'managed_contention_any_thread': overlaps(begin, end, contention),
            'trace_covers_interval': bool(coverage and coverage['lost'] == 0 and
                coverage['startQpcSeconds'] is not None and coverage['endQpcSeconds'] is not None and
                coverage['startQpcSeconds'] <= begin and coverage['endQpcSeconds'] >= end)})
    return {'threshold_ms': threshold, 'start_seconds': start, 'stalls': output,
            'sample_thread_ids': sample_threads,
            'trace': coverage, 'heartbeat_bounds': [r for r in heartbeat if r['kind'] != 'delay'],
            'host_counter_bounds': [r for r in host if r['kind'] != 'sample'],
            'host_process_bounds': [r for r in processes if r['kind'] != 'sample'],
            'monitor_has_qpc': bool(monitor and 'qpc_seconds_start' in monitor[0]),
            'max_monitor_collection_ms': max(((r['qpc_seconds_end']-r['qpc_seconds_start'])*1000
                for r in monitor if 'qpc_seconds_end' in r), default=None),
            'note': 'Monitor gaps include its intended 500 ms sleep. Coincidence is not causation. '
                    'Managed contention may be on a non-game thread; inspect thread IDs before attribution. '
                    'CPU brackets enclose the full stall and include all process threads and surrounding work. '
                    'A positive non-CPU estimate does not distinguish blocking, preemption, OS/driver waits, '
                    'or suspension, and is not measured GPU time or CPU clock speed. '
                    'Sampled flame charts may extrapolate stale stacks across sample gaps; '
                    'inspect raw sample coverage before attributing a long span to that code. '
                    'Host counters are coarse Windows-provider measurements, not per-frame causes or GHz. '
                    'Queue/memory gauges are endpoint observations, not interval averages. '
                    'Missing counters/coverage are unavailable, never zero; totals can hide a busy core.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--threshold', type=float, default=100)
    parser.add_argument('--start', type=float, default=60)
    args = parser.parse_args()
    print(json.dumps(summarize(args.folder, args.threshold, args.start), indent=2))
