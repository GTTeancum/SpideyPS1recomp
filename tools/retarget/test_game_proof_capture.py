"""Capture completeness tests, not gameplay visual acceptance."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image
from run_game_proof import verify_captures


class CaptureTests(unittest.TestCase):
    def test_exact_native_crop_pair(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            full = Image.new('RGB', (32, 24), 'black')
            full.putpixel((12, 12), (200, 100, 50))
            full.save(out/'frame_00001.png')
            detail = full.crop((8, 8, 24, 20))
            detail.save(out/'frame_00001_closeup.png')
            self.assertTrue(verify_captures(out, [495], (8, 8, 16, 12)))
            self.assertFalse(verify_captures(out, [495, 800], (8, 8, 16, 12)))
            self.assertFalse(verify_captures(out, [495], None))
            self.assertFalse(verify_captures(out, [495], (20, 8, 16, 12)))
            detail.putpixel((4, 4), (0, 0, 0))
            detail.save(out/'frame_00001_closeup.png')
            self.assertFalse(verify_captures(out, [495], (8, 8, 16, 12)))

    def test_missing_or_extra_images(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            self.assertFalse(verify_captures(out, [495], None))
            Image.new('RGB', (32, 24)).save(out/'frame_00001.png')
            self.assertTrue(verify_captures(out, [495], None))
            self.assertFalse(verify_captures(out, [495], (0, 0, 8, 8)))
            Image.new('RGB', (32, 24)).save(out/'unrequested.png')
            self.assertFalse(verify_captures(out, [495], None))

    def test_crop_budget_rejected_before_runtime_setup(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)/'must-not-exist'
            result = subprocess.run([sys.executable, str(Path(__file__).with_name('run_game_proof.py')),
                                     '--build', 'missing', '--data', 'missing', '--suits', 'missing',
                                     '--id', 'missing', '--out', str(out), '--crop', '0,0,16,16'],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn('exactly one shot', result.stderr)
            self.assertFalse(out.exists())


if __name__ == '__main__':
    unittest.main()
