"""Source-guarded, per-hand experiment. Never installs or alters production policy."""
import json
import argparse
from pathlib import Path
import runpy
import sys
import numpy as np

work = Path(__file__).resolve().parent
sys.path.insert(0, 'C:/Programming/GitHub/OpenSpideyPS1/tools/retarget')
import convert
from scene import align, unit

original_calibrate = convert.calibrate
parser = argparse.ArgumentParser(add_help=False)
parser.add_argument('--left-fit', type=Path, default=work / 'blackcatvenom-contact-search-dynamic.json')
parser.add_argument('--right-fit', type=Path, default=work / 'blackcatvenom-contact-search-right.json')
args, remaining = parser.parse_known_args()
sys.argv = [sys.argv[0], *remaining]
settings = {side: json.loads(path.read_text())['best'] for side, path in
            [('L', args.left_fit), ('R', args.right_fit)]}

def candidate_calibrate(scene, reference, origins, ground):
    assert scene.path.stem == 'blackcatvenom'
    assert scene.sha256 == 'fddcb2b97d41097717bf686f26a7663c2d7c10bd0eb8ab2ddc46925c196fcffc'
    cal = original_calibrate(scene, reference, origins, ground)
    bind = cal['bind']
    for side in ('L', 'R'):
        palm, middle, index, little = [scene.index(side + name) for name in
            ('ArmPalm', 'ArmDigit31', 'ArmDigit21', 'ArmDigit51')]
        forward = unit(bind[middle, :3, 3] - bind[palm, :3, 3])
        across = unit(bind[little, :3, 3] - bind[index, :3, 3])
        inward = unit(np.cross(across, forward))
        if inward @ bind[palm, :3, 0] < 0:
            inward = -inward
        for tuning in settings[side]:
            digit = tuning['digit']
            first, second = [scene.index(side + f'ArmDigit{digit}{segment}') for segment in (1, 2)]
            length = np.linalg.norm(bind[second, :3, 3] - bind[first, :3, 3])
            lateral = (bind[middle, :3, 3] - bind[first, :3, 3]) @ across
            q1 = align(bind[second, :3, 3] - bind[first, :3, 3], inward - forward * tuning['baseLean'])
            q2 = align(bind[second, :3, 1], -forward + across * (lateral / length) * tuning['convergence'] - inward * tuning['closure'])
            r1, r2 = bind[first, :3, :3], bind[second, :3, :3]
            for i, rotation in ((first, r1.T @ q1 @ r1), (second, r2.T @ q1.T @ q2 @ r2)):
                cal['fist'][i] = rotation
                report = next(r for r in cal['finger_report'] if r['bone'] == scene.names[i])
                report.clear()
                report.update(bone=scene.names[i], method='experimental-source-relative-digit-aim',
                              candidatePolicy=True, baseLean=tuning['baseLean'],
                              convergence=tuning['convergence'], closure=tuning['closure'],
                              localRotation=rotation.tolist())
    return cal

convert.calibrate = candidate_calibrate
runpy.run_path(str(work / 'prepare-fist-batch.py'), run_name='__main__')
