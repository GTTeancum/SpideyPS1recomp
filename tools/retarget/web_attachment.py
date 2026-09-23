"""Shared-core web socket bindings and native-oracle capture reader (offline tools)."""
import ctypes as C
import struct
from pathlib import Path
import numpy as np

FP=C.POINTER(C.c_float); IP=C.POINTER(C.c_int32); SP=C.POINTER(C.c_int16)
def ptr(array, kind=FP):
    return array.ctypes.data_as(kind)

def bind(lib):
    lib.rtg_pose.argtypes=[C.c_void_p,C.c_uint32,FP,C.c_uint32,FP,C.c_uint32]
    lib.rtg_pose.restype=C.c_int
    lib.rtg_web_target.argtypes=[C.c_void_p,C.c_uint32,FP,C.c_uint32,C.c_uint32,SP,IP,IP,C.c_uint32,IP]
    lib.rtg_web_target.restype=C.c_int
    lib.rtg_web_segment.argtypes=[IP,IP,IP,IP,C.c_uint32,C.c_uint32,IP]
    lib.rtg_web_segment.restype=C.c_int
    return lib

def pose(rig,drivers):
    bind(rig.lib); d=np.ascontiguousarray(drivers,dtype=np.float32); out=np.empty((rig.bones,3,4),np.float32)
    err=rig.lib.rtg_pose(rig.buf,len(rig.blob),ptr(d),0,ptr(out),out.size)
    if err: raise ValueError(f'rtg_pose: {err}')
    return out

def wrist(rig,bones,part,rotation=None,translation=None,position=None,mirror=False):
    bind(rig.lib); b=np.ascontiguousarray(bones,np.float32)
    r=np.ascontiguousarray(np.eye(3)*4096 if rotation is None else rotation,np.int16)
    t=np.ascontiguousarray([0]*3 if translation is None else translation,np.int32)
    p=np.ascontiguousarray([0]*3 if position is None else position,np.int32); out=np.empty(3,np.int32)
    err=rig.lib.rtg_web_target(rig.buf,len(rig.blob),ptr(b),b.size,part,ptr(r,SP),ptr(t,IP),ptr(p,IP),int(mirror),ptr(out,IP))
    if err: raise ValueError(f'rtg_web_target: {err}')
    return out

def warp(lib,start,end,tip,target,index,count):
    bind(lib); vals=[np.ascontiguousarray(x,np.int32) for x in [start,end,tip,target]];out=np.empty(6,np.int32)
    err=lib.rtg_web_segment(*(ptr(v,IP) for v in vals),index,count,ptr(out,IP))
    if err: raise ValueError(f'rtg_web_segment: {err}')
    return out

def read_capture(path):
    data=Path(path).read_bytes()
    if data[:4]!=b'WCP3' or len(data)<12: raise ValueError('Not an offline web-oracle capture')
    records,n=struct.unpack_from('<II',data,4)
    if not 0<n<=4096 or len(data)!=12+records*(24+48*n): raise ValueError('Truncated/invalid capture')
    result=[];p=12
    for _ in range(records):
        clip,frame,part=struct.unpack_from('<III',data,p);p+=12
        target=np.frombuffer(data,dtype='<i4',count=3,offset=p).copy();p+=12
        before=np.frombuffer(data,dtype='<i4',count=n*6,offset=p).reshape(n,6).copy();p+=n*24
        after=np.frombuffer(data,dtype='<i4',count=n*6,offset=p).reshape(n,6).copy();p+=n*24
        result.append(dict(clip=clip,frame=frame,part=part,target=target,before=before,after=after))
    return result
