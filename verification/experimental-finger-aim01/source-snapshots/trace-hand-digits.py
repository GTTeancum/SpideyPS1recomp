"""Offline source-weighted digit labels. Never edits an actor."""
import argparse
import json
from pathlib import Path
import re
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, 'C:/Programming/GitHub/OpenSpideyPS1/tools/retarget')
from inspect_hand_pose import hand_vertices, comparison_framing
from proof_render import load_asset, triangles, render, caption
from scene import unit, align
from rig_blob import Rig

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--actor',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--aim-fingers',action='store_true')
p.add_argument('--converge',type=float,default=0)
args=p.parse_args()
assert not args.out.exists()
parsed,rig=load_asset(args.actor)
names=rig.provenance['boneNames']
palm,selected=hand_vertices(rig,'L')
_,opened=rig.evaluate(flags=1)
origin=opened[palm,:,3]
forward=unit(opened[names.index('Clown001LArmDigit31'),:,3]-origin)
across=unit(opened[names.index('Clown001LArmDigit51'),:,3]-opened[names.index('Clown001LArmDigit21'),:,3])
inward=unit(np.cross(across,forward))
if inward@opened[palm,:,0]<0:inward=-inward
basis=np.column_stack([forward,across,inward])
if args.aim_fingers:
    blob=bytearray(rig.blob)
    for digit in (2,3,5):
        first=names.index(f'Clown001LArmDigit{digit}1')
        second=names.index(f'Clown001LArmDigit{digit}2')
        r1,r2=opened[first,:,:3],opened[second,:,:3]
        q1=align(opened[second,:,3]-opened[first,:,3],inward)
        lateral=float((opened[names.index('Clown001LArmDigit31'),:,3]-opened[first,:,3])@across)
        length=float(np.linalg.norm(opened[second,:,3]-opened[first,:,3]))
        q2=align(opened[second,:,1],-forward+across*(lateral/length)*args.converge)
        for i,rotation in ((first,r1.T@q1@r1),(second,r2.T@q1.T@q2@r2)):
            offset=rig.h[7]+i*196+160
            blob[offset:offset+36]=np.asarray(rotation,dtype='<f4').tobytes()
    rig=Rig(blob)
palette=np.array([[[170,170,170],[240,70,70],[70,215,240],[100,100,240],[220,220,65]]],dtype=np.uint8)
labels=['palm/wrist','thumb 0','index 2','middle 3','little 5']
groups={0:1,2:2,3:3,5:4}
vertexweights=[]
for i in range(rig.count):
    start,count=struct.unpack_from('<II',rig.blob,rig.h[8]+i*32+24)
    weights=np.zeros(5)
    for j in range(count):
        bone,weight=struct.unpack_from('<If',rig.blob,rig.h[9]+(start+j)*8)
        match=re.fullmatch(r'Clown001LArmDigit([0235])[1-3]',names[bone])
        weights[groups[int(match[1])] if match else 0]+=weight
    vertexweights.append(weights)
alltri,closed,_=triangles(parsed,rig,quantize=False)
visible=[]
counts={label:0 for label in labels}
for t in alltri:
    if not any(int(v) in selected for v in t['vertex_ids']):continue
    group=int(np.argmax(np.sum([vertexweights[int(v)] for v in t['vertex_ids']],axis=0)))
    counts[labels[group]]+=1
    visible.append(dict(t,p=(t['p']-origin)@basis,n=t['n']@basis,uv=np.tile([group/4,0],(3,1))))
args.out.mkdir(parents=True)
for yaw in (0,180):
    scale,center=comparison_framing([visible],yaw)
    picture=render(visible,Image.fromarray(palette),width=720,height=520,yaw=yaw,scale=scale,center=center,ground=False)
    caption(picture,args.actor.parent.name+' offline digit trace',
            'EXPERIMENTAL aiming; NOT GAME' if args.aim_fingers else
            'Gray palm; red thumb; cyan index; blue middle; yellow little. Source weights, NOT GAME.').save(args.out/f'digits-{yaw}.png')
records=[]
for i,name in enumerate(names):
    if 'LArmDigit' in name:
        records.append(dict(bone=name,driver=struct.unpack_from('<i',rig.blob,rig.h[7]+i*196+4)[0],
                            parent=names[rig.provenance['sourceParents'][i]],
                            open=((opened[i,:,3]-origin)@basis).tolist(),
                            closed=((closed[i,:,3]-origin)@basis).tolist()))
(args.out/'trace.json').write_text(json.dumps(dict(faceLabels=counts,bones=records,scope='Offline dominant per-face source-weight labels; mixed weights aggregated, no asset edits.'),indent=2)+'\n')
print(json.dumps(dict(faceLabels=counts,bones=records),indent=2))
