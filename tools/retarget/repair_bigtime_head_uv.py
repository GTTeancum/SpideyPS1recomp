"""Inset Big Time's head-island seam UVs without changing art, geometry, or rigs.

Both games ship these shared actors. The original 7-bit UV conversion rounded
the crown/back boundary onto the atlas's black gutters. Some right-side values
also rounded asymmetrically. This repair changes only native face UV bytes.
Run with --apply to repair the two source actors; otherwise audit known hashes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from proof_render import load_asset

ROOT = Path(__file__).resolve().parents[2]
HASHES = {
    'smu-bigtime': ('ae11bc25f1ae02b61b0760fa879f5b0b0a2b31a8ff4382602ab499ac9d7aedd0',
                    'e76bfb409e1e35408113c33e4976de8a12d557cdc92c3f799a8c86da11c96118'),
    'smu-bigtime-red': ('319ed735ab7dbcc2056c196033382568761a70025021d741d88c8e5a1eaeba9d',
                        'c2c0bb5b483ad1a7a438e92a78c8c1a03f5d8529aa44fc148a3af92cc13e635f'),
}
# The later pose calibration changes RTG2 only; these actors retain the verified UV bytes.
POSE_CALIBRATED = {
    'smu-bigtime': ('2df5db3dd6ec9824a533e7972f7e3ba8b77944019c79e581cd7b7e0cbf349b84',
                    '7440b9174e9ee5d805b171bde3c027599f142c4de24e69606646d01d95ce5aca'),
    'smu-bigtime-red': ('787b81eaf75d142b78b1598312a3ea0dddd49be7610f2d6fc174f493de7dba5d',
                        '09fb3d8e311ac79428e48f1baa4a03b4a35735a1b2172ffd2c47e3d20cbe6559'),
}
UV = {(92, 3): (92, 4), (102, 3): (102, 4),
      (86, 1): (86, 2), (108, 1): (108, 2),
      (80, 1): (80, 2), (114, 1): (114, 2),
      (75, 3): (76, 4), (119, 3): (118, 4),
      (71, 6): (72, 7), (123, 6): (122, 7),
      (69, 10): (70, 10), (126, 10): (124, 10)}
for y in range(10, 33):
    UV[68, y] = (70, y)
    UV[126, y] = (124, y)


def repair(path):
    original = path.read_bytes()
    data = bytearray(original)
    parsed, rig = load_asset(path)
    world, _ = rig.evaluate()
    packets = rig.packets()
    permitted = set()
    corners = 0
    for i, mesh in enumerate(parsed['meshes']):
        offset = struct.unpack_from('<I', data, 664 + i * 4)[0]
        nv, nn = struct.unpack_from('<HH', data, offset + 2)
        face_offset = offset + 28 + 8 * (nv + nn)
        for face in mesh['faces']:
            # Native rest pose: the head is entirely above -1200; neck/body UVs
            # and any coincident atlas values elsewhere must remain untouched.
            if world[packets[i][face['indices']], 1].max() < -1200:
                for corner, uv in enumerate(face['uv']):
                    if tuple(uv) in UV:
                        p = face_offset + 20 + 2 * corner
                        data[p:p + 2] = bytes(UV[tuple(uv)])
                        permitted.update((p, p + 1))
                        corners += 1
            face_offset += struct.unpack_from('<H', data, face_offset + 2)[0]
    changed = {i for i, (a, b) in enumerate(zip(original, data)) if a != b}
    assert len(original) == len(data) and changed <= permitted
    assert corners == 70, corners
    # Metadata, RTG2 source rig/influences, all vertices, indices, normal vectors,
    # material bindings, animations and embedded fallback textures are byte exact.
    return bytes(data), dict(corners=70, changed_bytes=len(changed), only_face_uv_bytes=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    for suit, (before, after) in HASHES.items():
        path = ROOT / 'spiderman/port/mods/suits' / suit / 'actor.psx'
        current = hashlib.sha256(path.read_bytes()).hexdigest()
        if current in POSE_CALIBRATED[suit]:
            print(suit + ': repaired UVs verified on pose-calibrated actor')
            continue
        if current == after:
            if args.apply:
                record_repair(path, before, after)
            print(suit + ': repaired hash verified')
            continue
        if current != before:
            raise ValueError('Unrecognized actor; refusing to modify ' + str(path))
        data, evidence = repair(path)
        assert hashlib.sha256(data).hexdigest() == after
        if args.apply:
            path.write_bytes(data)
            record_repair(path, before, after)
        print(json.dumps(dict(suit=suit, applied=args.apply, before=before, after=after, **evidence)))


def record_repair(path, before, after):
    report_path = path.parent / 'conversion-report.json'
    report = json.loads(report_path.read_text())
    report['outputSha256'] = after
    report['headUvRepair'] = dict(method='inset native head-island boundary UVs',
        originalSha256=before, repairedSha256=after, corners=70, changedBytes=84,
        preserved='all non-UV actor bytes and external textures')
    report_path.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
