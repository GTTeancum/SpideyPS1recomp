"""Check the shipped animation strides and reject the reproduced lizard layout."""
from pathlib import Path
import struct
import unittest
import zipfile

from build_bundled_runtime_assets import ROOT, validate_actor_lods


class BundledActorLods(unittest.TestCase):
    def test_both_bundles(self):
        for game in ('spiderman', 'spiderman2'):
            with zipfile.ZipFile(ROOT / game / 'port/bundled/runtime-assets.zip') as archive:
                for name in archive.namelist():
                    if '/' not in name and name.endswith('.psx'):
                        with self.subTest(game=game, actor=name):
                            validate_actor_lods(archive.read(name), name)

    def test_extra_terminal_lod_is_rejected(self):
        with zipfile.ZipFile(ROOT / 'spiderman/port/bundled/runtime-assets.zip') as archive:
            data = bytearray(archive.read('lizman.psx'))
        objects = struct.unpack_from('<I', data, 8)[0]
        first_mesh = struct.unpack_from('<I', data, 16 + objects * 36)[0]
        self.assertNotEqual(struct.unpack_from('<H', data, first_mesh + 26)[0], 0xFFFF)
        struct.pack_into('<H', data, first_mesh + 26, 0xFFFF)
        with self.assertRaisesRegex(ValueError, '20 terminal LODs for 19 animation bones'):
            validate_actor_lods(data, 'bad-lizard.psx')


if __name__ == '__main__':
    unittest.main()
