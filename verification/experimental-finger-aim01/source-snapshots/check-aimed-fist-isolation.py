"""Full-animation isolation of experimental non-thumb finger rotations."""
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

import numpy as np

repo=Path('C:/Programming/GitHub/OpenSpideyPS1')
work=Path(__file__).resolve().parent
sys.path.insert(0,str(repo/'tools/retarget'))
from animation_bank import read
from proof_render import load_asset

batch=work/'fist-aim-candidate01'
bank=work/'batch01-inspect/OpenSpidey-SMU-Batch01-Cumulative-07a/evidence/retained-fixtures/retarget-inputs/native-animation.bin'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for key in ('smu-arana-gymnast','smu-batty-brant'):
    before=repo/'spiderman/port/mods/suits'/key/'actor.psx'
    after=batch/'suits'/key/'actor.psx'
    _,a=load_asset(before)
    _,b=load_asset(after)
    assert {k:v for k,v in a.provenance.items() if k!='fistPose'}=={k:v for k,v in b.provenance.items() if k!='fistPose'}
    names=a.provenance['boneNames']
    changed=set()
    for i,name in enumerate(names):
        start=a.h[7]+i*196
        end=b.h[7]+i*196
        assert a.blob[start:start+160]==b.blob[end:end+160]
        if a.blob[start+160:start+196]!=b.blob[end+160:end+196]:
            assert re.fullmatch(r'Clown001[LR]ArmDigit[235][12]',name),name
            changed.add(i)
    assert len(changed)==12
    assert a.blob[64:a.h[7]]==b.blob[64:b.h[7]]
    assert a.blob[a.h[8]:a.h[11]]==b.blob[b.h[8]:b.h[11]]
    affected=set(changed)
    while True:
        expanded=affected|{i for i,p in enumerate(a.provenance['sourceParents']) if p in affected}
        if expanded==affected:break
        affected=expanded
    unaffected_bones=sorted(set(range(a.bones))-affected)
    unaffected_vertices=[]
    for i in range(a.count):
        first,count=struct.unpack_from('<II',a.blob,a.h[8]+i*32+24)
        influences={struct.unpack_from('<I',a.blob,a.h[9]+(first+j)*8)[0] for j in range(count)}
        if not influences&affected:unaffected_vertices.append(i)
    errors=dict(closedUnaffectedBones=0.,closedUnaffectedVertices=0.,openAllBones=0.,openAllVertices=0.)
    frames=0
    for clip in read(bank):
        for driver in clip:
            av,ab=a.evaluate(driver)
            bv,bb=b.evaluate(driver)
            errors['closedUnaffectedBones']=max(errors['closedUnaffectedBones'],float(abs(ab[unaffected_bones]-bb[unaffected_bones]).max()))
            errors['closedUnaffectedVertices']=max(errors['closedUnaffectedVertices'],float(abs(av[unaffected_vertices]-bv[unaffected_vertices]).max()))
            av,ab=a.evaluate(driver,flags=1)
            bv,bb=b.evaluate(driver,flags=1)
            errors['openAllBones']=max(errors['openAllBones'],float(abs(ab-bb).max()))
            errors['openAllVertices']=max(errors['openAllVertices'],float(abs(av-bv).max()))
            frames+=1
    result=dict(status='PASS' if frames==4196 and not any(errors.values()) else 'FAIL',
        beforeSha256=sha(before),afterSha256=sha(after),animationBankSha256=sha(bank),frames=frames,
        changedBones=[names[i] for i in sorted(changed)],errors=errors,
        unaffectedVertices=len(unaffected_vertices),unaffectedBones=len(unaffected_bones),
        sourceBindWeightsGeometryPacketsAndNonFistProvenanceUnchanged=True,
        scope='Experimental uninstalled pose-only candidate. Exact source/bind/weights/geometry and thumb preservation; full closed-body/open-hand isolation. Not visual closure/contact acceptance or production-policy validation.')
    (batch/(key+'-isolation.json')).write_text(json.dumps(result,indent=2)+'\n')
    print(key+': '+result['status'],flush=True)
    assert result['status']=='PASS'
