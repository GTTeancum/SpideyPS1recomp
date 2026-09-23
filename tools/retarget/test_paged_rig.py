"""Real-source page/core checks. Does not claim native rendering acceptance."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct

import numpy as np
from animation_bank import read
from batch_smu import inventory
from convert import donor_info
from rig_blob import Rig, unique_vertices, packetize, make_blob
from scene import Scene, calibrate
from test_retarget import chunks
from rig_blob import MAGIC


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('project','samples','library','reference-library','animation-bank','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    origins,ground,_,_=donor_info((a.project/'spiderman/extracted/wad/spidey.psx').read_bytes())
    reference=Scene(a.samples/'costumes/2099/2099.fbx')
    catalogue={r['key']:r for r in inventory(a.samples)}
    poses=np.concatenate(read(a.animation_bank))
    assert len(poses)==4196
    report={'scope':'Offline paged metadata/core only; no native actor or game acceptance.',
            'librarySha256':hashlib.sha256(a.library.read_bytes()).hexdigest(),
            'referenceLibrarySha256':hashlib.sha256(a.reference_library.read_bytes()).hexdigest(),'suits':[]}
    assert report['librarySha256']!=report['referenceLibrarySha256'], 'Legacy comparison requires a distinct baseline core'
    a.out.mkdir(parents=True,exist_ok=True)
    legacy=[]
    for actor in sorted((a.project/'spiderman/port/mods/suits').glob('*/actor.psx')):
        tags,_,_=chunks(actor.read_bytes())
        if MAGIC not in tags:continue
        if struct.unpack_from('<I',tags[MAGIC],4)[0]!=2:continue
        original=Rig(tags[MAGIC],library=a.reference_library);candidate=Rig(tags[MAGIC],library=a.library)
        assert original.h[1]==2
        for index in (0,1,1000,2500,4195):
            old_vertices,old_bones=original.evaluate(poses[index])
            new_vertices,new_bones=candidate.evaluate(poses[index])
            assert np.array_equal(old_vertices,new_vertices) and np.array_equal(old_bones,new_bones)
        legacy.append(actor.parent.name)
    assert len(legacy)==231
    report['legacyCompatibility']={'suits':legacy,'framesPerSuit':[0,1,1000,2500,4195],
                                   'result':'Bit-exact bones and weighted vertices against shipping core'}
    print('PASS legacy core comparison: 231 suits, five specified poses each',flush=True)
    for key in ('damon_ryder','other'):
        scene=Scene(a.samples/catalogue[key]['fbx'])
        cal=calibrate(scene,reference,origins,ground)
        vertices,controls,faces=unique_vertices(scene,cal)
        packets,bins,_=packetize(faces,controls,scene,cal)
        assert 18<len(packets)<=64 and all(len(x)<=256 for x in packets)
        coverage=collections.Counter(f for i,b in enumerate(bins) if i not in (6,11) for f in b)
        assert coverage==collections.Counter(range(len(faces)))
        assert packets[6]==packets[5] and packets[11]==packets[10]
        for ids,triangles in zip(packets,bins):
            assert set(map(int,faces[triangles].ravel()))==set(ids)
        blob=make_blob(scene,cal,vertices,controls,faces,packets)
        rig=Rig(blob,library=a.library)
        assert rig.packet_drivers()==list(range(18))+[0]*(len(packets)-18)
        assert rig.provenance['sourceParents']==scene.parents
        assert rig.provenance['sourceClusters']==scene.clusters
        assert np.array_equal(rig.provenance['sourceBindMatrices'],scene.source_bind)
        assert np.array_equal(rig.faces(),faces)
        bound=float(np.max(abs(rig.evaluate(flags=3)[0][:,:3]-vertices[:,:3])))
        assert bound<0.02
        malformed=[]
        mapping=struct.unpack_from('<I',blob,284)[0]
        packet_table=rig.h[10]
        cases={'page-count-low':(280,18),'page-count-high':(280,65),
               'map-outside':(284,len(blob)),'reserved':(288,1),
               'root-map':(mapping,1),'extra-map':(mapping+18*4,1),
               'page-too-large':(packet_table,257),'page-overlap':(packet_table+4,0),
               'page-after-faces':(packet_table+4,rig.h[13]+4),
               'vertex-limit':(16,8193),'bone-overlap':(28,280)}
        for name,(offset,value) in cases.items():
            bad=bytearray(blob);struct.pack_into('<I',bad,offset,value)
            try:Rig(bad,library=a.library)
            except ValueError:malformed.append(name)
            else:raise AssertionError('Malformed rig accepted: '+name)
        error=0.0
        for pose in poses:
            world,_=rig.evaluate(pose)
            for ids,driver in zip(packets,rig.packet_drivers()):
                local=rig.part_local(pose[driver],world[ids])
                assert np.isfinite(local).all() and np.max(abs(local[:,:3]),initial=0)<32760
                rebuilt=local[:,:3]@pose[driver,:,:3].T+pose[driver,:,3]
                error=max(error,float(np.max(abs(rebuilt-world[ids,:3]),initial=0)))
        assert error<0.02
        (a.out/(key+'.rtg')).write_bytes(blob)
        row=dict(suit=key,vertices=len(vertices),triangles=len(faces),pages=len(packets),
                 frames=len(poses),bindError=bound,pageRoundTripError=error,
                 malformedRejected=malformed,sourceSha256=scene.sha256,
                 rigSha256=hashlib.sha256(blob).hexdigest(),status='PASS')
        report['suits'].append(row);print(json.dumps(row),flush=True)
    report['status']='PASS'
    (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()
