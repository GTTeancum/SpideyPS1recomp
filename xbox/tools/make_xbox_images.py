"""Make the Xbox dashboard images from the game icon.

    python -I xbox/tools/make_xbox_images.py [icon.png] [output folder]

Defaults: spiderman/port/OpenSpidey.png -> xbox/build/release/. Writes
TitleImage.xbx (128x128, the title's picture in the dashboard's saved-games screen)
and SaveImage.xbx (64x64, the save's icon), both XPR0 files holding one DXT1 texture,
the format the dashboard reads; and default.tbn (a 256x256 PNG), the thumbnail
launchers such as XBMC show beside default.xbe. runtime/card.c copies the two .xbx
files from the disc into the title's save folder.
"""
import struct
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]

# NV2A texture format word: context DMA A, 2D, border colour source, one level,
# DXT1 (0x0C), log2 width and height (the hardware's own encoding, as hwgpu.c uses).
DXT1 = 0x0C


def format_word(log2):
    return 0x1 | (2 << 4) | (1 << 3) | (DXT1 << 8) | (1 << 16) | (log2 << 20) | (log2 << 24)


def rgb565(c):
    r, g, b = c[:3]
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def expand(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def dxt1_block(px):
    """One 4x4 block: endpoints at the colour box's extremes along its widest axis;
    transparent texels (alpha < 128) use DXT1's three-colour mode."""
    solid = [p for p in px if p[3] >= 128]
    transparent = len(solid) < 16
    if not solid:
        return struct.pack("<HHI", 0, 0, 0xFFFFFFFF)
    axis = max(range(3), key=lambda k: max(p[k] for p in solid) - min(p[k] for p in solid))
    lo = min(solid, key=lambda p: p[axis])
    hi = max(solid, key=lambda p: p[axis])
    c0, c1 = rgb565(hi), rgb565(lo)
    if transparent:
        if c0 > c1:
            c0, c1 = c1, c0
        e0, e1 = expand(c0), expand(c1)
        palette = [e0, e1, tuple((a + b) // 2 for a, b in zip(e0, e1))]
    else:
        if c0 == c1:
            c1 = c0 - 1 if c0 else 1
            if c1 > c0:
                c0, c1 = c1, c0
        if c0 < c1:
            c0, c1 = c1, c0
        e0, e1 = expand(c0), expand(c1)
        palette = [e0, e1, tuple((2 * a + b) // 3 for a, b in zip(e0, e1)), tuple((a + 2 * b) // 3 for a, b in zip(e0, e1))]
    bits = 0
    for i, p in enumerate(px):
        if transparent and p[3] < 128:
            index = 3
        else:
            index = min(range(len(palette)), key=lambda k: sum((p[j] - palette[k][j]) ** 2 for j in range(3)))
        bits |= index << (2 * i)
    return struct.pack("<HHI", c0, c1, bits)


def dxt1(image):
    w, h = image.size
    px = image.load()
    out = bytearray()
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            out += dxt1_block([px[bx + x, by + y] for y in range(4) for x in range(4)])
    return bytes(out)


def xpr(image, size):
    """XPR0: header (magic, total size, header size), one texture resource (common,
    data offset, lock, format, size), the 0xFFFFFFFF terminator, padding to 2 KB,
    then the texture data."""
    log2 = size.bit_length() - 1
    data = dxt1(image.resize((size, size), Image.LANCZOS))
    header_size = 2048
    resource = struct.pack("<IIIII", 0x00040001, 0, 0, format_word(log2), 0)
    header = b"XPR0" + struct.pack("<II", header_size + len(data), header_size) + resource + struct.pack("<I", 0xFFFFFFFF)
    header += b"\xAD" * (header_size - len(header))
    return header + data


def main():
    icon = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "spiderman/port/OpenSpidey.png"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "xbox/build/release"
    out.mkdir(parents=True, exist_ok=True)
    image = Image.open(icon).convert("RGBA")
    (out / "TitleImage.xbx").write_bytes(xpr(image, 128))
    (out / "SaveImage.xbx").write_bytes(xpr(image, 64))
    image.resize((256, 256), Image.LANCZOS).save(out / "default.tbn", "PNG")
    print("wrote TitleImage.xbx, SaveImage.xbx, default.tbn to " + str(out))


if __name__ == "__main__":
    main()
