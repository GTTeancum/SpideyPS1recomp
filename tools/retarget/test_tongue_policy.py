"""Verify a distal tongue pose edit does not move the mouth, body or other weights."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np

from animation_bank import read
from proof_render import load_asset


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before',type=Path,required=True)
    parser.add_argument('--after',type=Path,required=True)
    parser.add_argument('--animation-bank',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    _,before=load_asset(args.before);_,after=load_asset(args.after)
    original=dict(before.provenance);updated=dict(after.provenance)
    policy=updated.pop('appendagePose')
    assert original==updated, 'Original source provenance must not change'
    assert len(policy)==1 and policy[0]['bone']=='Tongue_c'
    names=original['boneNames'];changed={names.index('Tongue_c'),names.index('Tongue_d')}
    assert np.array_equal(before.source_vertices(),after.source_vertices())
    assert np.array_equal(before.faces(),after.faces())
    assert all(np.array_equal(a,b) for a,b in zip(before.packets(),after.packets()))
    affected=[]
    for i in range(before.count):
        start,count=struct.unpack_from('<II',before.blob,before.h[8]+i*32+24)
        influences={struct.unpack_from('<I',before.blob,before.h[9]+(start+j)*8)[0]
                    for j in range(count)}
        affected.append(bool(influences&changed))
    unaffected=np.logical_not(affected)
    fixed_bones=[i for i in range(before.bones) if i not in changed]
    max_fixed_bone=max_fixed_vertex=0.;frames=0
    for clip in read(args.animation_bank):
        for driver in clip:
            av,ab=before.evaluate(driver);bv,bb=after.evaluate(driver)
            max_fixed_bone=max(max_fixed_bone,float(abs(ab[fixed_bones]-bb[fixed_bones]).max()))
            max_fixed_vertex=max(max_fixed_vertex,float(abs(av[unaffected]-bv[unaffected]).max()))
            frames+=1
    _,a=before.evaluate();_,b=after.evaluate()
    tip=names.index('Tongue_d');joint=names.index('Tongue_c')
    tip_change=float(np.linalg.norm(b[tip,:,3]-a[tip,:,3]))
    length_change=float(abs(np.linalg.norm(b[tip,:,3]-b[joint,:,3])-np.linalg.norm(a[tip,:,3]-a[joint,:,3])))
    passed=max_fixed_bone<.002 and max_fixed_vertex<.002 and tip_change>1 and length_change<.002
    report=dict(status='PASS' if passed else 'FAIL',frames=frames,
                scope='Offline exact runtime core. Original source data and unaffected mouth/body/fingers must remain unchanged.',
                beforeSha256=hashlib.sha256(args.before.read_bytes()).hexdigest(),
                afterSha256=hashlib.sha256(args.after.read_bytes()).hexdigest(),
                affectedVertices=sum(affected),unaffectedVertices=int(unaffected.sum()),
                maxUnaffectedBoneError=max_fixed_bone,maxUnaffectedVertexError=max_fixed_vertex,
                tipPositionChange=tip_change,distalLengthChange=length_change,policy=policy)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return 0 if passed else 1


if __name__=='__main__':
    raise SystemExit(main())
