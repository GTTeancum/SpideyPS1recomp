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
p.add_argument('--closure',type=float,default=0)
p.add_argument('--side-view',action='store_true')
p.add_argument('--base-lean',type=float,default=0)
p.add_argument('--contact-parameters',type=Path)
args=p.parse_args()
contact=json.loads(args.contact_parameters.read_text())['best'] if args.contact_parameters else []
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
if args.side_view:basis=np.column_stack([forward,-inward,across])
if args.aim_fingers:
    blob=bytearray(rig.blob)
    for digit in (2,3,5):
        settings=next((r for r in contact if r['digit']==digit),{})
        lean=settings.get('baseLean',args.base_lean)
        convergence=settings.get('convergence',args.converge)
        closure=settings.get('closure',args.closure)
        first=names.index(f'Clown001LArmDigit{digit}1')
        second=names.index(f'Clown001LArmDigit{digit}2')
        r1,r2=opened[first,:,:3],opened[second,:,:3]
        q1=align(opened[second,:,3]-opened[first,:,3],inward-forward*lean)
        lateral=float((opened[names.index('Clown001LArmDigit31'),:,3]-opened[first,:,3])@across)
        length=float(np.linalg.norm(opened[second,:,3]-opened[first,:,3]))
        q2=align(opened[second,:,1],-forward+across*(lateral/length)*convergence-inward*closure)
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
def surface_distance(point,faces):
    a,b,c=faces[:,0],faces[:,1],faces[:,2]
    ab,ac=b-a,c-a
    n=np.cross(ab,ac)
    nn=np.sum(n*n,axis=1)
    projection=point-n*(np.sum((point-a)*n,axis=1)/np.maximum(nn,1e-20))[:,None]
    ap=projection-a
    d00=np.sum(ab*ab,axis=1);d01=np.sum(ab*ac,axis=1);d11=np.sum(ac*ac,axis=1)
    d20=np.sum(ap*ab,axis=1);d21=np.sum(ap*ac,axis=1)
    denominator=d00*d11-d01*d01
    u=(d11*d20-d01*d21)/np.maximum(denominator,1e-20)
    v=(d00*d21-d01*d20)/np.maximum(denominator,1e-20)
    valid=(nn>1e-12)&(u>=0)&(v>=0)&(u+v<=1)
    distances=np.where(valid,np.linalg.norm(point-projection,axis=1),np.inf)
    for x,y in ((a,b),(b,c),(c,a)):
        edge=y-x
        t=np.clip(np.sum((point-x)*edge,axis=1)/np.maximum(np.sum(edge*edge,axis=1),1e-20),0,1)
        distances=np.minimum(distances,np.linalg.norm(point-(x+t[:,None]*edge),axis=1))
    return float(distances.min())

fixture=np.array([[[0.,0,0],[1.,0,0],[0.,1,0]]])
assert abs(surface_distance(np.array([.2,.2,2]),fixture)-2)<1e-9
assert abs(surface_distance(np.array([2.,0,0]),fixture)-1)<1e-9
world,_=rig.evaluate()
open_vertices,_=rig.evaluate(flags=1)
palm_faces=np.array([t['p'] for t in alltri if any(int(v) in selected for v in t['vertex_ids'])
                     and int(np.argmax(np.sum([vertexweights[int(v)] for v in t['vertex_ids']],axis=0)))==0])
clearance=[]
ray_basis=np.column_stack([forward,unit(np.cross(inward,forward)),inward])
def palm_ray_clearance(point,faces=palm_faces,frame=ray_basis):
    q=point@frame
    ray_faces=faces@frame
    a,b,c=ray_faces[:,0],ray_faces[:,1],ray_faces[:,2]
    ab,ac=b[:,:2]-a[:,:2],c[:,:2]-a[:,:2]
    ap=q[:2]-a[:,:2]
    den=ab[:,0]*ac[:,1]-ab[:,1]*ac[:,0]
    safe=np.where(abs(den)>1e-9,den,1)
    u=(ap[:,0]*ac[:,1]-ap[:,1]*ac[:,0])/safe
    v=(ab[:,0]*ap[:,1]-ab[:,1]*ap[:,0])/safe
    valid=(abs(den)>1e-9)&(u>=0)&(v>=0)&(u+v<=1)
    if not valid.any():return None
    height=a[:,2]+u*(b[:,2]-a[:,2])+v*(c[:,2]-a[:,2])
    return float(q[2]-height[valid].max())
ray_fixture=fixture+np.array([0,0,1])
assert abs(palm_ray_clearance(np.array([.2,.2,3]),ray_fixture,np.eye(3))-2)<1e-9
assert abs(palm_ray_clearance(np.array([.2,.2,0]),ray_fixture,np.eye(3))+1)<1e-9
assert palm_ray_clearance(np.array([2.,2,3]),ray_fixture,np.eye(3)) is None
for digit in (2,3,5):
    bone=names.index(f'Clown001LArmDigit{digit}2')
    ids=[]
    for i in selected:
        first,count=struct.unpack_from('<II',rig.blob,rig.h[8]+i*32+24)
        weights=[struct.unpack_from('<If',rig.blob,rig.h[9]+(first+j)*8) for j in range(count)]
        if sum(w for bi,w in weights if bi==bone)>.5:ids.append(i)
    ids=np.array(ids)
    distances=(open_vertices[ids,:3]-opened[bone,:,3])@opened[bone,:,1]
    tips=ids[distances>=np.quantile(distances,.9)]
    gaps=[surface_distance(world[i,:3],palm_faces) for i in tips]
    clearance.append(dict(digit=digit,tipVertices=tips.tolist(),minimum=min(gaps),maximum=max(gaps),mean=float(np.mean(gaps)),
                          signedPalmNormalClearance=[palm_ray_clearance(world[i,:3]) for i in tips]))
(args.out/'trace.json').write_text(json.dumps(dict(faceLabels=counts,bones=records,
    parameters=dict(aim=args.aim_fingers,converge=args.converge,closure=args.closure,baseLean=args.base_lean,sideView=args.side_view),
    tipSurfaceDistances=clearance,contactParameters=contact,distanceFixtureChecks=5,
    scope='Offline source-weight labels. Signed palm-normal clearance is height above the upper palm/wrist envelope; negative means below that surface and null means no projected palm coverage. This sampled tip check and unsigned surface distance are not full volumetric or finger/finger collision tests. No asset edits.'),indent=2)+'\n')
print(json.dumps(dict(tipSurfaceDistances=clearance),indent=2))
print(json.dumps(dict(faceLabels=counts,bones=records),indent=2))
