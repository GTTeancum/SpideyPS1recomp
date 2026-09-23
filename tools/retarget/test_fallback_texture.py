"""Negative controls for the source-derived native fallback texture oracle."""
import unittest
import numpy as np
from PIL import Image
from native_asset import expand_texel
from test_retarget import expected_fallback_rgba


class FallbackTextureTests(unittest.TestCase):
    def test_native_palette_semantics(self):
        self.assertEqual(expand_texel(0),[0,0,0,0])
        self.assertEqual(expand_texel(0x8000),[0,0,0,128])
        self.assertEqual(expand_texel(0x7c1f),[255,0,255,255])
        self.assertEqual(expand_texel(0xfc1f),[255,0,255,128])
        self.assertEqual(expand_texel(4),[33,0,0,255])

    def test_uniform_texture_needs_no_artificial_asymmetry(self):
        actual=expected_fallback_rgba(Image.new('RGB',(128,128),(2,2,2)),False)
        self.assertTrue(np.array_equal(actual,actual[::-1]))
        self.assertTrue(np.all(actual==[0,0,0,128]))

    def test_flip_and_slot_swap_are_detectable(self):
        pixels=np.zeros((128,128,3),np.uint8)
        pixels[:64]=[255,0,0];pixels[64:]=[0,255,0]
        expected=expected_fallback_rgba(Image.fromarray(pixels),False)
        self.assertFalse(np.array_equal(expected,expected[::-1]))
        other=expected_fallback_rgba(Image.new('RGB',(128,128),(0,0,255)),False)
        self.assertFalse(np.array_equal(expected,other))

    def test_alpha_is_checked_independently_of_hidden_rgb(self):
        pixels=np.full((128,128,4),255,np.uint8)
        pixels[:64, :, 3]=0
        expected=expected_fallback_rgba(Image.fromarray(pixels),True)
        self.assertTrue(np.all(expected[:64]==0))
        self.assertTrue(np.all(expected[64:]==255))
        altered=expected.copy();altered[:64,:,3]=255
        self.assertFalse(np.array_equal(expected,altered))


if __name__=='__main__':unittest.main()
