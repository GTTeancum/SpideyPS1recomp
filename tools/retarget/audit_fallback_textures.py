"""Check every native fallback material against its authored source, without animation sampling."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from batch_smu import inventory
from materials import bindings
from proof_render import load_asset
from scene import Scene
from test_retarget import expected_fallback_rgba


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suits',type=Path,required=True)
    parser.add_argument('--samples',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    sources={row['id']:row for row in inventory(args.samples)}
    results=[]
    for asset in sorted(args.suits.glob('smu-*/actor.psx')):
        parsed,rig=load_asset(asset)
        source_record=sources[asset.parent.name]
        source=args.samples/source_record['fbx']
        if source_record['fbxSha256']!=rig.provenance['sourceSha256']:
            raise ValueError('Source identity changed: '+asset.parent.name)
        scene=Scene(source)
        records,images,_,_=bindings(scene,args.samples/source_record['textureFiles'][0])
        count_matches=len(records)==len(parsed['textures'])
        materials=[]
        for slot,(record,image) in enumerate(zip(records,images)):
            if slot>=len(parsed['textures']):
                materials.append(dict(slot=slot,alphaBound=record['alphaBound'],mismatchedPixels=128*128,missing=True))
                continue
            expected=expected_fallback_rgba(image,record['alphaBound'])
            actual=parsed['textures'][slot]
            materials.append(dict(slot=slot,alphaBound=record['alphaBound'],
                                  mismatchedPixels=int(np.any(actual!=expected,axis=2).sum())))
        row=dict(suit=asset.parent.name,actorSha256=hashlib.sha256(asset.read_bytes()).hexdigest(),
                 sourceSha256=scene.sha256,materials=materials,
                 expectedMaterialCount=len(records),actualMaterialCount=len(parsed['textures']),
                 status='PASS' if count_matches and all(m['mismatchedPixels']==0 for m in materials) else 'FAIL')
        results.append(row)
        if row['status']!='PASS':print(json.dumps(row),flush=True)
    report=dict(scope='Exact source-derived RGB5/STP/alpha and row-order check for every native fallback material. Not gameplay or original shader equivalence.',
                status='PASS' if results and all(r['status']=='PASS' for r in results) else 'FAIL',
                suits=len(results),materials=sum(len(r['materials']) for r in results),results=results)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'],report['suits'],'suits,',report['materials'],'materials')
    return 0 if report['status']=='PASS' else 1


if __name__=='__main__':raise SystemExit(main())
