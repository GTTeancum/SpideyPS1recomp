"""RTG2 native metadata serializer and binding to the actual runtime skinning core."""
from __future__ import annotations
import ctypes as C, json, struct, sys
from pathlib import Path
import numpy as np
MAGIC=0x32475452

def unique_vertices(scene,cal):
    # Split only at source normal discontinuities. UVs remain per-face in native format.
    lookup={};vertices=[];controls=[];faces=[]
    for fi,face in enumerate(scene.faces):
        tri=[]
        for ci,v in enumerate(face):
            normal=cal['normals'][fi,ci];key=(int(v),tuple(np.round(normal,5)))
            if key not in lookup:
                lookup[key]=len(vertices);vertices.append((*cal['vertices'][v],*normal));controls.append(int(v))
            tri.append(lookup[key])
        faces.append(tri)
    return np.array(vertices,float),controls,np.array(faces,np.uint32)

def packetize(faces,controls,scene,cal):
    # Native meshes are transport packets; source skin influences remain untouched.
    # Try anatomical ownership first, then spill oversized meshes into capacity in
    # other always-visible packets. Alternate hands remain exact duplicate packets.
    owner=[]
    for c in controls:
        bi=max(scene.weights[c],key=lambda w:w[1])[0]
        while cal['drivers'][bi]<0 and scene.parents[bi]>=0:bi=scene.parents[bi]
        owner.append(max(0,int(cal['drivers'][bi])))
    bins=[[] for _ in range(18)];ids=[set() for _ in range(18)];overflow=[]
    for fi,f in enumerate(faces):
        scores={}
        for v in f:scores[owner[v]]=scores.get(owner[v],0)+1
        part=max(scores,key=scores.get)
        if len(ids[part]|set(f))<=256:bins[part].append(fi);ids[part].update(f)
        else:overflow.append(fi)
    # Do not put spillover into hands: their native variant switching is meaningful.
    candidates=[0,1,2,3,4,7,8,9,12,13,14,15,16,17]
    for fi in overflow:
        f=faces[fi];opts=[i for i in candidates if len(ids[i]|set(f))<=256]
        if not opts:return compact_packets(faces,owner)
        part=min(opts,key=lambda i:(len(set(f)-ids[i]),len(ids[i])))
        bins[part].append(fi);ids[part].update(f)
    for alias,primary in [(6,5),(11,10)]:bins[alias]=bins[primary].copy();ids[alias]=ids[primary].copy()
    return [sorted(v) for v in ids],bins,len(overflow)

def compact_packets(faces,owner):
    """Connected meshlets reduce duplicated boundary vertices without changing faces."""
    bins=[[] for _ in range(18)];ids=[set() for _ in range(18)]
    pending=set();triangles=[set(map(int,f)) for f in faces]
    adjacency={}
    for fi,f in enumerate(faces):
        scores={}
        for v in f:scores[owner[v]]=scores.get(owner[v],0)+1
        part=max(scores,key=scores.get)
        if part in (5,10) and len(ids[part]|triangles[fi])<=256:
            bins[part].append(fi);ids[part].update(triangles[fi])
        else:pending.add(fi)
        for v in f:adjacency.setdefault(int(v),set()).add(fi)
    for part in [0,1,2,3,4,7,8,9,12,13,14,15,16,17]:
        frontier=set()
        while pending:
            options=[fi for fi in frontier if len(ids[part]|triangles[fi])<=256]
            if not options:
                options=[fi for fi in pending if len(ids[part]|triangles[fi])<=256]
            if not options:break
            fi=min(options,key=lambda i:(len(triangles[i]-ids[part]),i))
            pending.remove(fi);bins[part].append(fi);ids[part].update(triangles[fi])
            for v in triangles[fi]:frontier.update(adjacency[v]&pending)
            frontier.discard(fi)
    if pending:
        raise ValueError(f'Full-quality mesh exceeds native 18-packet capacity ({len(pending)} triangles unplaced); refused to decimate or drop triangles')
    for alias,primary in [(6,5),(11,10)]:bins[alias]=bins[primary].copy();ids[alias]=ids[primary].copy()
    return [sorted(v) for v in ids],bins,len(faces)

def make_blob(scene,cal,vertices,controls,faces,packets):
    d=bytearray(280);bo=len(d)
    for i in range(len(scene.names)):
        d.extend(struct.pack('<iiiI',scene.parents[i],int(cal['drivers'][i]),int(i==cal['anchor']),i))
        for mat in [np.linalg.inv(cal['bind'][i])[:3],cal['rest'][i,:3],cal['local'][i,:3],cal['fist'][i]]:
            d.extend(np.asarray(mat,dtype='<f4').tobytes())
    vo=len(d);weights=[]
    for v,ctrl in zip(vertices,controls):
        ws=scene.weights[ctrl];d.extend(struct.pack('<6fII',*v,len(weights),len(ws)));weights.extend(ws)
    wo=len(d)
    for b,w in weights:d.extend(struct.pack('<If',b,w))
    po=len(d);d.extend(bytes(18*8))
    for i,ids in enumerate(packets):
        struct.pack_into('<II',d,po+i*8,len(ids),len(d));d.extend(np.array(ids,dtype='<u4').tobytes())
    to=len(d);d.extend(np.asarray(faces,dtype='<u4').tobytes())
    jo=len(d)
    provenance={'schema':2,'sourceSha256':scene.sha256,'sourceName':scene.path.name,'boneNames':scene.names,'sourceParents':scene.parents,
                'sourceBindMatrices':scene.source_bind.tolist(),'sourceProperties':scene.source_properties,'sourceClusters':scene.clusters,
                'meshBind':scene.mesh_bind.tolist(),'controlPointForVertex':controls,'sourceControlPointCount':len(scene.vertices),
                'rootAnimationTranslationScale':cal['root_scale'],'referenceUnitScale':cal['scale'],'fistPose':cal['finger_report'],'method':'bind-calibrated-global-rotation/local-length-FK/full-weight-LBS'}
    if scene.normal_repairs:provenance['sourceNormalRepairs']=scene.normal_repairs
    if hasattr(scene,'source_meshes'):provenance['sourceMeshes']=scene.source_meshes
    text=json.dumps(provenance,separators=(',',':'),allow_nan=False).encode();d.extend(text)
    while len(d)%4:d.append(0)
    header=[MAGIC,2,len(d),len(scene.names),len(vertices),len(weights),cal['anchor'],bo,vo,wo,po,jo,len(text),to,len(faces),0]
    struct.pack_into('<16I',d,0,*header);struct.pack_into('<f',d,60,cal['root_scale']);d[64:280]=np.array(cal['native_origins'],dtype='<f4').tobytes()
    return bytes(d)

class Rig:
    def __init__(self,blob):
        self.blob=bytes(blob);self.h=struct.unpack_from('<16I',self.blob);self.bones=self.h[3];self.count=self.h[4]
        lib=Path(__file__).resolve().parent/'native'/'bin'/('OpenSpideyRetarget.dll' if sys.platform=='win32' else 'libOpenSpideyRetarget.so')
        self.lib=C.CDLL(str(lib));self.buf=C.create_string_buffer(self.blob)
        fp=C.POINTER(C.c_float)
        self.lib.rtg_validate.argtypes=[C.c_void_p,C.c_uint];self.lib.rtg_validate.restype=C.c_int
        self.lib.rtg_evaluate.argtypes=[C.c_void_p,C.c_uint,fp,C.c_uint,fp,C.c_uint,fp,C.c_uint];self.lib.rtg_evaluate.restype=C.c_int
        self.lib.rtg_to_part.argtypes=[fp,fp,C.c_uint,fp];self.lib.rtg_to_part.restype=C.c_int
        err=self.lib.rtg_validate(self.buf,len(blob))
        if err:raise ValueError('Runtime rig validator rejected blob, code '+str(err))
        self.provenance=json.loads(self.blob[self.h[11]:self.h[11]+self.h[12]])
    def rest_driver(self):
        p=np.tile(np.eye(4,dtype=np.float32)[:3],(18,1,1));p[:,:,3]=np.frombuffer(self.blob,dtype='<f4',count=54,offset=64).reshape(18,3);return p
    def evaluate(self,driver=None,flags=0):
        driver=np.ascontiguousarray(self.rest_driver() if driver is None else driver,dtype=np.float32)
        bones=np.zeros((self.bones,3,4),np.float32);verts=np.zeros((self.count,6),np.float32);ptr=lambda x:x.ctypes.data_as(C.POINTER(C.c_float))
        e=self.lib.rtg_evaluate(self.buf,len(self.blob),ptr(driver),flags,ptr(bones),bones.size,ptr(verts),verts.size)
        if e:raise ValueError('Runtime evaluation failed: '+str(e))
        return verts,bones
    def part_local(self,pose,verts):
        pose=np.ascontiguousarray(pose,np.float32);verts=np.ascontiguousarray(verts,np.float32);out=np.zeros_like(verts);ptr=lambda x:x.ctypes.data_as(C.POINTER(C.c_float))
        e=self.lib.rtg_to_part(ptr(pose),ptr(verts),len(verts),ptr(out))
        if e:raise ValueError('Part inverse failed '+str(e))
        return out
    def packets(self):
        out=[]
        for i in range(18):
            n,p=struct.unpack_from('<II',self.blob,self.h[10]+i*8);out.append(np.frombuffer(self.blob,'<u4',n,p).copy())
        return out
    def faces(self):return np.frombuffer(self.blob,'<u4',self.h[14]*3,self.h[13]).reshape(-1,3).copy()
    def source_vertices(self):
        return np.array([struct.unpack_from('<6f',self.blob,self.h[8]+i*32) for i in range(self.count)])
