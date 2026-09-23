"""Experimental source-relative finger aiming; no production policy/install edits."""
from pathlib import Path
import runpy
import sys

import numpy as np

sys.path.insert(0,'C:/Programming/GitHub/OpenSpideyPS1/tools/retarget')
import convert
from scene import align, unit

original_calibrate=convert.calibrate

def candidate_calibrate(scene,reference,origins,ground):
    cal=original_calibrate(scene,reference,origins,ground)
    bind=cal['bind']
    for side in ('L','R'):
        palm=scene.index(side+'ArmPalm')
        middle=scene.index(side+'ArmDigit31')
        index=scene.index(side+'ArmDigit21')
        little=scene.index(side+'ArmDigit51')
        forward=unit(bind[middle,:3,3]-bind[palm,:3,3])
        across=unit(bind[little,:3,3]-bind[index,:3,3])
        inward=unit(np.cross(across,forward))
        if inward@bind[palm,:3,0]<0:inward=-inward
        for digit in (2,3,5):
            first=scene.index(side+f'ArmDigit{digit}1')
            second=scene.index(side+f'ArmDigit{digit}2')
            length=np.linalg.norm(bind[second,:3,3]-bind[first,:3,3])
            lateral=(bind[middle,:3,3]-bind[first,:3,3])@across
            q1=align(bind[second,:3,3]-bind[first,:3,3],inward)
            q2=align(bind[second,:3,1],-forward+across*(lateral/length)*.35)
            r1,r2=bind[first,:3,:3],bind[second,:3,:3]
            for i,rotation in ((first,r1.T@q1@r1),(second,r2.T@q1.T@q2@r2)):
                cal['fist'][i]=rotation
                record=next(r for r in cal['finger_report'] if r['bone']==scene.names[i])
                record.clear()
                record.update(bone=scene.names[i],method='experimental-source-relative-digit-aim',
                              candidatePolicy=True,convergence=.35,localRotation=rotation.tolist())
    return cal

convert.calibrate=candidate_calibrate
runpy.run_path(str(Path(__file__).with_name('prepare-fist-batch.py')),run_name='__main__')
