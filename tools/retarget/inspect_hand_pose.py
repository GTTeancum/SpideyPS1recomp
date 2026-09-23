"""Render two offline hand diagnostics using anatomical influences, not packet IDs.

Candidate rotations exist only in memory. This never writes an actor or accepts
visual closure. Inspect native gameplay separately before installing a policy.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
import struct

import numpy as np
from PIL import Image

from proof_render import caption, load_asset, render, triangles
from rig_blob import Rig
from scene import axis_angle, unit


def hand_vertices(rig, side):
    names = rig.provenance['boneNames']
    palm = names.index('Clown001' + side + 'ArmPalm')
    descendants = {palm}
    while True:
        expanded = descendants | {i for i, parent in enumerate(rig.provenance['sourceParents'])
                                  if parent in descendants}
        if expanded == descendants:
            break
        descendants = expanded
    selected = set()
    for i in range(rig.count):
        first, count = struct.unpack_from('<II', rig.blob, rig.h[8] + i * 32 + 24)
        weights = [struct.unpack_from('<If', rig.blob, rig.h[9] + (first+j)*8)
                   for j in range(count)]
        if any(bone in descendants and weight > 0 for bone, weight in weights):
            selected.add(i)
    return palm, selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--actor', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--side', choices=('L', 'R'), default='L')
    parser.add_argument('--base', type=float, default=90)
    parser.add_argument('--distal', type=float, default=85)
    parser.add_argument('--yaw', type=float, default=20)
    args = parser.parse_args()
    if args.out.exists():
        parser.error('Choose a fresh diagnostic directory')
    if not all(np.isfinite(v) for v in (args.base, args.distal, args.yaw)):
        parser.error('Angles must be finite')
    parsed, original = load_asset(args.actor)
    names = original.provenance['boneNames']
    palm, selected_vertices = hand_vertices(original, args.side)
    _, opened = original.evaluate(flags=1)
    prefix = 'Clown001' + args.side + 'ArmDigit'
    middle = prefix + ('31' if prefix + '31' in names else '21')
    origin = opened[palm, :, 3]
    forward = unit(opened[names.index(middle), :, 3] - origin)
    across = unit(opened[names.index(prefix+'51'), :, 3] - opened[names.index(prefix+'21'), :, 3])
    inward = unit(np.cross(across, forward))
    if inward @ opened[palm, :, 0] < 0:
        inward = -inward
    basis = np.column_stack([forward, -inward, unit(np.cross(forward, -inward))])
    blob = bytearray(original.blob)
    changes = []
    for row in original.provenance['fistPose']:
        match = re.fullmatch(r'Clown001[LR]ArmDigit([1-9])([1-3])', row['bone'])
        if not match or 'angleDegrees' not in row:
            continue
        angle = args.base if match[2] == '1' else args.distal
        index = names.index(row['bone'])
        offset = original.h[7] + index * 196 + 160
        blob[offset:offset+36] = np.asarray(axis_angle(row['axisLocal'], angle), dtype='<f4').tobytes()
        changes.append(dict(bone=row['bone'], before=row['angleDegrees'], after=angle))
    candidate = Rig(blob)
    args.out.mkdir(parents=True)
    coverage = []
    for label, rig in (('current', original), ('candidate', candidate)):
        all_triangles, _, _ = triangles(parsed, rig)
        selected = [t for t in all_triangles if any(int(v) in selected_vertices for v in t['vertex_ids'])]
        assert selected, 'No hand triangles'
        visible = [dict(t, p=(t['p']-origin) @ basis, n=t['n'] @ basis) for t in selected]
        picture = render(visible, Image.new('RGB', (128, 128), (195, 195, 195)),
                         width=720, height=520, yaw=args.yaw, scale=1.3, center=[140, -40], ground=False)
        title = args.actor.parent.name + ': ' + label + ' hand'
        picture = caption(picture, title, 'OFFLINE neutral geometry; all hand-influenced faces; candidate NOT installed')
        picture.save(args.out / (label + '.png'))
        coverage.append(dict(label=label, triangles=len(selected), packets=sorted({t['mesh'] for t in selected})))
    result = dict(actorSha256=hashlib.sha256(args.actor.read_bytes()).hexdigest(),
                  scope='Offline anatomical hand diagnostic, including adjacent wrist triangles. '
                        'No game or visual closure acceptance. Thumb policy unchanged.',
                  side=args.side, handInfluencedVertices=len(selected_vertices), coverage=coverage,
                  candidateAngles=changes, candidateInstalled=False)
    (args.out / 'coverage.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
