"""Lossless source rig extraction and non-destructive retarget calibration."""
from __future__ import annotations
from pathlib import Path
import hashlib, re
import numpy as np
from fbx_binary import Fbx, clean

def unit(v):
    n=np.linalg.norm(v)
    if n<1e-10: raise ValueError('Degenerate axis')
    return v/n

def rotation(m):
    u,s,v=np.linalg.svd(m[:3,:3]);r=u@v
    if np.linalg.det(r)<0: raise ValueError('Reflected bone bind transform is not supported')
    if max(s)/min(s)>1.001: raise ValueError('Nonuniform bone scale/shear requires affine retargeting; rejected, not stripped')
    return r

def align(a,b):
    a,b=unit(a),unit(b);c=float(a@b)
    if c>1-1e-10:return np.eye(3)
    if c<-.999999:
        axis=unit(np.cross(a,np.eye(3)[np.argmin(abs(a))]));return 2*np.outer(axis,axis)-np.eye(3)
    v=np.cross(a,b);k=np.array([[0,-v[2],v[1]],[v[2],0,-v[0]],[-v[1],v[0],0]])
    return np.eye(3)+k+k@k/(1+c)

def axis_angle(axis, degrees):
    a=unit(axis);t=np.deg2rad(degrees);k=np.array([[0,-a[2],a[1]],[a[2],0,-a[0]],[-a[1],a[0],0]])
    return np.eye(3)+np.sin(t)*k+(1-np.cos(t))*k@k

class Scene:
    def __init__(self,path,geometry_index=None):
        self.path=Path(path);self.fbx=Fbx(path);f=self.fbx
        self.sha256=hashlib.sha256(f.data).hexdigest()
        gs=[(k,n) for k,n in f.objects.items() if n.name=='Geometry' and n.props[2]=='Mesh']
        if not gs:raise ValueError('No source meshes')
        if len(gs)>1 and geometry_index is None:
            parts=[Scene(path,i) for i in range(len(gs))]
            self.__dict__.update(parts[0].__dict__)
            self.parts=parts;self.source_meshes=[];self.clusters=[];self.normal_repairs=[]
            faces=[];weights=[];offset=0;face_offset=0
            for part in parts:
                if part.ids!=self.ids or part.parents!=self.parents or not np.array_equal(part.source_bind,self.source_bind):
                    raise ValueError('Multiple meshes use incompatible authoritative rigs')
                self.source_meshes.append(dict(name=clean(part.geometry.props[1]),
                    meshBind=part.mesh_bind.tolist(),vertices=part.vertices.tolist(),
                    clusters=part.clusters,vertexOffset=offset,faceOffset=face_offset))
                faces.extend(part.faces+offset);weights.extend(part.weights)
                for cluster in part.clusters:
                    self.clusters.append(dict(cluster,indices=[i+offset for i in cluster['indices']]))
                self.normal_repairs.extend(dict(r,face=r['face']+face_offset) for r in part.normal_repairs)
                offset+=len(part.vertices);face_offset+=len(part.faces)
            self.vertices=np.concatenate([p.world_vertices for p in parts]);self.world_vertices=self.vertices.copy()
            self.mesh_bind=np.eye(4);self.faces=np.asarray(faces,int);self.weights=weights
            self.weight_sums=np.concatenate([p.weight_sums for p in parts])
            self.uv=np.concatenate([p.uv for p in parts])
            self.world_normals=np.concatenate([p.world_normals for p in parts])
            self.corner_normals=self.world_normals.copy()
            return
        gid,g=gs[geometry_index or 0];self.geometry=g
        modelids=[c[2] for c in f.connections if c[0]=='OO' and c[1]==gid and f.objects.get(c[2],g).name=='Model']
        if len(modelids)!=1:raise ValueError('Missing mesh model connection')
        mesh_models={c[2] for c in f.connections if c[0]=='OO' and c[1] in {gid for gid,_ in gs}}
        mid=modelids[0];models={k:n for k,n in f.objects.items() if n.name=='Model' and k not in mesh_models}
        poses={}
        for n in f.objects.values():
            if n.name=='Pose' and n.props[2]=='BindPose':
                for p in n.all('PoseNode'):
                    poses[p.value('Node')]=np.array(p.value('Matrix'),dtype=float).reshape(4,4).T
        if mid not in poses: raise ValueError('Mesh has no authoritative bind pose')
        self.mesh_bind=poses[mid]
        self.ids=[]
        def visit(k,visiting):
            if k in self.ids:return
            if k in visiting:raise ValueError('Cyclic rig')
            p=f.parent.get(k,0)
            if p in models:visit(p,visiting|{k})
            self.ids.append(k)
        for k in models:visit(k,set())
        self.names=[clean(models[k].props[1]) for k in self.ids]
        self.parents=[self.ids.index(f.parent[k]) if f.parent.get(k) in models else -1 for k in self.ids]
        if len(set(self.names))!=len(self.names):raise ValueError('Ambiguous duplicate bone names')
        self.source_bind=np.array([poses[k] for k in self.ids]) # includes control/null bones
        self.source_properties=[models[k].properties() for k in self.ids]
        for prop in self.source_properties:
            if prop.get('InheritType',[1])[0] not in (0,1):
                raise ValueError('Unsupported FBX transform inheritance; source rig is not simplified')
        if not np.isfinite(self.source_bind).all():raise ValueError('Nonfinite bind pose')
        self.vertices=np.array(g.value('Vertices'),float).reshape(-1,3)
        if len(self.vertices)>100000:raise ValueError('Source vertex count too large')
        self.weights=[[] for _ in self.vertices];self.clusters=[]
        # Mesh -> skin -> clusters -> bones. Never use the last OO connection as a bone parent.
        skins={c[1] for c in f.connections if c[0]=='OO' and c[2]==gid and f.objects[c[1]].name=='Deformer'}
        clusterids={c[1] for c in f.connections if c[0]=='OO' and c[2] in skins}
        for cid in clusterids:
            cl=f.objects[cid]
            if cl.props[2]!='Cluster':raise ValueError('Unsupported deformer type')
            bones=[c[1] for c in f.connections if c[0]=='OO' and c[2]==cid and c[1] in models]
            if len(bones)!=1:raise ValueError('Ambiguous skin cluster')
            bi=self.ids.index(bones[0]);link=np.array(cl.value('TransformLink')).reshape(4,4).T
            transform=np.array(cl.value('Transform')).reshape(4,4).T
            if not np.allclose(link,self.source_bind[bi],atol=2e-4):raise ValueError('Cluster and bind pose disagree')
            # Blender-exported FBX supplies relative Transform, some exporters use mesh-global.
            if np.allclose(link@transform,self.mesh_bind,atol=2e-4): convention='relative'
            elif np.allclose(transform,self.mesh_bind,atol=2e-4): convention='global'
            else:raise ValueError('Unsupported mesh/cluster bind relationship')
            ix=cl.value('Indexes',np.array([],int));ww=cl.value('Weights',np.array([],float))
            if len(ix)!=len(ww):raise ValueError('Skin index/weight count mismatch')
            for v,w in zip(ix,ww):
                if not 0<=v<len(self.vertices) or not np.isfinite(w) or w<0:raise ValueError('Invalid skin influence')
                if w>0:self.weights[int(v)].append((bi,float(w)))
            self.clusters.append({'bone':bi,'convention':convention,'transform':transform.tolist(),'link':link.tolist(),'indices':ix.tolist(),'weights':ww.tolist()})
        self.weight_sums=np.array([sum(w for _,w in x) for x in self.weights])
        if np.any(self.weight_sums<.999) or np.any(self.weight_sums>1.001):raise ValueError('Unweighted vertex or malformed weight sum; weights are not reassigned')
        # Keep every influence. Normalize only floating-point export roundoff, retain exact doubles in provenance.
        self.weights=[[(b,w/self.weight_sums[i]) for b,w in x] for i,x in enumerate(self.weights)]
        indices=g.value('PolygonVertexIndex');norm=g.child('LayerElementNormal');uv=g.child('LayerElementUV')
        if uv is None or norm is None:raise ValueError('Source requires normals and UVs')
        def element(el,values,index_name,corner,vertex,polygon,width):
            mapping=el.value('MappingInformationType');ref=el.value('ReferenceInformationType')
            ix={'ByPolygonVertex':corner,'ByVertice':vertex,'ByVertex':vertex,'ByControlPoint':vertex,'ByPolygon':polygon,'AllSame':0}.get(mapping)
            if ix is None:raise ValueError('Unsupported layer mapping '+mapping)
            if ref=='IndexToDirect':ix=int(el.value(index_name)[ix])
            elif ref!='Direct':raise ValueError('Unsupported layer reference '+ref)
            return np.asarray(el.value(values)).reshape(-1,width)[ix]
        self.faces=[];self.uv=[];self.corner_normals=[];poly=[];pu=[];pn=[];polygon=0
        for c,x in enumerate(indices):
            v=int(x if x>=0 else -x-1)
            if not 0<=v<len(self.vertices):raise ValueError('Invalid polygon index')
            poly.append(v);pu.append(element(uv,'UV','UVIndex',c,v,polygon,2));pn.append(element(norm,'Normals','NormalsIndex',c,v,polygon,3))
            if x<0:
                if len(poly)!=3:raise ValueError('Nontriangulated source: triangulate without changing rig before conversion')
                self.faces.append(poly);self.uv.append(pu);self.corner_normals.append(pn);poly=[];pu=[];pn=[];polygon+=1
        if poly:raise ValueError('Unterminated polygon')
        self.faces=np.array(self.faces,int);self.uv=np.array(self.uv,float);self.corner_normals=np.array(self.corner_normals,float)
        self.world_vertices=(self.mesh_bind@np.c_[self.vertices,np.ones(len(self.vertices))].T).T[:,:3]
        self.world_normals=self.corner_normals@np.linalg.inv(self.mesh_bind[:3,:3])
        lengths=np.linalg.norm(self.world_normals,axis=2)
        self.normal_repairs=[]
        bad=np.argwhere(lengths<1e-12)
        if len(bad):
            triangles=self.world_vertices[self.faces]
            geometric=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
            adjacent=np.zeros_like(self.world_vertices)
            for corner in range(3):np.add.at(adjacent,self.faces[:,corner],geometric)
            for face,corner in bad:
                derived=geometric[face].copy()
                if np.linalg.norm(derived)<1e-12:derived=adjacent[self.faces[face,corner]].copy()
                # Fully degenerate geometry is retained but has no shading normal.
                if np.linalg.norm(derived)<1e-12:derived=np.array([0.,1.,0.])
                derived=unit(derived)
                self.normal_repairs.append(dict(face=int(face),corner=int(corner),
                    original=self.corner_normals[face,corner].tolist(),derivedWorld=derived.tolist()))
                self.world_normals[face,corner]=derived;lengths[face,corner]=1
        self.world_normals/=lengths[:,:,None]
    def index(self,suffix):
        x=[i for i,n in enumerate(self.names) if n=='Clown001'+suffix]
        if len(x)!=1:raise ValueError('Required SMU rig bone missing: '+suffix)
        return x[0]
    def p(self,s):return self.source_bind[self.index(s),:3,3]

def relax_tongue_tip(scene,bind,local,rest,up):
    """Pose-only correction for Poison's rigid, upward-hooked distal tongue."""
    if scene.path.stem!='poison':return []
    joint=scene.names.index('Tongue_c');tip=scene.names.index('Tongue_d')
    if scene.parents[tip]!=joint:raise ValueError('Poison tongue policy requires the authored c/d chain')
    head=scene.index('Head')
    head_up=rest[head,:3,:3]@bind[head,:3,:3].T@up
    direction=rest[tip,:3,3]-rest[joint,:3,3]
    horizontal=direction-head_up*(direction@head_up)
    if direction@head_up<=0:return []
    target=unit(horizontal-head_up*np.linalg.norm(horizontal)*.15)
    correction=align(direction,target)
    rest[joint,:3,:3]=correction@rest[joint,:3,:3]
    changed={joint}
    for i,parent in enumerate(scene.parents):
        if parent in changed:
            rest[i]=rest[parent]@local[i];changed.add(i)
    return [dict(policy='poison-relaxed-distal-tongue-v1',bone=scene.names[joint],
                 child=scene.names[tip],rotation=correction.tolist(),
                 downwardSlope=.15,sourceBindAndWeightsUnchanged=True)]


def calibrate(scene,reference,native_origins,ground):
    """Preserve target lengths; align segment axes, never copy driver shoulder translations.

    All source bones/weights survive. A separate virtual rest is a pose over the
    unchanged source inverse binds. Character size uses ONE reference-unit scale.
    """
    n=np.array(native_origins,float);s=scene;r=reference
    sr=unit(r.p('LArm1')-r.p('RArm1'));su=np.array([0.,1.,0.]);sr=unit(sr-su*(sr@su));sf=np.cross(sr,su)
    tr=unit(n[4]-n[9]);tu=np.array([0.,-1.,0.]);tr=unit(tr-tu*(tr@tu));tf=np.cross(tr,tu)
    basis=np.column_stack([tr,tu,tf])@np.column_stack([sr,su,sf]).T
    scale=abs(n[7,1]-(n[14,1]+n[17,1])/2)/abs(r.p('Head')[1]-(r.p('LLegAnkle')[1]+r.p('RLegAnkle')[1])/2)
    bind=np.tile(np.eye(4),(len(s.names),1,1))
    for i,b in enumerate(s.source_bind):
        bind[i,:3,:3]=basis@rotation(b);bind[i,:3,3]=scale*basis@b[:3,3]
    verts=scale*s.world_vertices@basis.T
    normals=s.world_normals@basis.T
    local=bind.copy()
    for i,p in enumerate(s.parents):
        if p>=0:local[i]=np.linalg.inv(bind[p])@bind[i]
    drivers=np.full(len(bind),-1,np.int32)
    maps={'Pelvis':0,'Spine1':0,'Spine2':2,'Spine3':1,'Head':7,'LArm1':4,'LArm2':3,'LArmPalm':5,'RArm1':9,'RArm2':8,'RArmPalm':10,'LLeg1':16,'LLeg2':15,'LLegAnkle':17,'RLeg1':13,'RLeg2':12,'RLegAnkle':14}
    for bone,driver in maps.items():drivers[s.index(bone)]=driver
    # Bind-axis calibration, including upper/lower arms separately. Clavicle is
    # deliberately not driven by the upper arm: it remains an articulated chest child.
    axes={'Spine1':('Spine2',0,2),'Spine2':('Spine3',2,1),'Spine3':('Ribcage',1,7),
          'LArm1':('LArm2',4,3),'LArm2':('LArmPalm',3,5),'RArm1':('RArm2',9,8),'RArm2':('RArmPalm',8,10),
          'LLeg1':('LLeg2',16,15),'LLeg2':('LLegAnkle',15,17),'RLeg1':('RLeg2',13,12),'RLeg2':('RLegAnkle',12,14)}
    rest=bind.copy()
    for i,p in enumerate(s.parents):
        if p>=0:rest[i]=rest[p]@local[i]
        suffix=s.names[i].removeprefix('Clown001')
        if suffix in axes:
            child,a,b=axes[suffix];q=s.index(child)
            rest[i,:3,:3]=align(bind[q,:3,3]-bind[i,:3,3],n[b]-n[a])@bind[i,:3,:3]
        elif drivers[i]>=0 and suffix not in ('Pelvis','Spine1'):
            # Preserve hand/head/foot rest orientation relative to their calibrated parent.
            pass
    appendage_report=relax_tongue_tip(s,bind,local,rest,tu)
    virtual_local=rest.copy()
    for i,p in enumerate(s.parents):
        if p>=0:virtual_local[i]=np.linalg.inv(rest[p])@rest[i]
    # Find the posed neutral ground from actual weighted geometry, not donor limb lengths.
    skin=rest@np.linalg.inv(bind);posed=np.zeros_like(verts)
    for v,ws in enumerate(s.weights):
        for bi,w in ws:posed[v]+=w*(skin[bi,:3,:3]@verts[v]+skin[bi,:3,3])
    anchor=s.index('Pelvis');shift=np.array([-rest[anchor,0,3],ground-posed[:,1].max(),n[0,2]-rest[anchor,2,3]])
    rest[:,:3,3]+=shift
    for i,p in enumerate(s.parents):
        if p<0:virtual_local[i,:3,3]+=shift
    fist=np.tile(np.eye(3),(len(bind),1,1));finger_report=[]
    # Curl across the palm, not around arbitrary FBX X/Y Euler axes. The palm's
    # rest joint plane provides an invariant curl axis for mirrored/rotated rigs.
    for side in ('L','R'):
        palm=s.index(side+'ArmPalm');index=s.index(side+'ArmDigit21');little=s.index(side+'ArmDigit51')
        middle=s.index(side+'ArmDigit31') if 'Clown001'+side+'ArmDigit31' in s.names else index
        forward=unit(bind[middle,:3,3]-bind[palm,:3,3]);across=unit(bind[little,:3,3]-bind[index,:3,3])
        # Choose inward side consistently using the original palm's local X axis;
        # sample rigs have their flexion toward local +X (verify with visual test).
        inward=unit(np.cross(across,forward))
        if inward@bind[palm,:3,0]<0:inward=-inward
        axis=unit(np.cross(forward,inward))
        for i,name in enumerate(s.names):
            match=re.fullmatch('Clown001'+side+r'ArmDigit([0-9])([1-3])',name)
            if not match:continue
            digit,segment=map(int,match.groups())
            angle=45 if digit==0 and segment==1 else 65 if digit==0 else 60 if segment==1 else 85
            local_axis=bind[i,:3,:3].T@axis
            fist[i]=axis_angle(local_axis,angle)
            finger_report.append({'bone':name,'angleDegrees':angle,'axisLocal':local_axis.tolist()})
        # Thumb opposition is not ordinary finger flexion. Aim the preserved
        # thumb chain across the curled index/middle fingers, keeping both lengths.
        thumb1=s.index(side+'ArmDigit01')
        thumb2=s.index(side+'ArmDigit02') if 'Clown001'+side+'ArmDigit02' in s.names else None
        middle_child=next((i for i,p in enumerate(s.parents) if p==middle),None)
        phalanx=(np.linalg.norm(bind[middle_child,:3,3]-bind[middle,:3,3]) if middle_child is not None
                 else np.linalg.norm(bind[middle,:3,3]-bind[palm,:3,3])*.35)
        target=(bind[index,:3,3]+bind[middle,:3,3])/2+inward*phalanx*.65+forward*phalanx*.15
        b=bind[thumb1,:3,3];c=bind[thumb2,:3,3] if thumb2 is not None else b+bind[thumb1,:3,1]*phalanx
        first=align(c-b,target-b)
        desired=unit(across+forward*.25-inward*.05)
        r1=bind[thumb1,:3,:3]
        fist[thumb1]=r1.T@first@r1
        if thumb2 is not None:
            second=align(bind[thumb2,:3,1],desired);r2=bind[thumb2,:3,:3]
            fist[thumb2]=r2.T@first.T@second@r2
        for report in finger_report:
            if report['bone'] in ([s.names[thumb1]]+([s.names[thumb2]] if thumb2 is not None else [])):
                bi=thumb1 if report['bone']==s.names[thumb1] else thumb2
                report.clear();report.update(bone=s.names[bi],method='opposed-thumb-across-knuckles',localRotation=fist[bi].tolist())

    leg_ratio=(np.linalg.norm(bind[s.index('LLeg2'),:3,3]-bind[s.index('LLeg1'),:3,3])+np.linalg.norm(bind[s.index('LLegAnkle'),:3,3]-bind[s.index('LLeg2'),:3,3]))/(np.linalg.norm(n[15]-n[16])+np.linalg.norm(n[17]-n[15]))
    return dict(root_scale=float(leg_ratio),bind=bind,rest=rest,local=virtual_local,drivers=drivers,parents=np.array(s.parents),fist=fist,vertices=verts,normals=normals,anchor=anchor,scale=scale,native_origins=n,finger_report=finger_report,appendage_report=appendage_report)
