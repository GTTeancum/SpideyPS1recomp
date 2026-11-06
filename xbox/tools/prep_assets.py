#!/usr/bin/env python3
"""Prepare the PC's built-in assets for the Xbox disc.

    python -I xbox/tools/prep_assets.py BUILTIN_DIR OUT_DIR [--max 512]

BUILTIN_DIR is a PC install's assets/builtin. OUT_DIR receives, as the Xbox reads
them from the disc (D:\\assets):

  builtin/<name>.psx and builtin/index.txt   the Dreamcast character actors, served in
                                             place of the game's own (LooseWadOverrides)
  packs/<pack>/<key>.dds and index.txt       each pack's textures as DXT1, keyed by the
                                             fingerprint the PC pack uses

This is the asset preparation a Windows installer for the Xbox build will run. The
Dreamcast actors carry compact PlayStation textures that only identify their art; the
pack (dreamcast-sm1-actors) is the art, 589 textures, most 64x64 to 256x256.
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from prep_suits import dds, dxt1, fit  # noqa: E402  (same DXT1 encoder and size rule)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("builtin", type=Path)
    p.add_argument("out", type=Path)
    p.add_argument("--max", type=int, default=512, help="longest texture side (power of two)")
    a = p.parse_args()

    actors = a.out / "builtin"
    actors.mkdir(parents=True, exist_ok=True)
    names = sorted(f.name for f in a.builtin.glob("*.psx"))
    for name in names:
        shutil.copy2(a.builtin / name, actors / name.lower())
    (actors / "index.txt").write_text("\n".join(n.lower() for n in names) + "\n", encoding="ascii")
    print(f"actors: {len(names)}")

    for pack in sorted(d for d in (a.builtin / "packs").iterdir() if (d / "textures").is_dir()):
        target = a.out / "packs" / pack.name
        target.mkdir(parents=True, exist_ok=True)
        keys, total = [], 0
        for png in sorted((pack / "textures").glob("*.png")):
            key = png.stem.lower()
            if len(key) != 33 or key[16] != "_":
                continue
            image = fit(Image.open(png).convert("RGBA"), a.max)
            data = dxt1(image)
            (target / f"{key}.dds").write_bytes(dds(image.width, image.height, data))
            keys.append(key)
            total += len(data)
        (target / "index.txt").write_text("\n".join(keys) + "\n", encoding="ascii")
        print(f"pack {pack.name}: {len(keys)} textures, {total // 1024} KB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
