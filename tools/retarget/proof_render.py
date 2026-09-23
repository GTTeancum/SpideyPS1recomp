"""Deterministic native-asset + runtime-core renderer. NOT a game screenshot."""
from pathlib import Path
import math,struct
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from rig_blob import Rig,MAGIC
from native_asset import parse,U

def load_asset(path):
    data=Path(path).read_bytes();parsed=parse(data);p=U(data,4)
    while U(data,p)!=0xffffffff:
        tag,size=U(data,p),U(data,p+4)
        if tag==MAGIC:rig=Rig(data[p+8:p+8+size]);return parsed,rig
        p+=size+8
    raise ValueError('Missing actual native rig')

def triangles(parsed,rig,driver=None,flags=0,quantize=True):
    poses=rig.rest_driver() if driver is None else driver
    world,bones=rig.evaluate(poses,flags);packets=rig.packets();result=[];maximum_error=0
    for i,mesh in enumerate(parsed['meshes']):
        if i in (6,11):continue
        ids=packets[i];v=rig.part_local(poses[i],world[ids]);v[:,:3]=np.rint(v[:,:3]) if quantize else v[:,:3]
        p=v[:,:3]@poses[i,:,:3].T+poses[i,:,3]
        if len(ids):maximum_error=max(maximum_error,float(abs(p-world[ids,:3]).max()))
        normals=v[:,3:]@np.linalg.inv(poses[i,:,:3]);normals/=np.maximum(1e-9,np.linalg.norm(normals,axis=1))[:,None]
        for f in mesh['faces']:
            order=[0,2,1];ii=np.array(f['indices'])[order]
            result.append({'p':p[ii],'n':normals[ii],'uv':np.array(f['uv'])[order]/127,'slot':f['slot'],'mesh':i})
    return result,bones,maximum_error

def render(tris,texture,width=620,height=740,yaw=-12,scale=.18,center=None,ground=True):
    a=np.deg2rad(yaw);R=np.array([[np.cos(a),0,np.sin(a)],[0,1,0],[-np.sin(a),0,np.cos(a)]])
    offset=np.array([width/2,height-70-1719*scale]) if center is None else np.array([width/2,height/2])-np.array(center)*scale
    background=np.zeros((height,width,3),np.uint8);background[:]=[23,30,41];zbuf=np.full((height,width),np.inf)
    # Fixed grounded grid and camera, no independent character-height normalization.
    floor=round(1719*scale+offset[1])
    if ground and 0<=floor<height:background[floor:min(height,floor+2)]=[62,75,90]
    tex=np.asarray(texture.convert('RGB'));light=np.array([-.3,-.5,-.8]);light/=np.linalg.norm(light)
    for t in tris:
        p=t['p']@R.T;n=t['n']@R.T;xy=p[:,:2]*scale+offset
        lo=np.maximum(0,np.floor(xy.min(0)).astype(int));hi=np.minimum([width-1,height-1],np.ceil(xy.max(0)).astype(int))
        if np.any(lo>hi):continue
        (x0,y0),(x1,y1),(x2,y2)=xy;det=(y1-y2)*(x0-x2)+(x2-x1)*(y0-y2)
        if abs(det)<1e-8:continue
        X,Y=np.meshgrid(np.arange(lo[0],hi[0]+1)+.5,np.arange(lo[1],hi[1]+1)+.5)
        w0=((y1-y2)*(X-x2)+(x2-x1)*(Y-y2))/det;w1=((y2-y0)*(X-x2)+(x0-x2)*(Y-y2))/det;w2=1-w0-w1
        z=w0*p[0,2]+w1*p[1,2]+w2*p[2,2];region=np.s_[lo[1]:hi[1]+1,lo[0]:hi[0]+1]
        mask=(w0>=-1e-6)&(w1>=-1e-6)&(w2>=-1e-6)&(z<zbuf[region])
        if not mask.any():continue
        uv=w0[:,:,None]*t['uv'][0]+w1[:,:,None]*t['uv'][1]+w2[:,:,None]*t['uv'][2]
        ix=np.clip(np.rint(uv[:,:,0]*(tex.shape[1]-1)).astype(int),0,tex.shape[1]-1);iy=np.clip(np.rint(uv[:,:,1]*(tex.shape[0]-1)).astype(int),0,tex.shape[0]-1)
        normal=w0[:,:,None]*n[0]+w1[:,:,None]*n[1]+w2[:,:,None]*n[2];normal/=np.maximum(1e-8,np.linalg.norm(normal,axis=2))[:,:,None]
        shade=.70+.30*np.maximum(0,normal@light);color=np.clip(tex[iy,ix]*shade[:,:,None],0,255).astype(np.uint8)
        background[region][mask]=color[mask];zbuf[region][mask]=z[mask]
    return Image.fromarray(background)

def font(size):
    p=Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
    return ImageFont.truetype(str(p),size) if p.exists() else ImageFont.load_default()

def caption(image,title,subtitle):
    canvas=Image.new('RGB',(image.width,image.height+75),(15,20,29));canvas.paste(image,(0,75));d=ImageDraw.Draw(canvas)
    d.text((16,12),title,fill=(237,242,250),font=font(22));d.text((16,43),subtitle,fill=(164,186,206),font=font(13));return canvas
