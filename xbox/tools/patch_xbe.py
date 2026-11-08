"""Write the title ID (runtime/title.h) into a built XBE's certificate.

    python -I xbox/tools/patch_xbe.py [default.xbe]

nxdk's cxbe always writes title ID 0xFFFF0002, which every nxdk program shares; the
dashboard keys saves and the title's name and picture on it (E:\\UDATA\\<ID>). The XBE
header gives the base address (0x104) and the certificate's address (0x118); the
certificate holds the title ID at +8.
"""
import re
import struct
import sys
from pathlib import Path

XBOX = Path(__file__).resolve().parents[1]


def title_id():
    text = (XBOX / "runtime/title.h").read_text()
    return int(re.search(r"#define OPENSPIDEY_TITLE_ID (0x[0-9A-Fa-f]+)u", text).group(1), 16)


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else XBOX / "build/release/default.xbe"
    data = bytearray(path.read_bytes())
    if data[:4] != b"XBEH":
        raise SystemExit(f"{path}: not an XBE")
    base, = struct.unpack_from("<I", data, 0x104)
    cert, = struct.unpack_from("<I", data, 0x118)
    offset = cert - base
    if not 0 < offset < len(data) - 12:
        raise SystemExit(f"{path}: certificate address 0x{cert:08X} outside the file")
    old, = struct.unpack_from("<I", data, offset + 8)
    new = title_id()
    struct.pack_into("<I", data, offset + 8, new)
    path.write_bytes(bytes(data))
    print(f"{path.name}: title ID 0x{old:08X} -> 0x{new:08X}")


if __name__ == "__main__":
    main()
