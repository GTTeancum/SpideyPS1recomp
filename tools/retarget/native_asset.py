"""Independent native v4 decoder, retained from the negative audit for cross-checks."""
import struct,hashlib
import numpy as np
def H(b,p):return struct.unpack_from('<H',b,p)[0]
def U(b,p):return struct.unpack_from('<I',b,p)[0]
def sha(b):return hashlib.sha256(b).hexdigest()

def expand_texel(v):
    # Match TextureTile.Expand: only zero is transparent; bit 15 is STP.
    rgb=[(v>>shift)&31 for shift in (0,5,10)]
    return [(x<<3)|(x>>2) for x in rgb]+[0 if v==0 else 128 if v&0x8000 else 255]
def parse(b):
    if (H(b,0),H(b,2))!=(4,2):raise ValueError('v4 PSX required')
    count=U(b,8);origins=np.array([struct.unpack_from('<iii',b,16+36*i) for i in range(count)],dtype=float)/256
    table=12+36*count;mc=U(b,table);ptrs=[U(b,table+4+4*i) for i in range(mc)]
    p=U(b,4);tags={}
    while U(b,p)!=0xffffffff:
        t=U(b,p);n=U(b,p+4);tags[t]=b[p+8:p+8+n];p+=8+n
    p+=4;names=[U(b,p+4*i) for i in range(mc)];p+=4*mc
    nh=U(b,p);p+=4;hashes=[U(b,p+4*i) for i in range(nh)];p+=4*nh
    palettes={}
    for size in (16,256):
        n=U(b,p);p+=4
        for i in range(n):
            key=U(b,p);p+=4;vals=struct.unpack_from('<'+'H'*size,b,p);p+=size*2
            palettes[key]=np.array([expand_texel(v) for v in vals],dtype=np.uint8)
    nt=U(b,p);p+=4
    if nt==0xffffffff:
        for _ in range(2):n=U(b,p);p+=4+36*n
        nt=U(b,p);p+=4
    textures={}
    for i in range(nt):
        off=U(b,p+4*i);flags,pal,slot=U(b,off+4),U(b,off+8),U(b,off+12);w,h=H(b,off+16),H(b,off+18)
        if flags&0x100:idx=np.frombuffer(b,dtype=np.uint8,count=w*h,offset=off+20).copy()
        else:
            packed=np.frombuffer(b,dtype=np.uint8,count=(w*h+1)//2,offset=off+20)
            idx=np.column_stack((packed&15,packed>>4)).reshape(-1)[:w*h]
        textures[slot]=palettes[pal][idx].reshape(h,w,4)
    sources=[];meshes=[];tris=[]
    for i,off in enumerate(ptrs):
        nv,nn,nf=H(b,off+2),H(b,off+4),H(b,off+6);vp=off+28;verts=[];owners=[];source_vertices=[]
        for j in range(nv):
            x,y,z,flag=struct.unpack_from('<hhhH',b,vp+8*j)
            if flag==2:
                ref=H(b,vp+8*j+2)
                if ref>=len(sources):raise ValueError(f'unresolved stitch {i}:{j}')
                pt,owner,sv=sources[ref];pt=pt.copy()
            elif flag in (0,1):
                pt=np.array([x,y,z],dtype=float)+origins[i];owner=i;sv=j
                if flag==1:sources.append((pt.copy(),owner,sv))
            else:raise ValueError(f'unknown vertex type {flag}')
            verts.append(pt);owners.append(owner);source_vertices.append(sv)
        verts=np.array(verts);fp=vp+nv*8+nn*8;faces=[]
        for j in range(nf):
            flags,length=H(b,fp),H(b,fp+2);quad=(flags&16)==0;nc=4 if quad else 3
            ix=list(b[fp+4:fp+4+nc]);uv=np.array(list(b[fp+20:fp+20+nc*2])).reshape(nc,2)
            slot=U(b,fp+16);orders=[(0,2,1),(1,2,3)] if quad else [(0,2,1)]
            for order in orders:
                order=list(order)
                if i not in (6,11):tris.append({'p':verts[np.array(ix)[order]],'uv':uv[order],
                    'slot':slot,'mesh':i,'face':j,'textured':bool(flags&1)})
            faces.append({'indices':ix,'uv':uv.tolist(),'slot':slot,'flags':flags})
            fp+=length
        meshes.append({'positions':verts,'owners':owners,'source_vertices':source_vertices,'faces':faces})
    parents=list(struct.unpack('<'+'H'*count,tags[0x52454948]))
    return {'origins':origins,'meshes':meshes,'triangles':tris,'textures':textures,'parents':parents,'sha256':sha(b),'names':names}
