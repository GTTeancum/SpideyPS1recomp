"""Verify narrow source-specific claw flexion without changing other rigs."""
import unittest
from scene import non_thumb_flexion


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


if __name__=='__main__':
    unittest.main()
