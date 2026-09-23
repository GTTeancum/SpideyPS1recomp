"""Offline parameter search against a palm surface, never an installation gate."""
import contextlib
import argparse
import io
import json
from pathlib import Path
import runpy
import sys

import numpy as np

work=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--actor',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--dynamic-palm',action='store_true')
parser.add_argument('--wide',action='store_true')
parser.add_argument('--side',choices=('L','R'),default='L')
parser.add_argument('--compact',action='store_true')
args=parser.parse_args()
sys.argv=['trace-hand-digits.py','--side',args.side,'--actor',str(args.actor.resolve()),
          '--out',str(args.out.with_suffix(''))+'-reference']
with contextlib.redirect_stdout(io.StringIO()):
    g=runpy.run_path(str(work/'trace-hand-digits.py'))
rig,opened,names=g['rig'],g['opened'],g['names']
align,Rig=g['align'],g['Rig']
forward,across,inward=g['forward'],g['across'],g['inward']
surface=g['palm_ray_clearance']
palm_ids=np.array([t['vertex_ids'] for t in g['alltri']
                   if any(int(v) in g['selected'] for v in t['vertex_ids'])
                   and int(np.argmax(np.sum([g['vertexweights'][int(v)] for v in t['vertex_ids']],axis=0)))==0])
best=[]
palm_vertices=[i for i in g['selected'] if g['vertexweights'][i][0]>.5]
palm_front=float(np.max((g['open_vertices'][palm_vertices,:3]-g['origin'])@forward))
for row in g['clearance']:
    digit=row['digit']
    digit_vertices=[i for i in g['selected'] if g['vertexweights'][i][g['groups'][digit]]>.5]
    i=names.index(f'Clown001{args.side}ArmDigit{digit}1');j=names.index(f'Clown001{args.side}ArmDigit{digit}2')
    r1,r2=opened[i,:,:3],opened[j,:,:3]
    length=np.linalg.norm(opened[j,:,3]-opened[i,:,3])
    lateral=(opened[names.index(f'Clown001{args.side}ArmDigit31'),:,3]-opened[i,:,3])@across
    winner=None
    for lean in (np.arange(-.4,3.21,.2) if args.wide else (0,.2,.4,.6,.8,1.)):
        q1=align(opened[j,:,3]-opened[i,:,3],inward-forward*lean)
        for convergence in ((0,.35,.7,1.,1.5,2.) if args.wide else (.35,.7,1.)):
            for closure in np.arange(.1,3.01,.1):
                q2=align(opened[j,:,1],-forward+across*(lateral/length)*convergence-inward*closure)
                blob=bytearray(rig.blob)
                for bone,rotation in ((i,r1.T@q1@r1),(j,r2.T@q1.T@q2@r2)):
                    offset=rig.h[7]+bone*196+160
                    blob[offset:offset+36]=np.asarray(rotation,dtype='<f4').tobytes()
                v,_=Rig(blob).evaluate()
                faces=v[palm_ids,:3] if args.dynamic_palm else g['palm_faces']
                gaps=[surface(v[t,:3],faces) for t in row['tipVertices']]
                if any(gap is None or gap<2 for gap in gaps):continue
                score=float(np.mean((np.array(gaps)-6)**2))
                forward_reach=float(np.max((v[digit_vertices,:3]-g['origin'])@forward))
                if args.compact:
                    score+=.2*max(0,forward_reach-palm_front)**2
                if winner is None or score<winner['score']:
                    winner=dict(digit=digit,baseLean=lean,convergence=convergence,closure=float(closure),
                                gaps=gaps,score=score,forwardReach=forward_reach,palmFront=palm_front)
    best.append(winner)
    print(json.dumps(winner),flush=True)
args.out.write_text(json.dumps(dict(scope='Experimental signed palm-normal sampled tip clearance; not full collision or native acceptance.',
                                   side=args.side,dynamicPalm=args.dynamic_palm,wideSearch=args.wide,compactObjective=args.compact,best=best),indent=2)+'\n')
