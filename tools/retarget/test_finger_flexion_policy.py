"""Verify narrow source-specific claw flexion without changing other rigs."""
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import numpy as np
from scene import non_thumb_flexion, apply_finger_aim, Scene, calibrate, FINGER_AIM_SHA256, FINGER_AIM_POLICIES
from convert import donor_info


class FlexionPolicyTests(unittest.TestCase):
    def test_base_curl_is_shared(self):
        for source in ('2099','2099_new','spiderham','other','poison','amazing_spider'):
            self.assertEqual(non_thumb_flexion(source,1),90)

    def test_original_2099_claws_have_eased_distal_joints(self):
        for source in ('2099','2099_new'):
            for segment in (2,3):
                self.assertEqual(non_thumb_flexion(source,segment),65)

    def test_no_prefix_or_general_2099_override(self):
        for source in ('2099_extra','venom_2099','spiderham_2099','spiderham','other','poison'):
            for segment in (2,3):
                self.assertEqual(non_thumb_flexion(source,segment),85)

    def test_aim_policy_does_not_match_other_names(self):
        for source in ('arana', 'arana_gymnast_extra', 'batty_brant_extra'):
            apply_finger_aim(SimpleNamespace(path=Path(source+'.fbx')), None, None, None)

    def test_changed_aim_source_is_rejected_before_mutation(self):
        with self.assertRaisesRegex(ValueError, 'verified source hash'):
            apply_finger_aim(SimpleNamespace(path=Path('arana_gymnast.fbx'), sha256='changed'), None, None, None)
        with self.assertRaisesRegex(ValueError, 'verified source hash'):
            apply_finger_aim(SimpleNamespace(path=Path('batty_brant.fbx'), sha256='changed'), None, None, None)

    def test_measured_aim_preserves_everything_except_twelve_finger_rotations(self):
        self.check_pose_fidelity('arana_gymnast',FINGER_AIM_SHA256)

    def test_batty_aim_preserves_everything_except_twelve_finger_rotations(self):
        self.check_pose_fidelity('batty_brant',FINGER_AIM_POLICIES['batty_brant'][0])

    def check_pose_fidelity(self,key,source_hash):
        samples=Path('C:/Programming/SMU-Costumes/costumes')
        source=Scene(samples/key/(key+'.fbx'))
        reference=Scene(samples/'2099/2099.fbx')
        donor=Path(__file__).resolve().parents[2]/'spiderman/extracted/wad/spidey.psx'
        origins,ground,_,_=donor_info(donor.read_bytes())
        self.assertEqual(source.sha256,source_hash)
        with patch('scene.apply_finger_aim'):
            before=calibrate(source,reference,origins,ground)
        after=calibrate(source,reference,origins,ground)
        for key in before:
            if key in ('fist','finger_report'):continue
            if isinstance(before[key],np.ndarray):np.testing.assert_array_equal(before[key],after[key])
            else:self.assertEqual(before[key],after[key])
        changed=np.where(np.any(before['fist']!=after['fist'],axis=(1,2)))[0]
        expected={source.index(side+f'ArmDigit{digit}{segment}') for side in ('L','R')
                  for digit in (2,3,5) for segment in (1,2)}
        self.assertEqual(set(changed),expected)
        for index in changed:
            matrix=after['fist'][index]
            np.testing.assert_allclose(matrix.T@matrix,np.eye(3),atol=1e-12)
            self.assertAlmostEqual(np.linalg.det(matrix),1.)
        reports=[r for r in after['finger_report'] if r.get('method')=='source-relative-digit-closure-v1']
        self.assertEqual(len(reports),12)
        self.assertTrue(all('candidatePolicy' not in r for r in reports))


if __name__=='__main__':
    unittest.main()
