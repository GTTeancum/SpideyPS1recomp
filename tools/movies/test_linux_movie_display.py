#!/usr/bin/env python3
"""Offline native GLFW/OpenGL texture test with actual decoded SFD pixels.
Not the managed game, not a gameplay screenshot. Requires a working X display.
"""
import argparse,ctypes as C,hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from sfd_native import Decoder

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--glfw',type=Path,required=True);ap.add_argument('--movie',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 g=C.CDLL(str(a.glfw));GL=C.CDLL('libGL.so.1')
 def f(lib,name,result,args):
  fn=getattr(lib,name);fn.restype=result;fn.argtypes=args;return fn
 P=C.c_void_p;I=C.c_int;U=C.c_uint;F=C.c_float
 init=f(g,'glfwInit',I,[]);hint=f(g,'glfwWindowHint',None,[I,I]);create=f(g,'glfwCreateWindow',P,[I,I,C.c_char_p,P,P]);current=f(g,'glfwMakeContextCurrent',None,[P]);destroy=f(g,'glfwDestroyWindow',None,[P]);terminate=f(g,'glfwTerminate',None,[])
 if not init():raise RuntimeError('GLFW initialization failed')
 hint(0x00022002,2);hint(0x00022003,1);hint(0x00021010,0) # GL2.1, no multisampling
 window=create(800,600,b'OpenSpidey offline movie transport (NOT gameplay)',None,None)
 if not window:terminate();raise RuntimeError('GLFW window creation failed')
 current(window)
 gen=f(GL,'glGenTextures',None,[I,C.POINTER(U)]);bind=f(GL,'glBindTexture',None,[U,U]);param=f(GL,'glTexParameteri',None,[U,U,I]);image=f(GL,'glTexImage2D',None,[U,I,I,I,I,I,U,U,P]);sub=f(GL,'glTexSubImage2D',None,[U,I,I,I,I,I,U,U,P]);gettex=f(GL,'glGetTexImage',None,[U,I,U,U,P]);error=f(GL,'glGetError',U,[])
 clear=f(GL,'glClear',None,[U]);clearcolor=f(GL,'glClearColor',None,[F,F,F,F]);enable=f(GL,'glEnable',None,[U]);viewport=f(GL,'glViewport',None,[I,I,I,I]);begin=f(GL,'glBegin',None,[U]);end=f(GL,'glEnd',None,[]);uv=f(GL,'glTexCoord2f',None,[F,F]);vertex=f(GL,'glVertex2f',None,[F,F]);finish=f(GL,'glFinish',None,[]);pixels=f(GL,'glReadPixels',None,[I,I,I,I,U,U,P]);info=f(GL,'glGetString',C.c_char_p,[U])
 result={'scope':'offline native GLFW/OpenGL fixture; not C# execution or gameplay','glfw_sha256':hashlib.sha256(a.glfw.read_bytes()).hexdigest(),'renderer':info(0x1F01).decode(),'version':info(0x1F02).decode(),'original_sfd_sha256':hashlib.sha256(a.movie.read_bytes()).hexdigest(),'frames':[]}
 try:
  t=U();gen(1,C.byref(t));bind(0xDE1,t.value);param(0xDE1,0x2801,0x2601);param(0xDE1,0x2800,0x2601)
  with Decoder(a.movie) as decoder:
   w,h=decoder.info['width'],decoder.info['height']
   # Sequential decode from original .SFD. No random cached frame reads.
   for n in range(901):
    if decoder.video() is None: break
    if n not in [0,30,450,900]: continue
    rgba=np.frombuffer(decoder.rgba(),np.uint8).reshape(h,w,4)
    if not result['frames']:image(0xDE1,0,0x1908,w,h,0,0x1908,0x1401,rgba.ctypes.data)
    else:sub(0xDE1,0,0,0,w,h,0x1908,0x1401,rgba.ctypes.data)
    read=np.empty_like(rgba);gettex(0xDE1,0,0x1908,0x1401,read.ctypes.data);assert np.array_equal(read,rgba);assert error()==0
    clearcolor(0,0,0,1);clear(0x4000);viewport(0,0,800,600);enable(0xDE1)
    aspect=w/h*decoder.info['sar_num']/decoder.info['sar_den'];sx=min(1,aspect/(800/600));sy=min(1,(800/600)/aspect)
    begin(7)
    for (tu,tv),(x,z) in zip([(0,1),(1,1),(1,0),(0,0)],[(-sx,-sy),(sx,-sy),(sx,sy),(-sx,sy)]):uv(tu,tv);vertex(x,z)
    end();finish();screen=np.empty((600,800,4),dtype=np.uint8);pixels(0,0,800,600,0x1908,0x1401,screen.ctypes.data);assert error()==0
    result['frames'].append({'index':n,'texture_roundtrip_exact':True,'rgba_sha256':hashlib.sha256(rgba.tobytes()).hexdigest()})
    if n==900:
     im=Image.fromarray(screen[::-1].copy());d=ImageDraw.Draw(im);d.rectangle((0,0,799,31),fill='black');d.text((10,10),'ORIGINAL .SFD -> NATIVE DECODER -> OPENGL | NOT GAMEPLAY',fill='white');im.save(a.out/'Native-SFD-Playback-Proof.png')
  result['all_passed']=True
 finally:destroy(window);terminate()
 (a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
