"""Measure the actual submitted Chase rail on every captured render frame."""
import argparse
from collections import defaultdict
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('run', type=Path)
args = parser.parse_args()
frames = defaultdict(list)
with (args.run / 'hud.jsonl').open() as source:
    for line in source:
        row = json.loads(line)
        vertices = row['vertices']
        if ((row['TPage'] & ~0x60) != 8 or row['Clut'] != 419 or
                not row['Textured'] or row['World']):
            continue
        if not all(240 <= v['U'] <= 251 and 248 <= v['V'] <= 255 for v in vertices):
            continue
        frames[row['frame']].append(row)

results = []
for frame, rows in sorted(frames.items()):
    spans = sorted(set((min(v['X'] for v in r['vertices']),
                        max(v['X'] for v in r['vertices'])) for r in rows))
    right = spans[0][1]
    gaps = []
    for left, end in spans[1:]:
        if left > right:
            gaps.append([right, left])
        right = max(right, end)
    results.append(dict(frame=frame, left=spans[0][0], right=right,
                        width=right-spans[0][0], triangles=len(rows), gaps=gaps,
                        all_hud=all(r['Hud'] for r in rows)))
summary = dict(frames=len(results),
               first=results[0] if results else None,
               last=results[-1] if results else None,
               widths=sorted(set(r['width'] for r in results)),
               gap_frames=[r for r in results if r['gaps']],
               unclassified_frames=[r['frame'] for r in results if not r['all_hud']])
(args.run / 'meter-audit.json').write_text(json.dumps(dict(summary=summary, frames=results), indent=2))
print(json.dumps({**summary, 'gap_frames': len(summary['gap_frames']),
                  'gap_first': summary['gap_frames'][0] if summary['gap_frames'] else None,
                  'gap_last': summary['gap_frames'][-1] if summary['gap_frames'] else None,
                  'unclassified_frames': len(summary['unclassified_frames'])}, indent=2))
if not results:
    raise SystemExit('No matching meter geometry: no acceptance claim')
