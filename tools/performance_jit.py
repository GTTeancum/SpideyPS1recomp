"""Correlate JIT start/load event pairs with first native-update presentation gaps.

Measures event-pair wall spans on the recorded main thread, not CPU time. Keeps
unmatched starts explicit; no inference from missing events or another thread.
"""
import argparse
import json
from pathlib import Path


def summarize(folder, all_gaps=False):
    system = json.loads((folder / 'performance-system.json').read_text())
    frequency, thread = system['stopwatchFrequency'], system['mainThreadId']
    records = (json.loads(line) for path in folder.glob('performance-frames-*.jsonl')
               for line in path.read_text().splitlines())
    frames = sorted((r for r in records if r['Phase'] != 'gpu-batch'), key=lambda r: r['Timestamp'])
    events = [json.loads(line) for line in (folder / 'runtime-events.jsonl').read_text().splitlines()]
    coverage = next((e for e in events if e.get('kind') == 'trace-summary'), None)
    pending, spans = {}, []
    for event in events:
        if event.get('thread') != thread:
            continue
        key = event['payload'].get('MethodID')
        if event['name'] == 'Method/JittingStarted':
            pending[key] = event
        elif event['name'] == 'Method/LoadVerbose' and key in pending:
            start = pending.pop(key)
            spans.append({'start': start['qpcSeconds'], 'end': event['qpcSeconds'],
                          'method': start['payload']['MethodNamespace'] + '.' + start['payload']['MethodName'],
                          'tier': event['payload'].get('OptimizationTier')})
    result = []
    for previous, frame in zip(frames, frames[1:]):
        if (not all_gaps and frame['GameCounter'] != 1) or frame['IntervalMs'] < 100:
            continue
        begin, end = previous['Timestamp'] / frequency, frame['Timestamp'] / frequency
        hits = [dict(s, overlap_ms=(min(end, s['end']) - max(begin, s['start'])) * 1000)
                for s in spans if s['start'] < end and s['end'] > begin]
        # Union avoids double counting nested/overlapping compilation spans.
        stop, total = begin, 0
        for hit in sorted(hits, key=lambda h: h['start']):
            left, right = max(stop, begin, hit['start']), min(end, hit['end'])
            total += max(0, right - left)
            stop = max(stop, right)
        result.append({'present': frame['Present'], 'game_counter': frame['GameCounter'],
                       'phase': frame['Phase'], 'interval_ms': frame['IntervalMs'],
                       'trace_covers_interval': bool(coverage and coverage['lost'] == 0
                           and coverage.get('startQpcSeconds') is not None
                           and coverage.get('endQpcSeconds') is not None
                           and coverage['startQpcSeconds'] <= begin and coverage['endQpcSeconds'] >= end),
                       'start': begin, 'end': end, 'main_thread_jit_wall_ms': total * 1000,
                       'methods': sorted(hits, key=lambda h: h['overlap_ms'], reverse=True)})
    return {'main_thread': thread, 'selection': 'all gaps over 100ms' if all_gaps else 'first native update',
            'first_update_hitches' if not all_gaps else 'hitches': result,
            'unmatched_main_thread_starts': list(pending.values()),
            'trace': coverage,
            'note': __doc__}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--all-gaps', action='store_true', help='Include every recorded gap over 100ms, preserving gameplay counters and phases')
    args = parser.parse_args()
    print(json.dumps(summarize(args.folder, args.all_gaps), indent=2))
