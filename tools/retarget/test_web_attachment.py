#!/usr/bin/env python3
"""Repair03 core/original-native-line regressions. This does NOT run the .NET game."""
import argparse,ctypes as C,hashlib,json,struct,time
from pathlib import Path
import numpy as np
from proof_render import load_asset,triangles
from animation_bank import read
from web_attachment import bind,pose,wrist,warp,read_capture,ptr,FP,IP,SP

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--project',type=Path,required=True);ap.add_argument('--bank',type=Path,required=True)
    ap.add_argument('--captures',type=Path,required=True);ap.add_argument('--previous-core',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    checks=[];metrics={};start_time=time.time()
    def check(name,result,**detail):
        ok=bool(result);checks.append(dict(name=name,passed=ok,**detail));print(('PASS ' if ok else 'FAIL ')+name,flush=True)
        if not ok:raise AssertionError(name)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    def rnd(x):return np.where(x>=0,np.floor(x+.5),np.ceil(x-.5)).astype(np.int64)
    old=C.CDLL(str(a.previous_core));old.rtg_evaluate.argtypes=[C.c_void_p,C.c_uint32,FP,C.c_uint32,FP,C.c_uint32,FP,C.c_uint32];old.rtg_evaluate.restype=C.c_int
    bank=read(a.bank);check('300 original animation clips / 4196 frames',len(bank)==300 and sum(map(len,bank))==4196)
    for name in ['spiderham','2099']:
        asset=a.project/f'spiderman/port/mods/suits/smu-{name}/actor.psx';before_sha=sha(asset);parsed,rig=load_asset(asset);lib=bind(rig.lib)
        check(name+': coordinated native ABI 0x20002',lib.rtg_abi()==0x20002)
        oldb=np.zeros((rig.bones,3,4),np.float32);oldv=np.zeros((rig.count,6),np.float32);n=0
        for clip in bank:
            for driver in clip:
                err=old.rtg_evaluate(rig.buf,len(rig.blob),ptr(driver),0,ptr(oldb),oldb.size,ptr(oldv),oldv.size)
                v,b=rig.evaluate(driver);p=pose(rig,driver)
                if err or not(np.array_equal(v,oldv) and np.array_equal(b,oldb) and np.array_equal(p,b)):raise AssertionError('Full-rig regression')
                n+=1
        check(name+': full weighted vertices AND bones bit-identical to Repair02',n==4196,frames=n)
        check(name+': socket-only FK bit-identical to skinning FK',n==4196)
        bo=rig.h[7];mapping=[struct.unpack_from('<i',rig.blob,bo+i*196+4)[0] for i in range(rig.bones)]
        bone={p:mapping.index(p) for p in [5,10]};capture=read_capture(a.captures/f'{name}-swing.bin');gaps=[];quant=0
        for rec in capture:
            driver=bank[rec['clip']][rec['frame']];p=pose(rig,driver);w=wrist(rig,p,rec['part']);b=rec['before'];z=rec['after'];q=rec['target']
            assert np.array_equal(w,q) and np.array_equal(z[-1,3:],q)
            assert np.array_equal(z[0,:3],b[0,:3]) and np.array_equal(z[:-1,3:],z[1:,:3])
            # Independent native-helper baseline: zero-offset socket part in original
            # pose, identity actor transform. Native forward setback is Z-only.
            native=rnd(driver[rec['part'],:,3].astype(float)*256);native[2]-=8*4096
            assert np.array_equal(native,b[-1,3:])
            expected=rnd(p[bone[rec['part']],:,3].astype(float)*256)
            assert np.array_equal(expected,q)
            gaps.append(float((int(q[1])-int(b[-1,4]))/4096))
            # Execute signed-16 transport of actual rendered mesh for swing poses.
            _,_,er=triangles(parsed,rig,driver);quant=max(quant,er)
        check(name+': both real swing clips (275/280), 115 frames',len(capture)==115 and {r['clip'] for r in capture}=={275,280})
        check(name+': original native socket agrees with independent original pose math',True)
        check(name+': corrected native final point equals current full-rig wrist',True,maxEndpointErrorQ12=0)
        check(name+': unchanged environment anchor, no inter-segment seams',True,segments=len(capture)*16)
        check(name+': signed16 mesh transport stays within 1 native model unit',quant<1,maxComponentError=quant)
        check(name+': source converted actor bytes unchanged',sha(asset)==before_sha)
        for driver in [bank[275][0],bank[280][-1]]:
            p=pose(rig,driver)
            assert np.array_equal(wrist(rig,p,5),wrist(rig,p,6)) and np.array_equal(wrist(rig,p,10),wrist(rig,p,11))
        check(name+': alternate-hand aliases use the same original wrist',True)
        p=pose(rig,bank[275][25]);transform_cases=0
        for deg in [0,30,90,180,270]:
            t=deg*np.pi/180;rot=np.rint(np.array([[np.cos(t),-np.sin(t),0],[np.sin(t),np.cos(t),0],[0,0,1]])*4096).astype(np.int16)
            for position in [[0,0,0],[123456789,-187654321,198765432],[-123456789,80000999,-30000123]]:
                position=np.array(position,np.int32);trans=(position.astype(np.int64)>>12).astype(np.int32)
                for mirror in [False,True]:
                    for part in [5,10]:
                        v=p[bone[part],:,3].astype(float).copy();v[0]*=-1 if mirror else 1
                        expect=rnd(position.astype(float)+(rot.astype(float)@v+trans.astype(float)*4096-position.astype(float))/16)
                        assert np.array_equal(wrist(rig,p,part,rot,trans,position,mirror),expect);transform_cases+=1
        check(name+': rotation/inversion/mirror/distant-world/fractional-position transform',True,cases=transform_cases)
        R=np.eye(3,dtype=np.int16)*4096;T=np.zeros(3,np.int32);P=T.copy();sentinel=np.array([1234567]*3,np.int32)
        def badtarget(label,blob=None,bones=None,size=None,part=10,mirror=0,r=None,t=None):
            data=rig.blob if blob is None else blob;buf=C.create_string_buffer(data);b=p.copy() if bones is None else bones;out=sentinel.copy()
            rc=lib.rtg_web_target(buf,len(data),ptr(b),b.size if size is None else size,part,ptr(R if r is None else r,SP),ptr(T if t is None else t,IP),ptr(P,IP),mirror,ptr(out,IP))
            check(name+': '+label,rc!=0 and np.array_equal(out,sentinel),error=rc)
        badtarget('non-hand socket rejected atomically',part=0)
        badtarget('undersized pose buffer rejected atomically',size=1)
        badtarget('invalid mirror flag rejected atomically',mirror=2)
        b=p.copy();b[bone[10],1,3]=np.nan;badtarget('nonfinite wrist rejected atomically',bones=b)
        b=p.copy();b[bone[10],1,3]=1e35;badtarget('world overflow rejected atomically',bones=b)
        z=bytearray(rig.blob);struct.pack_into('<i',z,bo+bone[10]*196+4,-1);badtarget('missing wrist mapping rejected atomically',blob=bytes(z))
        z=bytearray(rig.blob);i=next(i for i,v in enumerate(mapping) if v==-1);struct.pack_into('<i',z,bo+i*196+4,10);badtarget('ambiguous wrist mapping rejected atomically',blob=bytes(z))
        badtarget('truncated rig rejected atomically',blob=rig.blob[:200])
        metrics[name]={'assetSha256':before_sha,'fullRegressionFrames':n,'swingFrames':len(capture),'nativeSegments':len(capture)*16,'beforeVerticalGapWorldUnits':{'min':min(gaps),'max':max(gaps),'clip275frame25':gaps[25]},'afterWristEndpointErrorQ12':0,'maxMeshRoundtripComponentErrorModelUnits':quant}
    # Non-straight input tests: the same algorithm handles primary and secondary
    # polylines; it must not accumulate modified scratch between native segments.
    rng=np.random.default_rng(300275);random_segments=0
    for count in [1,2,3,16,17,32,65,4096]:
        points=rng.integers(-10000000,10000000,(count+1,3),dtype=np.int32);target=rng.integers(-10000000,10000000,3,dtype=np.int32);outputs=[]
        for i in range(count):
            x=warp(lib,points[i],points[i+1],points[-1],target,i,count);outputs.append(x)
            expected=np.concatenate([points[i+v].astype(np.int64)+rnd((target.astype(np.int64)-points[-1].astype(np.int64))*(i+v)/count) for v in [0,1]])
            assert np.array_equal(x,expected);random_segments+=1
        outputs=np.array(outputs);assert np.array_equal(outputs[0,:3],points[0]) and np.array_equal(outputs[-1,3:],target) and np.array_equal(outputs[:-1,3:],outputs[1:,:3])
        check(f'Curved/doubled line count {count}: anchor, endpoint, continuity, independent rounding',True)
    zeros=np.zeros(3,np.int32);target=np.array([1234,-56789,4567],np.int32)
    check('Zero correction preserves both endpoints bit-for-bit',np.array_equal(warp(lib,zeros,target,target,target,3,4),np.r_[zeros,target]))
    for label,i,n in [('zero count',0,0),('excess count',0,4097),('past-end segment',16,16),('unsigned-index maximum',0xffffffff,16)]:
        out=np.ones(6,np.int32)*98765;rc=lib.rtg_web_segment(ptr(zeros,IP),ptr(zeros,IP),ptr(zeros,IP),ptr(target,IP),i,n,ptr(out,IP))
        check('Segment '+label+' rejected without output writes',rc!=0 and np.all(out==98765))
    out=np.ones(6,np.int32)*98765;imax=np.array([2147483647]*3,np.int32);imin=np.array([-2147483648]*3,np.int32)
    rc=lib.rtg_web_segment(ptr(imax,IP),ptr(imax,IP),ptr(imin,IP),ptr(imax,IP),1,2,ptr(out,IP))
    check('Intermediate segment overflow rejected atomically',rc!=0 and np.all(out==98765))
    config=json.loads((a.project/'spiderman/config/spiderman.json').read_text());text=json.dumps(config)
    hook='Recompiled.SuitWebAttachment.ProjectSwingSegment';source=(a.project/'spiderman/generated/main.cs').read_text()
    check('Instruction hook survives codegen configuration (static)',text.count(hook)==1 and '8002CF80' in text)
    check('Generated main calls shipping hook exactly once (static)',source.count(hook+'(c, m);')==1)
    exe=(a.project/'spiderman/extracted/SLUS_008.75').read_bytes();base=struct.unpack_from('<I',exe,0x18)[0]
    opcode=struct.unpack_from('<I',exe,0x8002cf80-base+0x800)[0]
    check('Original hook address is JAL 80080988',opcode==(0x0c000000|((0x80080988>>2)&0x03ffffff)),opcode=hex(opcode))
    cs=(a.project/'spiderman/patches/SuitWebAttachment.cs').read_text()
    check('Managed hook contains only scratch output writes (static)',cs.count('m.Write')==1 and 'm.WriteU32(Scratch + i * 4' in cs)
    check('Managed integration scopes RTG2 suit / live swing object / current cache (static)',all(s in cs for s in ['rig == null','controller + 0x178','primary + 0x68','0x136','0x138','segment == 0','segment - 1']))
    result={'status':'PASS','boundary':'Compiled native core and original socket/line-input oracle. Managed hook statically checked, not compiled/run as full game. Fixture anchor/actor transform and forward explicitly controlled; not a saved gameplay state.','passed':len(checks),'failed':0,'checks':checks,'metrics':metrics,'randomLineSegments':random_segments,'seconds':time.time()-start_time,'hashes':{'nativeCore':sha(a.project/'tools/retarget/native/retarget_core.cpp'),'previousCore':sha(a.previous_core),'nativeAnimationBank':sha(a.bank)}}
    (a.out/'WEB_TEST_RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'passed':len(checks),'metrics':metrics},indent=2),flush=True)
if __name__=='__main__':main()
