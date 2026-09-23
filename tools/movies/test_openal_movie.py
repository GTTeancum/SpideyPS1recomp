#!/usr/bin/env python3
"""Execute the movie queue protocol against the supplied real OpenAL Soft library.
This is an offline native-library fixture, NOT execution of the C# game/player.
No sound hardware is required: OpenAL Soft renders into an output buffer.
"""
from __future__ import annotations
import argparse, ctypes as C, hashlib, json, sys
from collections import deque
from pathlib import Path
from sfd_native import Decoder

class Al:
    def __init__(self,path):
        self.lib=C.CDLL(str(path))
        def fn(name,restype,args):
            f=getattr(self.lib,name);f.restype=restype;f.argtypes=args;return f
        P=C.c_void_p; I=C.c_int; U=C.c_uint
        self.proc=fn('alcGetProcAddress',P,[P,C.c_char_p])
        self.open=C.CFUNCTYPE(P,C.c_char_p)(self.proc(None,b'alcLoopbackOpenDeviceSOFT'))
        self.render=C.CFUNCTYPE(None,P,P,I)(self.proc(None,b'alcRenderSamplesSOFT'))
        self.create=fn('alcCreateContext',P,[P,C.POINTER(I)])
        self.current=fn('alcMakeContextCurrent',C.c_bool,[P])
        self.destroy=fn('alcDestroyContext',None,[P]);self.close=fn('alcCloseDevice',C.c_bool,[P])
        self.genSource=fn('alGenSources',None,[I,C.POINTER(U)]);self.delSource=fn('alDeleteSources',None,[I,C.POINTER(U)])
        self.genBuffers=fn('alGenBuffers',None,[I,C.POINTER(U)]);self.delBuffers=fn('alDeleteBuffers',None,[I,C.POINTER(U)])
        self.data=fn('alBufferData',None,[U,I,P,I,I]);self.queue=fn('alSourceQueueBuffers',None,[U,I,C.POINTER(U)])
        self.unqueue=fn('alSourceUnqueueBuffers',None,[U,I,C.POINTER(U)])
        self.play=fn('alSourcePlay',None,[U]);self.stop=fn('alSourceStop',None,[U]);self.seti=fn('alSourcei',None,[U,I,I])
        self.geti=fn('alGetSourcei',None,[U,I,C.POINTER(I)]);self.error=fn('alGetError',I,[])
        self.device=self.open(None)
        if not self.device: raise RuntimeError('OpenAL Soft loopback unavailable')
        attrs=(I*9)(0x1007,44100,0x1990,0x1501,0x1991,0x1406,0,0,0) # stereo float output
        self.context=self.create(self.device,attrs)
        if not self.context or not self.current(self.context): raise RuntimeError('cannot create loopback context')
        self.buffer=(C.c_float*(441*2))()
    def check(self):
        e=self.error()
        if e: raise RuntimeError(f'OpenAL error 0x{e:x}')
    def query(self,src,p):
        v=C.c_int();self.geti(src,p,C.byref(v));self.check();return v.value
    def new_source(self):
        s=C.c_uint();self.genSource(1,C.byref(s));self.check();return s.value
    def new_buffers(self,n):
        a=(C.c_uint*n)();self.genBuffers(n,a);self.check();return list(a)
    def attach(self,src,b,data,rate):
        pcm=C.create_string_buffer(data);self.data(b,0x1103,pcm,len(data),rate)
        i=C.c_uint(b);self.queue(src,1,C.byref(i));self.check()
    def tick(self):
        self.render(self.device,self.buffer,441);self.check();return bytes(self.buffer)
    def clean_source(self,src,buffers):
        self.stop(src);s=C.c_uint(src);self.delSource(1,C.byref(s));a=(C.c_uint*len(buffers))(*buffers);self.delBuffers(len(a),a);self.check()
    def shutdown(self):
        self.current(None);self.destroy(self.context);self.close(self.device)

def scenario(al,path,underrun=False):
    with Decoder(path) as decoder:
        info=decoder.info;rate=info['sample_rate'];total=info['audio_frames']
        src=al.new_source();ids=al.new_buffers(4);pending=deque();completed=submitted=0
        positions=[];hashout=hashlib.sha256();peak=0.;ticks=0;caught=False
        def queue(b):
            nonlocal submitted
            count=min(2048,total-submitted)
            if count:
                data=decoder.audio(count)
                if data is None or len(data)!=count*4: raise ValueError('PCM truncated')
                al.attach(src,b,data,rate);pending.append((b,count));submitted+=count
        try:
            for b in ids:queue(b)
            al.play(src);al.check()
            while completed<total:
                # An injected host stall exceeds the bounded queue. The shipping
                # policy must reject rather than restart consumed samples.
                for _ in range(50 if underrun and ticks==0 else 1):
                    data=al.tick();hashout.update(data);peak=max(peak,max(abs(v) for v in al.buffer));ticks+=1
                processed=al.query(src,0x1016);refill=[]
                assert 0<=processed<=len(pending)
                for _ in range(processed):
                    b=C.c_uint();al.unqueue(src,1,C.byref(b));old,count=pending.popleft();assert b.value==old
                    completed+=count;refill.append(old)
                state=al.query(src,0x1010)
                if state==0x1014 and submitted<total:
                    if not underrun: raise AssertionError('unexpected underrun')
                    caught=True;break
                for b in refill:queue(b)
                if completed==total and not pending:break
                offset=al.query(src,0x1025)
                assert 0<=offset<=submitted-completed
                position=(completed+offset)/rate
                assert not positions or position>=positions[-1]
                # Loopback output and device-derived source clock must agree to
                # within two source samples, including 22050 -> 44100 playback.
                assert abs(position-ticks*.01)<=2/rate
                positions.append(position)
                if ticks>int(info['audio_duration']*100)+100:raise AssertionError('queue never ended')
            if underrun:assert caught
            else:
                assert completed==submitted==total and not pending
                assert peak>0.001
            return dict(file=path.name,rate=rate,source_frames=total,completed=completed,submitted=submitted,
                        queued_frames_remaining=sum(n for _,n in pending),clock_samples=len(positions),
                        output_peak=peak,output_float_sha256=hashout.hexdigest(),injected_underrun=underrun,
                        underrun_rejected=caught,passed=True)
        finally:al.clean_source(src,ids)

def spu_transition(al):
    src=al.new_source();ids=al.new_buffers(8)
    try:
        # Old nonzero PCM must not survive Stop + detach. New playback starts
        # with freshly refilled silent data, not a Play on the old queue.
        for b in ids:al.attach(src,b,b'\0\x40\0\x40'*256,44100)
        al.play(src);al.tick();al.stop(src);al.seti(src,0x1009,0);al.check()
        assert al.query(src,0x1015)==0
        assert al.query(src,0x1010)==0x1014
        for b in ids:al.attach(src,b,b'\0'*256*4,44100)
        al.play(src)
        # OpenAL may ramp the stopped source's last sample briefly. After the
        # documented mixer transition, no old queued audio remains.
        for _ in range(3):al.tick()
        assert max(abs(x) for x in al.buffer)<1e-6
        return {'old_queue_detached':True,'fresh_queue_started':True,'old_pcm_after_settle':0,'passed':True}
    finally:al.clean_source(src,ids)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--library',type=Path,required=True);ap.add_argument('--movies',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
    result={'scope':'actual OpenAL Soft library + Python queue fixture; NOT C# or gameplay','library_sha256':hashlib.sha256(a.library.read_bytes()).hexdigest(),'cases':[]}
    al=Al(a.library)
    try:
        for name in ['ATVILOGO.SFD','L5M2.SFD']:result['cases'].append(scenario(al,a.movies/name))
        result['cases'].append(scenario(al,a.movies/'ATVILOGO.SFD',True))
        result['spu_transition']=spu_transition(al)
    finally:al.shutdown()
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
