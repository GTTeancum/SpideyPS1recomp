"""Convert selected classic Spider-Man 2000 texture mods into suit packages."""

import argparse
import hashlib
import io
import json
from pathlib import Path
import zipfile

from PIL import Image


MATERIALS = {
    "00059588": "9D39C02B",
    "0005DDAC": "29AF154F",
    "000625D0": "154B7D71",
    "00066DF4": "42991273",
    "00068618": "D7669388",
    "00069E3C": "D8425644",
    "0006E660": "F82D1471",
    "0006FE84": "FBC5A5A0",
}

SPECS = {
    "negative-zone": {
        "archive": "negative_zone.zip",
        "md5": "fb282db03015bf73d6b21691f4aa3aee",
        "file_id": 530830,
        "name": "Negative Zone",
        "prefix": "",
        "source": "https://gamebanana.com/mods/249025",
        "source_name": "Negative Zone Costume",
    },
    "americas-spider": {
        "archive": "captain_america_over_spider_man.zip",
        "md5": "c3707bc97c0e6bcb6d8619314fead4a1",
        "file_id": 587158,
        "name": "America's Spider",
        "prefix": "With Spider-Man Mask/",
        "source": "https://gamebanana.com/mods/293310",
        "source_name": "Captain America Spider-Man",
    },
    "1967-tv-series": {
        "archive": "1967spideyv1.zip",
        "md5": "32c893f4f3e619e65cb1560e6c056b30",
        "file_id": 685802,
        "name": "1967 TV Series",
        "prefix": "",
        "source": "https://gamebanana.com/mods/332983",
        "source_name": "1967 TV Series Costume",
    },
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suit", choices=sorted(SPECS), required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    spec = SPECS[args.suit]

    digest = hashlib.md5(args.archive.read_bytes()).hexdigest()
    if digest != spec["md5"]:
        raise ValueError(
            f"Expected {spec['archive']}, GameBanana file {spec['file_id']} "
            f"with MD5 {spec['md5']}; found {digest}"
        )
    if args.output.exists():
        raise ValueError("Use a fresh output directory")
    (args.output / "textures").mkdir(parents=True)

    textures = {}
    with zipfile.ZipFile(args.archive) as archive:
        for offset, material in MATERIALS.items():
            member = f"{spec['prefix']}{offset}.bmp"
            original = Image.open(io.BytesIO(archive.read(member))).convert("RGB")
            relative = f"textures/{material}.png"
            original.save(args.output / relative)
            with Image.open(args.output / relative) as converted:
                if converted.size != original.size or converted.convert("RGB").tobytes() != original.tobytes():
                    raise ValueError(f"Pixel mismatch: {member}")
            textures[material] = relative

    manifest = {
        "version": 1,
        "id": args.suit,
        "name": spec["name"],
        "comments": "By Dat Mental Gamer",
        "model": "spiderman",
        "abilities": {"profile": "spiderman"},
        "textures": textures,
    }
    (args.output / "suit.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (args.output / "README.md").write_text(
        f"# {spec['name']}\n\n"
        "By Dat Mental Gamer.\n\n"
        f"Source: {spec['source']} ({spec['source_name']}).\n"
        "Original eight BMP textures converted losslessly to PNG, with no resizing\n"
        "or recoloring. Uses the wingless SM1 body in both games and standard\n"
        "Spider-Man powers. Install this folder in mods/suits.\n",
        encoding="utf-8",
    )
    instructions = Path(__file__).resolve().parents[2] / "spiderman" / "port" / "legacy-suits" / "steve-ditko" / "instructions.txt"
    instruction_text = instructions.read_text(encoding="utf-8").replace(
        "The selector supports 60 total costumes: 40 mod slots in SM1, 41 in SM2.",
        "The selector supports 253 total costumes: 233 mod slots in SM1, 234 in SM2.",
    )
    (args.output / "instructions.txt").write_text(instruction_text, encoding="utf-8")
    print(f"{args.output}: eight textures; source RGB pixels verified")


if __name__ == "__main__":
    main()
