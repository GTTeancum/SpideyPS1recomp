#!/usr/bin/env python3
"""A small client for the Xbox debug monitor (XBDM, TCP 731) on a development console.

    python -I xbox/tools/xbdm.py HOST ls 'E:\\Games'
    python -I xbox/tools/xbdm.py HOST put LOCAL_DIR 'E:\\Games\\OpenSpidey'
    python -I xbox/tools/xbdm.py HOST run 'E:\\Games\\OpenSpidey\\default.xbe'
    python -I xbox/tools/xbdm.py HOST shot OUT.png
    python -I xbox/tools/xbdm.py HOST get 'E:\\...\\file' LOCAL_FILE

Written from the debug monitor's text protocol (status lines "2xx- ...", multi-line
responses ending in "."); no SDK code. put copies only files that differ in size,
so re-installing after a build sends the XBE and little else.
"""
from __future__ import annotations

import socket
import struct
import sys
from pathlib import Path


def map_path() -> Path:
    """The running build's link map: OPENSPIDEY_MAP, else Spider-Man 1's
    (build/release/OpenSpidey.map; Spider-Man 2's is build/sm2/OpenSpidey.map)."""
    import os
    env = os.environ.get("OPENSPIDEY_MAP")
    return Path(env) if env else Path(__file__).resolve().parents[1] / "build" / "release" / "OpenSpidey.map"


class Xbdm:
    def __init__(self, host: str, timeout: float = 15.0):
        self.s = socket.create_connection((host, 731), timeout=timeout)
        self.s.settimeout(timeout)
        self.buf = b""
        self.line()  # 201- connected

    def line(self) -> str:
        while b"\r\n" not in self.buf:
            chunk = self.s.recv(65536)
            if not chunk:
                raise ConnectionError("debug monitor closed the connection")
            self.buf += chunk
        out, self.buf = self.buf.split(b"\r\n", 1)
        return out.decode("latin-1")

    def read(self, n: int) -> bytes:
        while len(self.buf) < n:
            chunk = self.s.recv(max(65536, n - len(self.buf)))
            if not chunk:
                raise ConnectionError("debug monitor closed the connection")
            self.buf += chunk
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def cmd(self, text: str) -> str:
        self.s.sendall(text.encode("latin-1") + b"\r\n")
        return self.line()

    def multiline(self, text: str) -> list[str]:
        status = self.cmd(text)
        if not status.startswith("202"):
            raise RuntimeError(f"{text}: {status}")
        lines = []
        while (l := self.line()) != ".":
            lines.append(l)
        return lines

    def dirlist(self, path: str) -> list[dict]:
        out = []
        for l in self.multiline(f'dirlist name="{path}"'):
            fields = {}
            for part in l.split(" "):
                if "=" in part:
                    k, v = part.split("=", 1)
                    fields[k] = v.strip('"')
                elif part:
                    fields[part] = True
            if "name" in fields:
                fields["size"] = (int(fields.get("sizehi", "0"), 16) << 32) | int(fields.get("sizelo", "0"), 16)
                out.append(fields)
        return out

    def mkdir(self, path: str) -> None:
        r = self.cmd(f'mkdir name="{path}"')
        if not r.startswith("200") and "exists" not in r.lower() and not r.startswith("410"):
            raise RuntimeError(f"mkdir {path}: {r}")

    def sendfile(self, local: Path, remote: str) -> None:
        data = local.read_bytes()
        r = self.cmd(f'sendfile name="{remote}" length=0x{len(data):x}')
        if not r.startswith("204"):
            raise RuntimeError(f"sendfile {remote}: {r}")
        self.s.sendall(data)
        r = self.line()
        if not r.startswith("200"):
            raise RuntimeError(f"sendfile {remote}: {r}")

    def getfile(self, remote: str) -> bytes:
        r = self.cmd(f'getfile name="{remote}"')
        if not r.startswith("203"):
            raise RuntimeError(f"getfile {remote}: {r}")
        n = struct.unpack("<I", self.read(4))[0]
        return self.read(n)

    def run(self, xbe: str) -> str:
        folder, name = xbe.rsplit("\\", 1)
        return self.cmd(f'magicboot title="{xbe}" debug')

    def screenshot(self) -> tuple[int, int, int, bytes]:
        r = self.cmd("screenshot")
        if not r.startswith("203"):
            raise RuntimeError(f"screenshot: {r}")
        info = {}
        for part in self.line().split(" "):
            if "=" in part:
                k, v = part.split("=", 1)
                info[k] = int(v.rstrip(","), 16)
        return info["width"], info["height"], info["pitch"], self.read(info["framebuffersize"]), info.get("format", 0)


def getmem(x: Xbdm, address: int, length: int) -> bytes:
    """Console memory, in 64 KB pieces (getmem answers in hex; ?? for unreadable)."""
    out = bytearray()
    for at in range(address, address + length, 0x10000):
        n = min(0x10000, address + length - at)
        text = "".join(x.multiline(f"getmem addr=0x{at:x} length=0x{n:x}"))
        out += bytes.fromhex(text.replace("?", "0"))
    return bytes(out)


def put_tree(x: Xbdm, local: Path, remote: str) -> tuple[int, int]:
    sent = skipped = 0
    try:
        existing = {e["name"].lower(): e for e in x.dirlist(remote)}
    except RuntimeError:
        x.mkdir(remote)
        existing = {}
    for item in sorted(local.iterdir()):
        target = remote + "\\" + item.name
        if item.is_dir():
            if item.name.lower() not in existing:
                x.mkdir(target)
            s, k = put_tree(x, item, target)
            sent += s; skipped += k
        else:
            e = existing.get(item.name.lower())
            if e and e["size"] == item.stat().st_size and item.name.lower() != "default.xbe":
                skipped += 1
                continue
            x.sendfile(item, target)
            sent += 1
            print(f"  {target} ({item.stat().st_size // 1024} KB)", flush=True)
    return sent, skipped


def main() -> int:
    host, op, *args = sys.argv[1:]
    x = Xbdm(host)
    if op == "ls":
        for e in x.dirlist(args[0]):
            print(("<dir> " if "directory" in e else f"{e['size']:>10} ") + e["name"])
    elif op == "put":
        sent, skipped = put_tree(x, Path(args[0]), args[1])
        print(f"sent {sent}, unchanged {skipped}")
    elif op == "run":
        print(x.run(args[0]))
    elif op == "get":
        Path(args[1]).write_bytes(x.getfile(args[0]))
    elif op == "cmd":
        print(x.cmd(" ".join(args)))
    elif op in ("status", "frame"):
        # OpenSpidey's status text and last shown frame (the console's own screenshot
        # cannot find a picture drawn without D3D). Address from the link map.
        import re
        m = re.search(r"_g_status_text\s+([0-9a-fA-F]{8,16})\s",
                      map_path().read_text(errors="replace"))
        # The game rewrites the text about once a second; a read that overlaps a
        # rewrite comes back cut short, so read until the copy is whole.
        for _ in range(5):
            text = getmem(x, int(m[1], 16), 0x4000).split(b"\0")[0].decode("latin-1")
            if text.startswith("OPENSPIDEY-STATUS") and "recent calls:" in text:
                break
        if op == "status":
            print(text)
        else:
            shown = re.search(r"^shown ([0-9A-F]{8}) (\d+)x(\d+)", text, re.M)
            address, w, h = int(shown[1], 16), int(shown[2]), int(shown[3])
            from PIL import Image
            # Freeze the console while copying, or the game redraws the buffer under
            # the copy (about 3 s) and the grab mixes frames.
            x.cmd("stop")
            # The GPU keeps running queued commands; let it finish the frame being read.
            import time
            time.sleep(0.3)
            try:
                data = getmem(x, address, w * h * 4)
            finally:
                x.cmd("go")
            Image.frombuffer("RGBX", (w, h), data, "raw", "BGRX", w * 4, 1).convert("RGB").save(args[0])
            print(f"{w}x{h} from 0x{address:08X}")
    elif op == "screen":
        # The picture the video chip is sending to the TV: the scan-out start register
        # (PCRTC_START) holds the physical address of the front buffer.
        import re
        from PIL import Image
        m = re.search(r"_g_status_text\s+([0-9a-fA-F]{8,16})\s",
                      map_path().read_text(errors="replace"))
        text = getmem(x, int(m[1], 16), 0x4000).split(b"\0")[0].decode("latin-1")
        mode = re.search(r"^mode (\d+)x(\d+)", text, re.M)
        w, h = int(mode[1]), int(mode[2])
        x.cmd("stop")
        try:
            start = struct.unpack("<I", getmem(x, 0xFD600800, 4))[0] & 0x03FFFFFF
            data = getmem(x, 0x80000000 | start, w * h * 4)
        finally:
            x.cmd("go")
        Image.frombuffer("RGBX", (w, h), data, "raw", "BGRX", w * 4, 1).convert("RGB").save(args[0])
        print(f"{w}x{h} front buffer at 0x{start:08X}")
    elif op == "shot":
        from PIL import Image
        w, h, pitch, data, fmt = x.screenshot()
        img = Image.frombuffer("RGBX", (w, h), data, "raw", "BGRX", pitch, 1).convert("RGB")
        img.save(args[0])
        print(f"{w}x{h} format 0x{fmt:x}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
