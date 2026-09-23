"""Guard against treating native transport packets as anatomical hand meshes."""
from collections import Counter
from pathlib import Path
import unittest

import numpy as np

from inspect_hand_pose import hand_vertices
from proof_render import load_asset, triangles


class HandDiagnosticTests(unittest.TestCase):
    def test_complete_hand_faces_across_legacy_and_paged_packets(self):
        suits = Path(__file__).resolve().parents[2] / 'spiderman/port/mods/suits'
        for key in ('smu-amazing-spider', 'smu-2099', 'smu-spiderham', 'smu-other'):
            with self.subTest(suit=key):
                parsed, rig = load_asset(suits / key / 'actor.psx')
                rendered, _, _ = triangles(parsed, rig, quantize=False)
                actual = Counter(tuple(sorted(map(int, t['vertex_ids']))) for t in rendered)
                expected = Counter(tuple(sorted(map(int, face))) for face in rig.faces())
                self.assertEqual(actual, expected)
                world, _ = rig.evaluate()
                for t in rendered:
                    self.assertNotIn(t['mesh'], (6, 11))
                    np.testing.assert_allclose(t['p'], world[t['vertex_ids'], :3], atol=.003, rtol=0)
                for side in ('L', 'R'):
                    _, selected = hand_vertices(rig, side)
                    self.assertTrue(selected)
                    selected_faces = [t for t in rendered if any(int(v) in selected for v in t['vertex_ids'])]
                    expected_count = sum(any(int(v) in selected for v in face) for face in rig.faces())
                    self.assertEqual(len(selected_faces), expected_count)
                    if key == 'smu-amazing-spider' and side == 'L':
                        self.assertEqual(expected_count, 529)
                        self.assertEqual({t['mesh'] for t in selected_faces}, {3, 4, 5, 9, 14, 17})


if __name__ == '__main__':
    unittest.main()
