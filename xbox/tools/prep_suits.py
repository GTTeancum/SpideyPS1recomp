#!/usr/bin/env python3
"""Prepare the PC's mod suits for the Xbox: PNG textures to DXT1 DDS, size capped.

    python -I xbox/tools/prep_suits.py SUITS_DIR OUT_DIR [--max 256] [--min 256] [--only ID ...]

SUITS_DIR is a PC install's mods/suits folder (one folder per suit with suit.json).
OUT_DIR receives suits/<id>/suit.txt and the DDS files, laid out as the Xbox reads
them from the disc (D:\\mods\\suits). This is the asset preparation a Windows
installer for the Xbox build will run.

Why: suit textures are authored for the PC -- 1024x1024 and 2048x1024 PNGs are
common, up to 34 MB per suit unpacked -- while the Xbox has about 5-6 MB spare in a
level at 720p (xbox/README.md, Memory). The NV2A samples DXT1 directly, at 4 bits per texel, and a PlayStation
character is a few hundred pixels tall even at 720p, so textures are halved until
the longest side is 256, but never below 256 on either side (a suit's atlas keeps at
least 256x256): 32 KB for a 256x256 texture against 4 MB for a 1024x1024 PNG
unpacked. Textures already smaller are kept as they are. DXT1's 1-bit alpha carries the cutouts (the PC treats
alpha below one half as transparent).

Suits with their own model (modelFile, the Spider-Man Unlimited conversions) get it
split as the PC's SuitModel.Parse does at load: model.psx is the native actor the
game's loader reads, without the RTG2 retarget rig, and rig.rtg2 is that rig, which
the Xbox evaluates each frame with the PC's retarget core. Only the SMU suits listed
in smu-eligibility.json are converted, as on the PC.

suit.txt lines:  name <text> / model <bundled model> / profile <0..9> /
                 modelfile model.psx / rig rig.rtg2 / texture <id> <file> <w> <h>
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def pow2_floor(n: int) -> int:
    p = 1
    while p * 2 <= n:
        p *= 2
    return p


def fit(image: Image.Image, cap: int, least: int) -> Image.Image:
    """Power-of-two size, halved while the longest side is over `cap` and both sides
    stay at least `least`; aspect kept."""
    w, h = image.size
    w2, h2 = pow2_floor(w), pow2_floor(h)
    while max(w2, h2) > cap and min(w2, h2) // 2 >= least:
        w2, h2 = w2 // 2, h2 // 2
    if (w2, h2) == (w, h):
        return image
    rgb = image.convert("RGB").resize((w2, h2), Image.LANCZOS)
    alpha = image.getchannel("A").resize((w2, h2), Image.LANCZOS)
    rgb.putalpha(alpha)
    return rgb


def to565(c: np.ndarray) -> np.ndarray:
    c = c.astype(np.int32)
    return ((c[..., 0] >> 3) << 11) | ((c[..., 1] >> 2) << 5) | (c[..., 2] >> 3)


def from565(v: np.ndarray) -> np.ndarray:
    r = (v >> 11) & 31
    g = (v >> 5) & 63
    b = v & 31
    return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], axis=-1).astype(np.int32)


def dxt1(image: Image.Image) -> bytes:
    """Encode RGBA to DXT1: endpoints at the extremes of each block's main colour
    axis, 4-colour blocks, or 3 colours plus transparent where any texel is cut out."""
    a = np.asarray(image.convert("RGBA"), dtype=np.int32)
    h, w = a.shape[:2]
    blocks = a.reshape(h // 4, 4, w // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4)
    rgb, alpha = blocks[..., :3], blocks[..., 3]
    clear = alpha < 128
    n = len(blocks)
    # Principal axis of the opaque texels (all texels when a block is fully clear).
    weight = (~clear).astype(np.float64)
    weight[weight.sum(axis=1) == 0] = 1.0
    mean = (rgb * weight[..., None]).sum(axis=1) / weight.sum(axis=1)[:, None]
    centred = (rgb - mean[:, None, :]) * weight[..., None]
    cov = np.einsum("nki,nkj->nij", centred, centred)
    axis = np.ones((n, 3))
    for _ in range(8):  # power iteration
        axis = np.einsum("nij,nj->ni", cov, axis)
        norm = np.linalg.norm(axis, axis=1, keepdims=True)
        axis = np.where(norm > 1e-9, axis / np.maximum(norm, 1e-9), np.array([0.577, 0.577, 0.577]))
    proj = np.einsum("nki,ni->nk", rgb - mean[:, None, :], axis)
    proj_masked_hi = np.where(clear & (weight < 1), -np.inf, proj)
    proj_masked_lo = np.where(clear & (weight < 1), np.inf, proj)
    hi = rgb[np.arange(n), proj_masked_hi.argmax(axis=1)]
    lo = rgb[np.arange(n), proj_masked_lo.argmin(axis=1)]
    c_hi, c_lo = to565(hi), to565(lo)
    any_clear = clear.any(axis=1)
    # 4-colour mode needs c0 > c1; 3-colour + transparent needs c0 <= c1.
    c0 = np.where(any_clear, np.minimum(c_hi, c_lo), np.maximum(c_hi, c_lo))
    c1 = np.where(any_clear, np.maximum(c_hi, c_lo), np.minimum(c_hi, c_lo))
    equal = (c0 == c1) & ~any_clear
    c1 = np.where(equal & (c0 > 0), c0 - 1, c1)          # keep 4-colour mode legal
    c0 = np.where(equal & (c0 == 0), 1, c0)
    e0, e1 = from565(c0), from565(c1)
    four = c0 > c1
    pal = np.stack([e0, e1,
                    np.where(four[:, None], (2 * e0 + e1) // 3, (e0 + e1) // 2),
                    np.where(four[:, None], (e0 + 2 * e1) // 3, 0)], axis=1)
    dist = ((rgb[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(axis=-1)
    dist[~four, :, 3] = np.iinfo(np.int32).max // 2      # index 3 is transparent in 3-colour mode
    index = dist.argmin(axis=2)
    index = np.where(clear, 3, index)
    bits = (index.astype(np.uint32) << (2 * np.arange(16, dtype=np.uint32))).sum(axis=1).astype(np.uint32)
    out = np.zeros(n, dtype=[("c0", "<u2"), ("c1", "<u2"), ("bits", "<u4")])
    out["c0"], out["c1"], out["bits"] = c0, c1, bits
    return out.tobytes()


def dds(width: int, height: int, data: bytes) -> bytes:
    """A DDS file holding one DXT1 surface, no mipmaps."""
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000       # caps, height, width, pixelformat, linearsize
    header = struct.pack("<4sIIIIIII44x", b"DDS ", 124, flags, height, width, len(data), 0, 1)
    pixel_format = struct.pack("<II4sIIIII", 32, 0x4, b"DXT1", 0, 0, 0, 0, 0)
    caps = struct.pack("<IIIII", 0x1000, 0, 0, 0, 0)
    return header + pixel_format + caps + data


def read_manifest(path: Path) -> dict:
    """suit.json allows // and /* */ comments, as the PC's reader (SuitManifest) skips them."""
    text, out, i, quoted = path.read_text(encoding="utf-8"), [], 0, False
    while i < len(text):
        ch = text[i]
        if quoted:
            out.append(ch)
            if ch == "\\" and i + 1 < len(text):
                out.append(text[i + 1]); i += 1
            elif ch == '"':
                quoted = False
        elif ch == '"':
            quoted = True; out.append(ch)
        elif text.startswith("//", i):
            while i < len(text) and text[i] != "\n":
                i += 1
            continue
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            i = len(text) if end < 0 else end + 2
            continue
        else:
            out.append(ch)
        i += 1
    return json.loads("".join(out))


# SuitManifest: bundled models a suit may name, and the stock power profiles.
MODELS = ("spiderman", "scarlet-spider", "symbiote", "quick-change", "peter-parker", "sm2-spiderman")
PROFILES = ("spiderman", "2099", "symbiote", "captain-universe", "unlimited",
            "bagman", "scarlet", "ben-reilly", "quick-change", "peter-parker")
RTG2 = 0x32475452


def u32(b: bytes, p: int) -> int:
    return struct.unpack_from("<I", b, p)[0]


def u16(b: bytes, p: int) -> int:
    return struct.unpack_from("<H", b, p)[0]


def split_model(data: bytes) -> tuple[bytes, bytes | None]:
    """SuitModel.Parse's layout walk: return the guest actor (RTG2 removed, texture
    pointers moved down) and the rig. Raises on anything the PC would reject."""
    if not 1024 <= len(data) <= 4 * 1024 * 1024 or u16(data, 0) != 4 or u16(data, 2) != 2 or u32(data, 8) != 18:
        raise ValueError("not a native v4 18-part Spider-Man actor")
    table = 12 + 18 * 36
    meshes = u32(data, table)
    if not 18 <= meshes <= 64:
        raise ValueError("mesh count")
    p = u32(data, 4)
    rig_start = rig_size = -1
    rig = None
    for _ in range(65):
        tag = u32(data, p); p += 4
        if tag == 0xFFFFFFFF:
            break
        size = u32(data, p); p += 4
        if tag == RTG2:
            if rig is not None:
                raise ValueError("duplicate RTG2")
            rig_start, rig_size, rig = p - 8, size + 8, data[p:p + size]
        p += size
    else:
        raise ValueError("too many metadata blocks")
    if meshes > 18 and rig is None:
        raise ValueError("paged actor without a rig")
    p += meshes * 4
    hashes = u32(data, p); p += 4 + hashes * 4
    palette4 = u32(data, p); p += 4 + palette4 * 36
    palette8 = u32(data, p); p += 4 + palette8 * 516
    if u32(data, p) == 0xFFFFFFFF:
        p += 4
        details = u32(data, p); p += 4 + details * 36
        cubes = u32(data, p); p += 4 + cubes * 36
    textures = u32(data, p); p += 4
    if textures > 128:
        raise ValueError("texture count")
    if rig is None:
        return data, None
    guest = bytearray(data[:rig_start] + data[rig_start + rig_size:])
    for i in range(textures):
        pointer = u32(data, p + i * 4)
        if pointer < rig_start + rig_size:
            raise ValueError("texture overlaps RTG2")
        struct.pack_into("<I", guest, p - rig_size + i * 4, pointer - rig_size)
    if len(guest) > 1024 * 1024:
        raise ValueError("guest actor over 1 MiB")
    return bytes(guest), rig


def smu_eligible(suits: Path) -> set[str] | None:
    path = suits / "smu-eligibility.json"
    return set(json.loads(path.read_text(encoding="utf-8"))["eligibleIds"]) if path.exists() else None


def convert(suit: Path, out: Path, cap: int, least: int, eligible: set[str] | None) -> str:
    manifest = read_manifest(suit / "suit.json")
    sid = manifest["id"]
    if sid.startswith("smu-") and (eligible is None or sid not in eligible):
        return f"skipped {sid}: outside the approved SMU suits"
    model = manifest.get("model", "spiderman")
    if model not in MODELS:
        return f"skipped {sid}: unknown model {model}"
    profile = manifest.get("abilities", {}).get("profile", "spiderman")
    if profile not in PROFILES:
        return f"skipped {sid}: unknown ability profile {profile}"
    target = out / "suits" / sid
    target.mkdir(parents=True, exist_ok=True)
    lines = [f"name {manifest['name']}", f"model {model}", f"profile {PROFILES.index(profile)}"]
    if manifest.get("modelFile"):
        guest, rig = split_model((suit / manifest["modelFile"]).read_bytes())
        (target / "model.psx").write_bytes(guest)
        lines.append("modelfile model.psx")
        if rig is not None:
            (target / "rig.rtg2").write_bytes(rig)
            lines.append("rig rig.rtg2")
    total = 0
    for key, rel in sorted(manifest["textures"].items()):
        image = fit(Image.open(suit / rel).convert("RGBA"), cap, least)
        data = dxt1(image)
        name = f"{key.upper()}.dds"
        (target / name).write_bytes(dds(image.width, image.height, data))
        lines.append(f"texture {key.upper()} {name} {image.width} {image.height}")
        total += len(data)
    (target / "suit.txt").write_text("\n".join(lines) + "\n", encoding="ascii")
    own = " + own model" if manifest.get("modelFile") else ""
    return f"{sid}: {len(manifest['textures'])} textures, {total // 1024} KB{own}"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("suits", type=Path)
    p.add_argument("out", type=Path)
    p.add_argument("--max", type=int, default=256, help="longest texture side (power of two)")
    p.add_argument("--min", type=int, default=256, help="no side is halved below this")
    p.add_argument("--only", nargs="*", help="suit ids to convert (default: all)")
    a = p.parse_args()
    eligible = smu_eligible(a.suits)
    for suit in sorted(d for d in a.suits.iterdir() if (d / "suit.json").exists()):
        if a.only and suit.name not in a.only:
            continue
        try:
            print(convert(suit, a.out, a.max, a.min, eligible))
        except (ValueError, struct.error) as error:
            print(f"rejected {suit.name}: {error}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
