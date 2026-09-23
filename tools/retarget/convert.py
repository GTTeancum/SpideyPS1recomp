#!/usr/bin/env python3
"""Convert SMU binary FBX to a native v4 actor with an embedded, preserved RTG2 rig.

Uses the original shared animation bank. Does NOT rewrite the donor skeleton,
transfer template weights, fit every character to donor height, or decimate.
"""
from __future__ import annotations
import argparse, hashlib, json, math, re, struct, sys, zlib
from pathlib import Path
import numpy as np
from PIL import Image
from scene import Scene,calibrate
from rig_blob import unique_vertices,packetize,make_blob,Rig,MAGIC
from materials import bindings

def U(d,p):return struct.unpack_from('<I',d,p)[0]
def H(d,p):return struct.unpack_from('<H',d,p)[0]
def donor_info(d):
    if H(d,0)!=4 or H(d,2)!=2 or U(d,8)!=18 or U(d,660)!=18:raise ValueError('Expected intact v4 18-part animation donor')
    origins=np.array([struct.unpack_from('<iii',d,16+36*i) for i in range(18)])/256
    p=U(d,4);chunks=[]
    while U(d,p)!=0xffffffff:
        size=U(d,p+4)
        if p+8+size>len(d):raise ValueError('Bad donor metadata')
        chunks.append(d[p:p+8+size]);p+=8+size
    names=d[p+4:p+4+72]
    if not any(U(c,0)==0x52454948 for c in chunks):raise ValueError('Missing native hierarchy')
    ground=-float('inf')
    for i in range(18):
        m=U(d,664+4*i)
        for v in range(H(d,m+2)):
            x,y,z,kind=struct.unpack_from('<hhhH',d,m+28+v*8)
            if kind in (0,1):ground=max(ground,y+origins[i,1])
    return origins,ground,chunks,names

def qnormal(v):
    v=np.array(v,float);length=np.linalg.norm(v)
    return np.rint(v*(4096/length)).astype(int).tolist() if length>1e-8 else [0,-4096,0]

def native_mesh(verts,faces,uv,materials=None,alpha=None):
    nv=len(verts);nf=len(faces);head=bytearray(28)
    struct.pack_into('<HHHH',head,0,0,nv,nv+nf,nf)
    radius=math.ceil(max(np.linalg.norm(verts[:,:3],axis=1),default=0))*256;struct.pack_into('<I',head,8,radius)
    for axis in range(3):
        if nv:struct.pack_into('<hh',head,12+axis*4,math.ceil(verts[:,axis].max()/16),math.floor(verts[:,axis].min()/16))
    struct.pack_into('<hH',head,24,32767,0xffff)
    data=head
    for v in verts:
        xyz=np.rint(v[:3]).astype(int)
        if np.any(abs(xyz)>32760):raise ValueError('Native vertex range exceeded')
        data.extend(struct.pack('<hhhH',*xyz,0))
    for v in verts:data.extend(struct.pack('<hhhH',*qnormal(v[3:]),0))
    for tri in faces:
        a,b,c=verts[tri,:3];data.extend(struct.pack('<hhhH',*qnormal(np.cross(b-a,c-a)),0))
    for fi,tri in enumerate(faces):
        face=bytearray(36);struct.pack_into('<HH',face,0,0x1f,36)
        for k,corner in enumerate((0,2,1)):
            face[4+k]=int(tri[corner]);u,v=uv[fi,corner]
            if not -.001<=u<=1.001 or not -.001<=v<=1.001:raise ValueError('UV outside single-atlas range')
            face[20+2*k]=int(np.clip(round(u*127),0,127));face[21+2*k]=int(np.clip(round((1-v)*127),0,127))
        slot=0 if materials is None else int(materials[fi])
        face[8:12]=bytes([210,210,210,38 if alpha and alpha[slot] else 36])
        struct.pack_into('<H',face,12,nv+fi);struct.pack_into('<I',face,16,slot);data.extend(face)
    return data

def convert(fbx,reference,donor,texture,out,suit_id,name):
    for p in [fbx,reference,donor,texture]:
        if not p.is_file():raise ValueError('Missing input: '+str(p))
    if not re.fullmatch('[a-z0-9][a-z0-9-]{0,47}',suit_id):raise ValueError('Unsafe suit id')
    if not 1<=len(name)<=19 or any(ord(c)<32 or ord(c)>126 for c in name):raise ValueError('Suit display name must be 1..19 printable ASCII characters')
    if out.exists() and any(out.iterdir()):raise ValueError('Output directory must be new or empty; original assets will not be overwritten')
    inputs={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in [fbx,reference,donor,texture]}
    s=Scene(fbx);ref=Scene(reference);raw=donor.read_bytes();origins,ground,chunks,names=donor_info(raw)
    material_records,images,face_materials,render_uv=bindings(s,texture)
    for record in material_records:inputs[record['source']]=hashlib.sha256(Path(record['source']).read_bytes()).hexdigest()
    cal=calibrate(s,ref,origins,ground);vertices,controls,faces=unique_vertices(s,cal)
    packets,bins,overflow=packetize(faces,controls,s,cal)
    blob=make_blob(s,cal,vertices,controls,faces,packets);rig=Rig(blob)
    posed,bones=rig.evaluate();driver=rig.rest_driver()
    count=len(packets);mapping=rig.packet_drivers()
    data=bytearray(raw[:660]);data.extend(struct.pack('<I',count));data.extend(bytes(count*4));stats=[]
    for i in range(count):
        struct.pack_into('<I',data,664+4*i,len(data));ids=packets[i];lookup={v:j for j,v in enumerate(ids)}
        local=rig.part_local(driver[mapping[i]],posed[ids]);fs=np.array([[lookup[v] for v in faces[j]] for j in bins[i]],int).reshape(-1,3)
        uvs=render_uv[bins[i]];mesh=native_mesh(local,fs,uvs,face_materials[bins[i]],[r['alphaBound'] for r in material_records])
        if count>18:
            link=18 if i==0 else i+1 if 18<=i<count-1 else 0xffff
            struct.pack_into('<H',mesh,26,link)
        data.extend(mesh)
        stats.append({'packet':i,'vertices':len(ids),'triangles':len(fs)})
    struct.pack_into('<I',data,4,len(data))
    for c in chunks:
        if U(c,0)!=MAGIC:data.extend(c)
    data.extend(struct.pack('<II',MAGIC,len(blob)));data.extend(blob);data.extend(struct.pack('<I',0xffffffff));data.extend(names)
    for i in range(18,count):data.extend(struct.pack('<I',zlib.crc32(f'suit-page:{suit_id}:{i}'.encode())))
    materials=[zlib.crc32(('suit-material:'+suit_id+(':'+str(i) if i else '')).encode()) for i in range(len(images))]
    palettes=[zlib.crc32(('suit-palette:'+suit_id+(':'+str(i) if i else '')).encode()) for i in range(len(images))]
    data.extend(struct.pack('<I',len(materials)));data.extend(struct.pack('<'+'I'*len(materials),*materials));data.extend(struct.pack('<II',0,len(images)))
    texture_rows=[]
    for slot,source_image in enumerate(images):
        resized=source_image.resize((128,128),Image.Resampling.LANCZOS)
        small=resized.convert('RGB').quantize(colors=255 if material_records[slot]['alphaBound'] else 256,method=Image.Quantize.MEDIANCUT)
        rgb=np.array(small.getpalette(),np.uint16).reshape(-1,3)
        pal=np.zeros(256,np.uint16);pal[:len(rgb)]=((rgb[:,0]>>3)|((rgb[:,1]>>3)<<5)|((rgb[:,2]>>3)<<10))
        pal[pal==0]=0x8000;pal[pal==0x7c1f]=0xfc1f
        rows=np.array(small)
        if material_records[slot]['alphaBound']:
            pal[255]=0;rows[np.array(resized.getchannel('A'))<128]=255
        data.extend(struct.pack('<I256H',palettes[slot],*pal));texture_rows.append(rows)
    data.extend(struct.pack('<I',len(images)));table=len(data);data.extend(bytes(4*len(images)))
    for slot,rows in enumerate(texture_rows):
        struct.pack_into('<I',data,table+4*slot,len(data))
        data.extend(struct.pack('<IIIIHH',0,0x100,palettes[slot],slot,128,128));data.extend(rows.tobytes())
    if len(data)>4*1024*1024:raise ValueError(f'Native actor exceeds 4 MiB host bound: {len(data)}')
    if len(data)-len(blob)-8>1024*1024:raise ValueError('Native geometry exceeds 1 MiB guest bound after rig removal')
    if bytes(data[12:660])!=raw[12:660]:raise AssertionError('Object flags/origins changed')
    out.mkdir(parents=True,exist_ok=True);(out/'textures').mkdir()
    (out/'actor.psx').write_bytes(data)
    texture_map={}
    for slot,im in enumerate(images):
        filename='diffuse.png' if slot==0 else f'diffuse-{slot}.png'
        im.save(out/'textures'/filename);texture_map[f'{materials[slot]:08X}']='textures/'+filename
    manifest={'version':1,'id':suit_id,'name':name,'comments':'RIG PRESERVED','model':'spiderman','modelFile':'actor.psx','abilities':{'profile':'spiderman'},'textures':texture_map}
    (out/'suit.json').write_text(json.dumps(manifest,indent=2)+'\n')
    # The external texture is a direct source RGB conversion, NOT an invented upscale.
    report={'schema':2,'status':'converted and core-evaluated; game execution is a separate acceptance gate','sourceRigBones':len(s.names),
            'sourceControlPoints':len(s.vertices),'sourcePositiveInfluences':sum(len(w) for w in s.weights),'runtimeVertices':len(vertices),
            'runtimeInfluences':sum(len(s.weights[c]) for c in controls),'originalTriangles':len(s.faces),'uniqueOutputTriangles':sum(len(bins[i]) for i in range(count) if i not in (6,11)),
            'nativeBytes':len(data),'rigBytes':len(blob),'packetSpilloverTriangles':overflow,'packets':stats,'unitScale':cal['scale'],'rootAnimationScale':cal['root_scale'],
            'neutralBounds':[posed[:,:3].min(0).tolist(),posed[:,:3].max(0).tolist()],'sourceBindReconstructionMaxError':float(np.max(abs(rig.evaluate(flags=3)[0][:,:3]-vertices[:,:3]))),
            'inputHashes':inputs,'outputSha256':hashlib.sha256(data).hexdigest(),'fistBones':cal['finger_report'],'materials':material_records}
    for p,h in inputs.items():
        if hashlib.sha256(Path(p).read_bytes()).hexdigest()!=h:raise AssertionError('An original input changed')
    (out/'conversion-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('packets','inputHashes','fistBones')},indent=2),flush=True)
    return report

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('fbx','reference','donor','texture','out'):ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--id',required=True);ap.add_argument('--name',required=True);a=ap.parse_args()
    try:convert(a.fbx,a.reference,a.donor,a.texture,a.out,a.id,a.name)
    except (ValueError,OSError) as e:ap.exit(1,str(e)+'\n')
if __name__=='__main__':main()
