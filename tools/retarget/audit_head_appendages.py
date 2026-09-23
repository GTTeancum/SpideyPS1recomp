"""Check preserved tongue attachment through the runtime core, not visual acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import numpy as np

from animation_bank import read
from proof_render import load_asset


def affine(m):
    result = np.eye(4)
    result[:3] = m
    return result


def audit(asset, clips):
    _, rig = load_asset(asset)
    names = rig.provenance['boneNames']
    tongue = [i for i, name in enumerate(names) if 'tongue' in name.lower()]
    if not tongue:
        return None
    head = names.index('Clown001Head')
    parents = rig.provenance['sourceParents']
    descendants = {head}
    for i, parent in enumerate(parents):
        if parent in descendants:
            descendants.add(i)
    if not set(tongue) <= descendants:
        raise ValueError(f'{asset}: tongue not under head; requires explicit attachment policy')
    for i in descendants - {head}:
        driver = struct.unpack_from('<i', rig.blob, rig.h[7] + i * 196 + 4)[0]
        if driver >= 0:
            raise ValueError(f'{asset}: separately driven head descendant {names[i]}')
    vertex_ids = []
    mixed_ids = []
    for i in range(rig.count):
        start, count = struct.unpack_from('<II', rig.blob, rig.h[8] + i * 32 + 24)
        influences = {struct.unpack_from('<I', rig.blob, rig.h[9] + (start+j)*8)[0]
                      for j in range(count)}
        if influences & set(tongue):
            (vertex_ids if influences <= descendants else mixed_ids).append(i)
    rest_vertices, rest_bones = rig.evaluate()
    head_inverse = np.linalg.inv(affine(rest_bones[head]))
    relative_bones = head_inverse @ np.array([affine(rest_bones[i]) for i in tongue])
    relative_vertices = (head_inverse @ np.c_[rest_vertices[vertex_ids, :3],
                                             np.ones(len(vertex_ids))].T).T
    max_bone_error = max_vertex_error = 0.0
    frames = 0
    for clip in clips:
        for driver in clip:
            vertices, bones = rig.evaluate(driver)
            inverse = np.linalg.inv(affine(bones[head]))
            actual_bones = inverse @ np.array([affine(bones[i]) for i in tongue])
            actual_vertices = (inverse @ np.c_[vertices[vertex_ids, :3],
                                               np.ones(len(vertex_ids))].T).T
            max_bone_error = max(max_bone_error, float(abs(actual_bones-relative_bones).max()))
            if vertex_ids:
                max_vertex_error = max(max_vertex_error, float(abs(actual_vertices-relative_vertices).max()))
            frames += 1
    return dict(suit=asset.parent.name, assetSha256=hashlib.sha256(asset.read_bytes()).hexdigest(),
                tongueBones=[names[i] for i in tongue], frames=frames,
                headRelativeVertices=len(vertex_ids), mixedBodyTongueVertices=len(mixed_ids),
                maxHeadRelativeBoneError=max_bone_error, maxHeadRelativeVertexError=max_vertex_error,
                status='PASS' if max(max_bone_error, max_vertex_error) < .01 else 'FAIL')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suits', type=Path, required=True)
    parser.add_argument('--ids', nargs='*')
    parser.add_argument('--animation-bank', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    clips = read(args.animation_bank)
    assets = ([args.suits / key / 'actor.psx' for key in args.ids] if args.ids else
              sorted(args.suits.glob('*/actor.psx')))
    results = []
    for asset in assets:
        row = audit(asset, clips)
        if row:
            results.append(row)
            print(json.dumps(row), flush=True)
    output = dict(scope='Offline full-animation attachment invariant. Does not accept tongue pose, '
                        'mouth intersection, dynamic tongue motion, or live game appearance. '
                        'Mixed body/tongue vertices are counted but excluded from the rigid head-relative vertex test.',
                  scannedAssets=len(assets),
                  animationBankSha256=hashlib.sha256(args.animation_bank.read_bytes()).hexdigest(),
                  results=results, status='PASS' if results and all(r['status']=='PASS' for r in results) else 'FAIL')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(output, indent=2)+'\n')
    return 0 if output['status']=='PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
