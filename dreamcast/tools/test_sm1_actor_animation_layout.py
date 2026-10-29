"""Native animation-count regressions using synthetic actors; no retail assets."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest


CONVERTER_PATH = Path(__file__).resolve().parents[2] / "spiderman/tools/port_dc_character.py"
SPEC = importlib.util.spec_from_file_location("sm1_actor_animation_converter", CONVERTER_PATH)
converter = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = converter
SPEC.loader.exec_module(converter)


def actor(objects, links):
    """Emit a v6 actor with distinct vertex bytes in every LOD mesh."""
    data = bytearray(struct.pack("<HHII", 6, 2, 0, objects))
    data.extend(bytes(objects * 36))
    data.extend(struct.pack("<I", len(links)))
    table = len(data)
    data.extend(bytes(len(links) * 4))
    for index, link in enumerate(links):
        struct.pack_into("<I", data, table + index * 4, len(data))
        header = bytearray(28)
        struct.pack_into("<H", header, 2, 1)  # One vertex, no normals or faces.
        struct.pack_into("<HH", header, 24, 32767 if link == 0xFFFF else 2048, link)
        data.extend(header)
        data.extend(struct.pack("<hhhH", index + 1, -index - 2, index * 3, 0))
    struct.pack_into("<I", data, 4, len(data))
    data.extend(struct.pack("<I", 0xFFFFFFFF))
    data.extend(struct.pack(f"<{len(links)}I", *range(1, len(links) + 1)))
    data.extend(bytes(16))  # No textures or palettes.
    return bytes(data)


def meshes(data):
    objects = struct.unpack_from("<I", data, 8)[0]
    count_offset = 12 + objects * 36
    count = struct.unpack_from("<I", data, count_offset)[0]
    pointers = struct.unpack_from(f"<{count}I", data, count_offset + 4)
    return [data[pointer:pointer + 36] for pointer in pointers]


class Sm1ActorAnimationLayoutTests(unittest.TestCase):
    def convert(self, source, **options):
        with tempfile.TemporaryDirectory(prefix="sm1-actor-layout-") as directory:
            path = Path(directory) / "synthetic.psx"
            path.write_bytes(source)
            model = converter.parse_model(path)
            return converter.build_character(model, Path(directory), 4, None, **options)

    def test_one_terminal_mesh_per_part_preserves_geometry(self):
        source = actor(19, [0xFFFF] * 19)
        self.assertEqual(meshes(source), meshes(self.convert(source)))

    def test_two_valid_lod_tiers_preserve_every_mesh(self):
        source = actor(19, list(range(19, 38)) + [0xFFFF] * 19)
        self.assertEqual(meshes(source), meshes(self.convert(source)))

    def test_three_chained_lod_tiers_do_not_add_animation_parts(self):
        source = actor(19, list(range(19, 57)) + [0xFFFF] * 19)
        self.assertEqual(meshes(source), meshes(self.convert(source)))

    def test_legacy_lizman_duplicate_terminals_are_rejected(self):
        # The shipped legacy lizman had 19 full-detail, 19 reduced, and 18
        # unlinked duplicate reduced meshes. ParsePSX counts all 37 terminals.
        source = actor(19, list(range(19, 38)) + [0xFFFF] * 37)
        with self.assertRaisesRegex(ValueError, "56 meshes with 37 terminal LOD links for 19"):
            self.convert(source)

    def test_missing_terminal_parts_are_rejected(self):
        source = actor(2, [2, 3, 0xFFFF, 0])
        with self.assertRaisesRegex(ValueError, "4 meshes with 1 terminal LOD links for 2"):
            self.convert(source)

    def test_single_lod_conversion_validates_emitted_layout(self):
        source = actor(19, list(range(19, 38)) + [0xFFFF] * 37)
        output = meshes(self.convert(source, sm1_single_lod=True))
        self.assertEqual(len(output), 19)
        for before, after in zip(meshes(source), output):
            self.assertEqual(before[:24], after[:24])
            self.assertEqual(before[28:], after[28:])
            self.assertEqual(struct.unpack_from("<HH", after, 24), (32767, 0xFFFF))


if __name__ == "__main__":
    unittest.main()
