"""Read-only structural preflight. Passing is not conversion or gameplay proof."""
import argparse
from pathlib import Path

from batch_smu import inventory, material_policy, save_json
from convert import donor_info
from rig_blob import unique_vertices, packetize, make_blob, Rig
from scene import Scene, calibrate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--samples', type=Path, required=True)
    parser.add_argument('--donor', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    reference = Scene(args.samples / 'costumes/2099/2099.fbx')
    origins, ground, _, _ = donor_info(args.donor.read_bytes())
    records = []
    for row in inventory(args.samples):
        result = dict(key=row['key'], status='unsupported')
        try:
            if row['sourceMeshCount'] != 1 or len(row['textureFiles']) != 1:
                raise ValueError('Requires lossless multi-mesh/material support')
            material_policy(args.samples / row['fbx'], args.samples / row['textureFiles'][0])
            source = Scene(args.samples / row['fbx'])
            calibration = calibrate(source, reference, origins, ground)
            vertices, controls, faces = unique_vertices(source, calibration)
            packets, bins, overflow = packetize(faces, controls, source, calibration)
            rig = Rig(make_blob(source, calibration, vertices, controls, faces, packets))
            posed, _ = rig.evaluate()
            result.update(status='preflight-pass', triangles=len(faces), rigBones=rig.bones,
                vertices=rig.count, metadataBytes=len(rig.blob), spilloverTriangles=overflow,
                height=float(posed[:, 1].max() - posed[:, 1].min()))
        except (ValueError, OSError, KeyError, AssertionError) as error:
            result['reason'] = str(error)
            print(row['key'] + ': ' + str(error), flush=True)
        records.append(result)
        save_json(args.out, dict(scope='Structural preflight only; no converted assets or gameplay acceptance',
            complete=len(records), requested=233, results=records))
    print('Audited', len(records), 'suits;', sum(r['status'] == 'preflight-pass' for r in records), 'passed preflight')


if __name__ == '__main__':
    main()
