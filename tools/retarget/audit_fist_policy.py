"""Check actual weighted finger curling; this is not visual fist acceptance."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
from proof_render import load_asset


def audit(asset):
    _,rig=load_asset(asset)
    opened,bones=rig.evaluate(flags=1)
    closed,_=rig.evaluate()
    names=rig.provenance['boneNames']
    weights=[]
    for i in range(rig.count):
        first,count=struct.unpack_from('<II',rig.blob,rig.h[8]+i*32+24)
        weights.append([struct.unpack_from('<If',rig.blob,rig.h[9]+(first+j)*8) for j in range(count)])
    results=[]
    for side in ('L','R'):
        prefix='Clown001'+side+'Arm'
        palm=names.index(prefix+'Palm')
        middle=names.index(prefix+('Digit31' if prefix+'Digit31' in names else 'Digit21'))
        forward=bones[middle,:,3]-bones[palm,:,3]
        forward/=np.linalg.norm(forward)
        distal={i for i,name in enumerate(names) if name in {prefix+'Digit22',prefix+'Digit32',prefix+'Digit52'}}
        mask=np.array([any(bone in distal and weight>.5 for bone,weight in row) for row in weights])
        if not mask.any():
            results.append(dict(side=side,passed=False,reason='No predominantly distal-weighted vertices'))
            continue
        before=float(((opened[mask,:3]-bones[palm,:,3])@forward).mean())
        after=float(((closed[mask,:3]-bones[palm,:,3])@forward).mean())
        results.append(dict(side=side,passed=bool(np.isfinite([before,after]).all() and before>0 and after<before*.85),
                            meanForwardOpen=before,meanForwardClosed=after,distalVertices=int(mask.sum())))
    return dict(suit=asset.parent.name,assetSha256=hashlib.sha256(asset.read_bytes()).hexdigest(),
                passed=all(r['passed'] for r in results),hands=results)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suits',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    records=[audit(path) for path in sorted(args.suits.glob('smu-*/actor.psx'))]
    result=dict(scope='Offline compiled-core geometry check of fist-policy curl; not visual hand closure or contact validation.',
                status='PASS' if records and all(r['passed'] for r in records) else 'FAIL',
                count=len(records),results=records)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(result,indent=2)+'\n')
    print(result['status'],len(records),'suits')
    for row in records:
        if not row['passed']:print(json.dumps(row))
    return 0 if result['status']=='PASS' else 1


if __name__=='__main__':
    raise SystemExit(main())
