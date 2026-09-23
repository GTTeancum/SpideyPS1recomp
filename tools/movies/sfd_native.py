"""Developer/test C-ABI binding. The game uses NativeSfd.cs, never Python."""
from pathlib import Path
import ctypes as C,os

HERE=Path(__file__).resolve().parent
class Info(C.Structure):
 _fields_=[(x,C.c_uint32) for x in ['width','height','fps_num','fps_den','sar_num','sar_den','sample_rate','channels','audio_frames','reserved']]
ReadAt=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint64,C.c_void_p,C.c_uint32)
def load(path=None):
 path=path or HERE/'native/bin'/('OpenSpideySfd.dll' if os.name=='nt' else 'libOpenSpideySfd.so')
 lib=C.CDLL(str(path));lib.sfd_abi.restype=C.c_uint32;lib.sfd_workspace_size.restype=C.c_uint32
 lib.sfd_open.argtypes=[C.c_void_p,C.c_uint32,ReadAt,C.c_void_p,C.c_uint64,C.POINTER(Info)]
 lib.sfd_video.argtypes=[C.c_void_p,C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)]
 lib.sfd_audio.argtypes=[C.c_void_p,C.c_void_p,C.c_uint32]
 lib.sfd_rgba.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_uint32,C.c_void_p,C.c_uint32]
 lib.sfd_close.argtypes=[C.c_void_p];lib.sfd_close.restype=None;lib.sfd_error.argtypes=[C.c_void_p]
 if lib.sfd_abi()!=0x10000:raise ValueError('SFD ABI mismatch')
 return lib
class Decoder:
 def __init__(self,path,*,library=None,max_read=None,fail_at=None):
  self.lib=load(library);self.file=open(path,'rb',buffering=0);self.reads=0;self.bytes=0;self.closed=False;self.error=None
  self.max_read=max_read;self.fail_at=fail_at;self.video_count=0;self.audio_count=0
  def read(_,off,dest,n):
   try:
    if self.fail_at is not None and off>=self.fail_at:return -1
    if self.max_read is not None:n=min(n,self.max_read)
    if hasattr(os,'pread'):b=os.pread(self.file.fileno(),n,off)
    else:self.file.seek(off);b=self.file.read(n)
    self.reads+=1;self.bytes+=len(b);C.memmove(dest,b,len(b));return len(b)
   except Exception as e:self.error=str(e);return -1
  self.callback=ReadAt(read);self.workspace_bytes=self.lib.sfd_workspace_size()
  self.storage=C.create_string_buffer(self.workspace_bytes+15);self.ptr=(C.addressof(self.storage)+15)&~15;self.metadata=Info()
  rc=self.lib.sfd_open(self.ptr,self.workspace_bytes,self.callback,None,os.fstat(self.file.fileno()).st_size,C.byref(self.metadata))
  if rc:self.close();raise ValueError(f'SFD open: {rc}')
  self.info={x:getattr(self.metadata,x) for x,_ in Info._fields_};self.info['audio_duration']=self.info['audio_frames']/self.info['sample_rate']
  self.yuv=C.create_string_buffer(self.info['width']*self.info['height']*3//2);self.index=C.c_uint32()
 def video(self):
  rc=self.lib.sfd_video(self.ptr,self.yuv,len(self.yuv),C.byref(self.index))
  if rc<0:raise ValueError(f'SFD video: {rc}')
  if rc==0:return None
  if self.index.value!=self.video_count:raise AssertionError('native frame order')
  self.video_count+=1;return self.yuv.raw
 def rgba(self):
  if not self.video_count:raise ValueError('decode a frame first')
  out=C.create_string_buffer(self.info['width']*self.info['height']*4)
  rc=self.lib.sfd_rgba(self.yuv,len(self.yuv),self.info['width'],self.info['height'],out,len(out))
  if rc:raise ValueError(f'SFD color: {rc}')
  return out.raw
 def audio(self,frames=2048):
  if frames<=0 or frames>65536:raise ValueError('audio request bound')
  pcm=(C.c_int16*(frames*2))();rc=self.lib.sfd_audio(self.ptr,pcm,frames)
  if rc<0:raise ValueError(f'SFD audio: {rc}')
  self.audio_count+=rc
  return C.string_at(pcm,rc*4) if rc else None
 def close(self):
  if not self.closed:self.closed=True;self.lib.sfd_close(self.ptr);self.file.close()
 def __enter__(self):return self
 def __exit__(self,*args):self.close()
