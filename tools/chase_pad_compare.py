"""Compare bounded native SM1 pad traces without changing game state."""
import argparse
import collections
import json
import re
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('first', type=Path)
p.add_argument('second', type=Path)
p.add_argument('--output', required=True, type=Path)
p.add_argument('--start', type=int, default=400, help='Minimum level-relative console tick')
p.add_argument('--native-start', type=int, help='Compare from this native game counter without requiring an archive-tick anchor')
a = p.parse_args()
fields = ['held', 'pressed', 'script', 'controller', 'player', 'x', 'y', 'z', 'yaw']
inputs = []
for folder in (a.first, a.second):
    log = (folder / 'console.log').read_text(errors='replace')
    anchors = re.findall(r"'l5a1_t\.trg' (?:load #\d+ )?at frame (\d+)", log)
    if not anchors and a.native_start is None:
        raise SystemExit(f'No Chase anchor: {folder}')
    anchor = int(anchors[0]) if anchors else None
    rows = [json.loads(line.split('[pad-trace] ', 1)[1]) for line in log.splitlines()
            if line.startswith('[pad-trace] {')]
    if not rows:
        raise SystemExit(f'No pad trace records: {folder}')
    counts = collections.Counter()
    index = {}
    for row in rows:
        if (a.native_start is not None and row['gameCounter'] < a.native_start) or (
                a.native_start is None and row['tick'] < anchor + a.start):
            continue
        key = (row['gameCounter'], row['caller'])
        ordinal = counts[key]
        counts[key] += 1
        index[(*key, ordinal)] = row
    inputs.append(({'run': str(folder), 'anchor': anchor, 'records': len(rows),
                    'contiguous': all(r['record'] == i + 1 for i, r in enumerate(rows)),
                    'cap_reached': '[pad-trace] record limit reached' in log}, index))
left, right = inputs[0][1], inputs[1][1]
common = sorted(left.keys() & right.keys(), key=lambda key: left[key]['record'])
differences = []
for key in common:
    changed = [f for f in fields if left[key][f] != right[key][f]]
    if changed:
        differences.append({'key': key, 'different': changed, 'a': left[key], 'b': right[key]})
report = {'runs': [item[0] for item in inputs], 'start_level_tick': a.start if a.native_start is None else None,
          'start_native_counter': a.native_start,
          'alignment': 'native game counter, caller, occurrence; not wall time or trace row',
          'matched_keys': len(common), 'unmatched_keys': [len(left.keys()-right.keys()), len(right.keys()-left.keys())],
          'difference_count': len(differences), 'first_differences': differences[:10],
          'first_position_difference': next((d for d in differences if any(f in d['different'] for f in ('x', 'y', 'z', 'yaw'))), None)}
if not common:
    raise SystemExit('No comparable native calls')
a.output.write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
