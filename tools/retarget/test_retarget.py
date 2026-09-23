#!/usr/bin/env python3
"""Regression tests against real converted actors and the compiled runtime core.

This is NOT a substitute for compiling/running the full .NET game.
"""
import os
os.environ.setdefault('OPENBLAS_NUM_THREADS','1')
os.environ.setdefault('OMP_NUM_THREADS','1')
import argparse, collections, ctypes, hashlib, json, math, struct, subprocess, sys, time
from pathlib import Path
import numpy as np
from PIL import Image
from scene import Scene
from rig_blob import Rig,MAGIC
from proof_render import load_asset
from animation_bank import read
from native_asset import parse,U,H
from materials import bindings

def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def chunks(data):
    p=U(data,4);out={};ranges={}
    while U(data,p)!=0xffffffff:
        tag,n=U(data,p),U(data,p+4);out[tag]=data[p+8:p+8+n];ranges[tag]=(p,n+8);p+=8+n
    return out,ranges,p

def guest_strip(data):
    tags,ranges,p=chunks(data);start,size=ranges[MAGIC];p+=4+72
    n=U(data,p);p+=4+n*4
    for width in [36,516]:n=U(data,p);p+=4+n*width
    if U(data,p)==0xffffffff:
        p+=4
        for _ in range(2):n=U(data,p);p+=4+n*36
    n=U(data,p);p+=4
    guest=bytearray(data[:start]+data[start+size:])
    for i in range(n):struct.pack_into('<I',guest,p-size+i*4,U(data,p+i*4)-size)
    return bytes(guest)

class Checks:
    def __init__(self):self.results=[]
    def check(self,name,ok,**details):
        self.results.append(dict(name=name,passed=bool(ok),**details))
        print(('PASS ' if ok else 'FAIL ')+name,flush=True)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--project',required=True,type=Path);ap.add_argument('--samples',required=True,type=Path)
    ap.add_argument('--batch-manifest',type=Path,help='Journal from batch_smu.py; test all successfully converted records')
    ap.add_argument('--suits',required=True,type=Path);ap.add_argument('--animation-bank',required=True,type=Path);ap.add_argument('--out',required=True,type=Path)
    a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True);c=Checks();metrics={};inputs={}
    donor=a.project/'spiderman/extracted/wad/spidey.psx';raw=donor.read_bytes();dtags,_,_=chunks(raw)
    clips=read(a.animation_bank);frames=sum(map(len,clips));c.check('300 native clips / 4196 source frames',len(clips)==300 and frames==4196)
    allposes=np.concatenate(clips);det=np.linalg.det(allposes[:,:,:,:3]);c.check('original decoder produces nonsingular Q12 driver rotations',np.isfinite(allposes).all() and det.min()>.98 and det.max()<1.02,minDet=float(det.min()),maxDet=float(det.max()))
    selected=[('smu-spiderham','costumes/spiderham/spiderham.fbx','textures-png/SpiderHam_D.png'),('smu-2099','costumes/2099/2099.fbx','textures-png/Spiderman2099_D.png')]
    if a.batch_manifest:
        batch=json.loads(a.batch_manifest.read_text())
        if not batch.get('results') or any(r['status']!='converted' for r in batch['results']):
            raise ValueError('Batch contains unconverted or unsupported records')
        selected=[(r['id'],r['fbx'],r['textureFiles'][0]) for r in batch['results']]
    for suit,fbx,tex in selected:
        folder=a.suits/suit;asset=folder/'actor.psx';source=a.samples/fbx;texture=a.samples/tex
        for p in [source,texture,donor,asset]:inputs[str(p)]=digest(p)
        s=Scene(source);data=asset.read_bytes();parsed,rig=load_asset(asset);tags,_,_=chunks(data);p=rig.provenance
        pre=suit+': ';packets=rig.packets();controls=np.array(p['controlPointForVertex'])
        c.check(pre+'entire native object table including flags/XYZ preserved',data[12:660]==raw[12:660])
        c.check(pre+'native animation/HIER bytes preserved',all(tags.get(k)==v for k,v in dtags.items()))
        c.check(pre+'source skeleton names, parents and complete bind matrices preserved',p['boneNames']==s.names and p['sourceParents']==s.parents and np.array_equal(p['sourceBindMatrices'],s.source_bind))
        c.check(pre+'original cluster matrices and exact unnormalized weights preserved',p['sourceClusters']==s.clusters and p['sourceProperties']==s.source_properties and np.array_equal(p['meshBind'],s.mesh_bind))
        if hasattr(s,'source_meshes'):
            c.check(pre+'all source meshes retain their own bind, control points and clusters',p.get('sourceMeshes')==s.source_meshes)
        if s.normal_repairs:
            c.check(pre+'only undefined source normals are repaired with provenance',p.get('sourceNormalRepairs')==s.normal_repairs)
        wc=[];influence_error=0
        for i,ctrl in enumerate(controls):
            first,count=struct.unpack_from('<II',rig.blob,rig.h[8]+i*32+24)
            actual=[struct.unpack_from('<If',rig.blob,rig.h[9]+(first+j)*8) for j in range(count)]
            expected=s.weights[ctrl];wc.append(len(actual)==len(expected) and [b for b,w in actual]==[b for b,w in expected])
            if len(actual)==len(expected):influence_error=max(influence_error,max(abs(x[1]-y[1]) for x,y in zip(actual,expected)))
        c.check(pre+'every skin influence survives without template reassignment',all(wc) and influence_error<6e-8,maxFloat32WeightError=influence_error)
        observed=[]
        for i,m in enumerate(parsed['meshes']):
            if i in (6,11):continue
            for f in m['faces']:observed.append(tuple(controls[packets[i][np.array(f['indices'])[[0,2,1]]]].tolist()))
        c.check(pre+'every original triangle survives exactly once',collections.Counter(observed)==collections.Counter(map(tuple,s.faces.tolist())),triangles=len(observed))
        c.check(pre+'native packet capacity and face references valid',all(len(x)<=256 for x in packets) and all(all(max(f['indices'])<len(packets[i]) for f in m['faces']) for i,m in enumerate(parsed['meshes'])))
        c.check(pre+'both alternate-hand packets and wrist origins identical',all(np.array_equal(packets[x],packets[y]) and parsed['meshes'][x]['faces']==parsed['meshes'][y]['faces'] and np.array_equal(parsed['origins'][x],parsed['origins'][y]) for x,y in [(5,6),(10,11)]))
        source_v=rig.source_vertices();bind_v,_=rig.evaluate(flags=3);bind_error=float(abs(source_v[:,:3]-bind_v[:,:3]).max())
        c.check(pre+'weighted source-bind reconstruction through real core',bind_error<.002,maxNativeUnitError=bind_error)
        rest=np.array([np.frombuffer(rig.blob,'<f4',12,rig.h[7]+i*196+64).reshape(3,4) for i in range(rig.bones)])
        opened,ob=rig.evaluate(flags=1);closed,cb=rig.evaluate()
        c.check(pre+'identity native pose reproduces calibrated target rest, not donor proportions',np.allclose(ob,rest,atol=.001,rtol=1e-6))
        finger=[i for i,n in enumerate(s.names) if 'ArmDigit' in n];nonfinger=[i for i in range(rig.bones) if i not in finger]
        posechanges=[np.max(abs(ob[i,:,:3]-cb[i,:,:3])) for i in finger]
        c.check(pre+'finger/thumb fist pose articulated independently of wrists/body',all(x>.05 for x in posechanges) and np.array_equal(ob[nonfinger],cb[nonfinger]),fingerBones=len(finger))
        thumb_count=sum(n in s.names for n in ['Clown001'+side+'ArmDigit0'+segment for side in ('L','R') for segment in ('1','2')])
        c.check(pre+'thumb opposition retained on all authored thumb joints',sum(x.get('method')=='opposed-thumb-across-knuckles' for x in p['fistPose'])==thumb_count)
        perturbed=rig.rest_driver();perturbed[[3,4,5,6,8,9,10,11],:,3]+=np.array([2500,-3500,1700])
        c.check(pre+'donor arm/shoulder translations cannot produce shoulder shrug',np.array_equal(rig.evaluate(perturbed)[0],closed))
        dr=rig.rest_driver();delta=np.array([23,-141,72],np.float32);dr[0,:,3]+=delta
        moved,mb=rig.evaluate(dr);ratio=struct.unpack_from('<f',rig.blob,60)[0]
        root_error=float(abs((moved[:,:3]-closed[:,:3])-delta*ratio).max())
        c.check(pre+'animation-local root delta explicitly uses target leg ratio',root_error<.002,maxNativeUnitError=root_error,scale=ratio)
        v1,b1=rig.evaluate(clips[0][0]);rig.evaluate(clips[19][-1]);v2,b2=rig.evaluate(clips[0][0])
        c.check(pre+'A-B-A evaluation has no accumulated drift',np.array_equal(v1,v2) and np.array_equal(b1,b2))
        guest=guest_strip(data);g=parse(guest)
        c.check(pre+'host rig strip / texture relocation yields ordinary native guest asset',MAGIC not in chunks(guest)[0] and all(chunks(guest)[0].get(k)==v for k,v in dtags.items()) and all(np.array_equal(g['textures'][k],v) for k,v in parsed['textures'].items()) and guest[12:660]==raw[12:660])
        material_records,images,material_indices,render_uv=bindings(s,texture)
        texture_identity=all(np.array_equal(np.array(Image.open(folder/'textures'/('diffuse.png' if i==0 else f'diffuse-{i}.png'))),np.array(im)) for i,im in enumerate(images))
        c.check(pre+'all full-resolution material textures preserve authored RGB/alpha and repeat tiles',texture_identity)
        expected_materials=collections.Counter((tuple(face),int(slot)) for face,slot in zip(s.faces,material_indices))
        observed_materials=[]
        for i,m in enumerate(parsed['meshes']):
            if i in (6,11):continue
            for face in m['faces']:
                indices=controls[packets[i][np.array(face['indices'])[[0,2,1]]]]
                observed_materials.append((tuple(indices.tolist()),face['slot']))
        c.check(pre+'every triangle retains its authored material slot',collections.Counter(observed_materials)==expected_materials)
        small=np.array(images[0].convert('RGB').resize((128,128),Image.Resampling.LANCZOS),float);native=parsed['textures'][0][:,:,:3].astype(float)
        err=float(abs(small-native).mean());flipped=float(abs(small-native[::-1]).mean())
        c.check(pre+'native fallback atlas has correct vertical orientation',err<15 and err<flipped,meanRGBError=err,flippedMeanRGBError=flipped)
        manifest=json.loads((folder/'suit.json').read_text());c.check(pre+'native costume manifest matches loader fields',manifest.get('version')==1 and manifest.get('id')==suit and manifest.get('model')=='spiderman' and manifest.get('modelFile')=='actor.psx' and manifest.get('abilities')=={'profile':'spiderman'} and 1<=len(manifest['name'])<=19)
        # Exhaust every original frame through the exact runtime shared library.
        parents=np.array(s.parents);anchor=rig.h[6];indices=np.array([i for i,x in enumerate(parents) if x>=0 and i!=anchor]);par=parents[indices]
        lengths=np.linalg.norm(rest[indices,:,3]-rest[par,:,3],axis=1);nonzero=lengths>1e-3
        max_length_rel=0.;max_local=0.;max_comp_error=0.;max_quant_error=0.;passed=True;worst=None
        t0=time.perf_counter()
        for ci,clip in enumerate(clips):
            for fi,driver in enumerate(clip):
                v,b=rig.evaluate(driver)
                le=np.linalg.norm(b[indices,:,3]-b[par,:,3],axis=1)
                max_length_rel=max(max_length_rel,float((abs(le[nonzero]-lengths[nonzero])/lengths[nonzero]).max()))
                for i,ids in enumerate(packets):
                    if not len(ids):continue
                    local=rig.part_local(driver[i],v[ids]);max_local=max(max_local,float(abs(local[:,:3]).max()))
                    reconstructed=local[:,:3]@driver[i,:,:3].T+driver[i,:,3]
                    max_comp_error=max(max_comp_error,float(abs(reconstructed-v[ids,:3]).max()))
                    quantized=np.rint(local[:,:3])@driver[i,:,:3].T+driver[i,:,3]
                    max_quant_error=max(max_quant_error,float(abs(quantized-v[ids,:3]).max()))
                    if not np.isfinite(local).all() or np.max(abs(local[:,:3]))>32760:passed=False;worst=[ci,fi,i]
        elapsed=time.perf_counter()-t0
        c.check(pre+'all 4196 original frames pass weighted skinning and s16 packet bounds',passed,maxAbsLocal=max_local,worst=worst)
        c.check(pre+'all source link lengths preserved across original animations',max_length_rel<.003,maxRelativeError=max_length_rel)
        c.check(pre+'native transform applied once, not double-skinned',max_comp_error<.004 and max_quant_error<.9,maxFloatError=max_comp_error,maxRoundedComponentError=max_quant_error)
        # Exercise validator independently of the converter's own checks.
        bad=[]
        for length in [0,1,32,279,len(rig.blob)-1]:bad.append(rig.blob[:length])
        def mut(off,fmt,val):z=bytearray(rig.blob);struct.pack_into(fmt,z,off,val);bad.append(bytes(z))
        for offset,value in [(0,0),(4,999),(8,0),(12,257),(16,4097),(20,65537),(24,9999),(28,0xffffffff),(32,0xffffffff),(36,0xffffffff),(40,0xffffffff),(44,0xffffffff),(52,0xffffffff)]:mut(offset,'<I',value)
        mut(60,'<f',float('nan'));mut(60,'<f',0)
        mut(rig.h[7],'<i',0);mut(rig.h[7]+4,'<i',18);mut(rig.h[7]+16,'<f',float('inf'))
        mut(rig.h[8]+28,'<I',0);mut(rig.h[9],'<I',rig.bones);mut(rig.h[9]+4,'<f',-1);mut(rig.h[10],'<I',257)
        rejections=[]
        for blob in bad:
            buf=ctypes.create_string_buffer(blob);rejections.append(rig.lib.rtg_validate(buf,len(blob))!=0)
        c.check(pre+'truncated/corrupt rigs rejected before guest write',all(rejections),malformedCases=len(bad))
        report=json.loads((folder/'conversion-report.json').read_text())
        current_inputs=[source,texture,donor,a.samples/'costumes/2099/2099.fbx']+[Path(r['source']) for r in material_records]
        c.check(pre+'all conversion inputs remain hash-identical',
                sorted(report['inputHashes'].values())==sorted({str(path.resolve()):digest(path) for path in current_inputs}.values()))
        metrics[suit]=dict(bones=rig.bones,sourceControlPoints=len(s.vertices),sourceInfluences=sum(map(len,s.weights)),runtimeVertices=rig.count,runtimeInfluences=rig.h[5],triangles=len(observed),nativeBytes=len(data),sha256=digest(asset),rootScale=ratio,unitScale=p['referenceUnitScale'],maxLinkRelativeError=max_length_rel,maxRoundedComponentError=max_quant_error,frames=frames,clips=len(clips),testSeconds=elapsed)
        checkpoint={'scope':'Offline native asset/core test; no full game execution', 'completedSuits':list(metrics),'metrics':metrics,'checks':c.results}
        temporary=a.out/'checkpoint.json.tmp';temporary.write_text(json.dumps(checkpoint,indent=2)+'\n');temporary.replace(a.out/'checkpoint.json')
    if 'smu-spiderham' in metrics and 'smu-2099' in metrics:
        c.check('normal and short character share unit conversion, not height fit',metrics['smu-spiderham']['unitScale']==metrics['smu-2099']['unitScale'] and metrics['smu-spiderham']['rootScale']<metrics['smu-2099']['rootScale']*.8)
    generated=(a.project/'spiderman/generated/main.cs').read_text();hook=(a.project/'spiderman/patches/SuitRetargeting.cs').read_text();cfg=(a.project/'spiderman/config/spiderman.json').read_text()
    c.check('source integration: exactly one pose-ready hook and regeneration config',generated.count('Recompiled.SuitRetargeting.ApplyPose(c, m);')+generated.count('Recompiled.SuitRetargeting.ApplyPose(c,m);')==1 and 'L80077418:' in generated and 'Recompiled.SuitRetargeting.ApplyPose' in cfg and 'instruction' in cfg)
    c.check('source integration: resident validation / fists / culling update present',all(s in hook for s in ['rig.Evaluate(Driver)','0x800A0904','0x64697073','nn != nv + nf','mesh + 8','SPIDEY_RETARGET_TRACE']))
    dll=a.project/'tools/RecompOne/native/win-x64/OpenSpideyRetarget.dll'
    c.check('Windows deployed native DLL is the tested-source build artifact',dll.read_bytes()==(Path(__file__).parent/'native/bin/OpenSpideyRetarget.dll').read_bytes() and dll.read_bytes()[:2]==b'MZ')
    results=dict(status='PASS' if all(x['passed'] for x in c.results) else 'FAIL',scope='Native assets + compiled C++ runtime core + original animation decoding oracle; C# integration checks are static only. Full .NET build/game execution NOT run.',passed=sum(x['passed'] for x in c.results),failed=sum(not x['passed'] for x in c.results),metrics=metrics,results=c.results,inputHashes=inputs,animationBankSha256=digest(a.animation_bank))
    (a.out/'TEST_RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps({k:v for k,v in results.items() if k not in ['results','inputHashes']},indent=2))
    return 0 if results['failed']==0 else 1
if __name__=='__main__':sys.exit(main())
