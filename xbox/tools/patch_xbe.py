"""Write the title identity (runtime/title.h) and the dashboard pictures into a built XBE.

    python -I xbox/tools/patch_xbe.py [default.xbe]

1. Certificate. nxdk's cxbe always writes title ID 0xFFFF0002, which every nxdk program
   shares; the dashboard keys saves and the title's name and picture on it
   (E:\\UDATA\\<ID>). The XBE header gives the base address (0x104) and the
   certificate's address (0x118); the certificate holds the title ID at +8 and the
   title name (40 UTF-16 characters, what launchers list) at +0xC. The name is written
   here rather than through cxbe's -TITLE, which the nxdk makefile passes unquoted (no
   spaces).

2. Inserted-file sections, as the XDK's image builder adds them (/TITLEINFO,
   /TITLEIMAGE, /DEFAULTSAVEIMAGE), which cxbe cannot: $$XTINFO (the title name as
   text), $$XTIMAGE (TitleImage.xbx, the 128x128 icon) and $$XSIMAGE (SaveImage.xbx,
   64x64), read from beside the XBE (tools/make_xbox_images.py makes them). Launchers
   and emulators show the icon from $$XTIMAGE, not from loose files. The method is
   Perfect Dark X's scripts/insert-xbe-dashboard-assets.py: the section table moves
   into the free header space after the used header (refused unless that space is
   all zero and ends before the first section's data), the three payloads are
   appended on page boundaries with flags 0x38 (inserted file, head and tail pages
   read-only; not preloaded) and their SHA-1 digests, and the header's sizes, count
   and table address are updated. A build whose XBE already carries the sections (make
   did not relink) is left as it is.
"""
import hashlib
import re
import struct
import sys
from pathlib import Path

XBOX = Path(__file__).resolve().parents[1]
SECTION = 56            # bytes per section header
INSERTED = 0x38         # inserted file | head page read-only | tail page read-only


def title_h():
    return (XBOX / "runtime/title.h").read_text()


def title_id():
    return int(re.search(r"#define OPENSPIDEY_TITLE_ID (0x[0-9A-Fa-f]+)u", title_h()).group(1), 16)


def title_name():
    return re.search(r'#define OPENSPIDEY_TITLE_NAME "([^"]*)"', title_h()).group(1)


def align(v, a):
    return (v + a - 1) & ~(a - 1)


def u32(data, at):
    return struct.unpack_from("<I", data, at)[0]


def patch_certificate(path, data):
    base, cert = u32(data, 0x104), u32(data, 0x118)
    offset = cert - base
    if not 0 < offset < len(data) - 0x5C:
        raise SystemExit(f"{path}: certificate address 0x{cert:08X} outside the file")
    old = u32(data, offset + 8)
    struct.pack_into("<I", data, offset + 8, title_id())
    name = title_name()
    encoded = name.encode("utf-16-le")
    if len(encoded) > 0x50:
        raise SystemExit(f"title name longer than 40 characters: {name}")
    data[offset + 0xC:offset + 0x5C] = encoded.ljust(0x50, b"\0")
    print(f"{path.name}: title ID 0x{old:08X} -> 0x{title_id():08X}, name {name}")


def section_names(data):
    base, count, table = u32(data, 0x104), u32(data, 0x11C), u32(data, 0x120) - u32(data, 0x104)
    names = []
    for i in range(count):
        at = u32(data, table + i * SECTION + 20) - base
        names.append(bytes(data[at:data.index(b"\0", at)]).decode("latin-1"))
    return names


def xpr(path, size):
    payload = path.read_bytes()
    if len(payload) != size or payload[:4] != b"XPR0":
        raise SystemExit(f"{path}: not the {size}-byte XPR0 picture make_xbox_images.py writes")
    return payload


def insert_pictures(path, data):
    if "$$XTIMAGE" in section_names(data):
        print(f"{path.name}: dashboard sections already present")
        return
    resources = (
        ("$$XTINFO", f"[default]\r\nTitleName={title_name()}\r\n".encode("ascii")),
        ("$$XTIMAGE", xpr(path.parent / "TitleImage.xbx", 10240)),
        ("$$XSIMAGE", xpr(path.parent / "SaveImage.xbx", 4096)),
    )
    base, header_size, image_size = u32(data, 0x104), u32(data, 0x108), u32(data, 0x10C)
    count, table = u32(data, 0x11C), u32(data, 0x120) - base
    table_end = table + count * SECTION
    if table < 0x178 or table_end > header_size:
        raise SystemExit(f"{path}: section table outside the used header")
    headers = bytes(data[table:table_end])
    first_raw = min(r for r in (u32(headers, i * SECTION + 12) for i in range(count)) if r)

    new_count = count + len(resources)
    new_table = align(header_size, 16)
    cursor = new_table + new_count * SECTION
    name_at = []
    for name, _ in resources:
        name_at.append(cursor)
        cursor += len(name) + 1
    refs = align(cursor, 2)
    refs_size = (len(resources) + 1) * 2
    new_header_size = align(refs + refs_size, 4)
    # Bounds: the new table, names and page references go only into header space that
    # is free (all zero) and ends before the first section's data.
    if new_header_size > first_raw:
        raise SystemExit(f"{path}: new header end 0x{new_header_size:X} reaches section data at 0x{first_raw:X}")
    if any(data[new_table:new_header_size]):
        raise SystemExit(f"{path}: header space after 0x{new_table:X} is in use; refusing to write there")

    data[new_table:new_table + len(headers)] = headers
    for at, (name, _) in zip(name_at, resources):
        data[at:at + len(name) + 1] = name.encode("ascii") + b"\0"
    data[refs:refs + refs_size] = bytes(refs_size)

    data.extend(bytes(align(len(data), 0x1000) - len(data)))
    virtual = base + align(image_size, 0x1000)
    for i, ((name, payload), at) in enumerate(zip(resources, name_at)):
        raw = len(data)
        struct.pack_into("<9I20s", data, new_table + (count + i) * SECTION,
                         INSERTED, virtual, len(payload), raw, len(payload), base + at, 0,
                         base + refs + i * 2, base + refs + (i + 1) * 2, hashlib.sha1(payload).digest())
        data.extend(payload)
        data.extend(bytes(align(len(data), 0x1000) - len(data)))
        virtual = align(virtual + len(payload), 0x20)

    struct.pack_into("<I", data, 0x108, new_header_size)
    struct.pack_into("<I", data, 0x10C, virtual - base)
    struct.pack_into("<I", data, 0x11C, new_count)
    struct.pack_into("<I", data, 0x120, base + new_table)
    print(f"{path.name}: inserted " + ", ".join(f"{n} {len(p)}" for n, p in resources))


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else XBOX / "build/release/default.xbe"
    data = bytearray(path.read_bytes())
    if data[:4] != b"XBEH":
        raise SystemExit(f"{path}: not an XBE")
    patch_certificate(path, data)
    insert_pictures(path, data)
    path.write_bytes(bytes(data))


if __name__ == "__main__":
    main()
