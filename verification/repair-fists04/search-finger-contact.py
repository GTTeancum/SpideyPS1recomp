"""Offline parameter search against a palm surface, never an installation gate."""
import contextlib
import io
import json
from pathlib import Path
import runpy
import sys

import numpy as np

work=Path(__file__).resolve().parent
sys.argv=['trace-hand-digits.py','--actor',str(work/'fist-repair-batch04/suits/smu-arana-gymnast/actor.psx'),
          '--out',str(work/'arana-contact-search-reference')]
with contextlib.redirect_stdout(io.StringIO()):
    g=runpy.run_path(str(work/'trace-hand-digits.py'))
rig,opened,names=g['rig'],g['opened'],g['names']
align,Rig=g['align'],g['Rig']
forward,across,inward=g['forward'],g['across'],g['inward']
surface=g['palm_ray_clearance']
best=[]
for row in g['clearance']:
    digit=row['digit']
    i=names.index(f'Clown001LArmDigit{digit}1');j=names.index(f'Clown001LArmDigit{digit}2')
    r1,r2=opened[i,:,:3],opened[j,:,:3]
    length=np.linalg.norm(opened[j,:,3]-opened[i,:,3])
    lateral=(opened[names.index('Clown001LArmDigit31'),:,3]-opened[i,:,3])@across
    winner=None
    for lean in (0,.2,.4,.6,.8,1.):
        q1=align(opened[j,:,3]-opened[i,:,3],inward-forward*lean)
        for convergence in (.35,.7,1.):
            for closure in np.arange(.1,3.01,.1):
                q2=align(opened[j,:,1],-forward+across*(lateral/length)*convergence-inward*closure)
                blob=bytearray(rig.blob)
                for bone,rotation in ((i,r1.T@q1@r1),(j,r2.T@q1.T@q2@r2)):
                    offset=rig.h[7]+bone*196+160
                    blob[offset:offset+36]=np.asarray(rotation,dtype='<f4').tobytes()
                v,_=Rig(blob).evaluate()
                gaps=[surface(v[t,:3]) for t in row['tipVertices']]
                if any(gap is None or gap<2 for gap in gaps):continue
                score=float(np.mean((np.array(gaps)-6)**2))
                if winner is None or score<winner['score']:
                    winner=dict(digit=digit,baseLean=lean,convergence=convergence,closure=float(closure),
                                gaps=gaps,score=score)
    best.append(winner)
    print(json.dumps(winner),flush=True)
(work/'arana-contact-search.json').write_text(json.dumps(dict(scope='Experimental unsigned-source and signed palm-normal tip clearance; static reference palm. Not full collision or native acceptance.',best=best),indent=2)+'\n')
