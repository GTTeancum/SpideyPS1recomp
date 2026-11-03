#!/usr/bin/env python3
"""Write the Xbox disc's sector map from an install's recompone-disc.json.

    python -I xbox/tools/make_disc_map.py <game folder> <output disc.map>

One line per file: "lba size raw2336 path", plus "data_sectors N". The Xbox CD
library (xbox/runtime/cd.c) reads sectors of the extracted files through it.
"""
import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    manifest = json.loads((Path(sys.argv[1]) / "recompone-disc.json").read_text(encoding="utf-8"))
    lines = [f"data_sectors {manifest.get('dataSectors', 0x7FFFFFFF)}"]
    for f in manifest["files"]:
        path = f["path"].replace("\\", "/")
        if " " in path:
            raise SystemExit(f"path with a space: {path}")
        lines.append(f"{f['lba']} {f['size']} {1 if f.get('raw2336') else 0} {path}")
    Path(sys.argv[2]).write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"{len(manifest['files'])} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
