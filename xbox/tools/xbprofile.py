#!/usr/bin/env python3
"""Sample where OpenSpidey's game thread spends its time on a development console.

    python -I xbox/tools/xbprofile.py HOST [SAMPLES] [THREAD]

Suspends the thread, reads its stack, takes the interrupted position from the interrupt
code (the innermost function of ours it was in; the context's own EIP is the kernel
point where the thread is parked) and names it from the build's link map (xbdm.map_path). Prints the
functions by share of samples.
"""
from __future__ import annotations

import bisect
import collections
import re
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from xbdm import Xbdm, getmem, map_path  # noqa: E402


def symbols() -> tuple[list[int], list[str], int, int]:
    text = map_path().read_text(errors="replace")
    syms = sorted((int(m[2], 16), m[1]) for m in re.finditer(r"^\s*0001:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})\s", text, re.M))
    lo = syms[0][0]
    hi = syms[-1][0] + 0x10000
    return [a for a, _ in syms], [n for _, n in syms], lo, hi


def code_reader():
    """Bytes of the linked image's code (xbox/main.exe), to tell return addresses from
    other words: a return address follows a call instruction."""
    image = (Path(__file__).resolve().parents[1] / "main.exe").read_bytes()
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    sections, opt = struct.unpack_from("<H", image, pe + 6)[0], struct.unpack_from("<H", image, pe + 20)[0]
    base = struct.unpack_from("<I", image, pe + 24 + 28)[0]
    spans = []
    for s in range(sections):
        o = pe + 24 + opt + s * 40
        size, rva, raw_size, raw = struct.unpack_from("<IIII", image, o + 8)
        spans.append((base + rva, min(size, raw_size), raw))

    def read(address: int, n: int) -> bytes:
        for start, size, raw in spans:
            if start <= address and address + n <= start + size:
                return image[raw + address - start: raw + address - start + n]
        return b""
    return read


_code = None


def after_call(w: int) -> bool:
    global _code
    if _code is None:
        _code = code_reader()
    b = _code(w - 7, 7)
    if len(b) < 7:
        return False
    # call rel32 (E8), or call r/m32 (FF /2) of 2, 3, 6 or 7 bytes.
    return b[2] == 0xE8 or any(b[7 - n] == 0xFF and (b[8 - n] >> 3) & 7 == 2 for n in (2, 3, 6, 7))


def main() -> int:
    host = sys.argv[1]
    samples = int(sys.argv[2]) if len(sys.argv) > 2 else 300
    addrs, names, lo, hi = symbols()
    x = Xbdm(host)
    threads = x.multiline("threads")
    thread = sys.argv[3] if len(sys.argv) > 3 else threads[0]
    counts = collections.Counter()
    callers = collections.Counter()
    taken = 0
    for _ in range(samples):
        x.cmd(f"suspend thread={thread}")
        try:
            ctx = dict(kv.split("=") for kv in x.multiline(f"getcontext thread={thread} control"))
            esp = int(ctx["Esp"], 16)
            stack = getmem(x, esp, 0x800)
        finally:
            x.cmd(f"resume thread={thread}")
        words = [w for (w,) in struct.iter_unpack("<I", stack[: len(stack) // 4 * 4])]
        found = []
        # The interrupted position: the processor's interrupt frame (EIP, CS 8, EFLAGS
        # with bit 1 set). Words that merely look like code addresses (stale return
        # addresses, function pointers) come before it on a parked thread's stack.
        for i in range(len(words) - 2):
            if lo <= words[i] < hi and words[i + 1] == 8 and words[i + 2] & 0xFFFF0002 == 2:
                found = [words[i]] + [w for w in words[i + 3:] if lo <= w < hi and after_call(w)]
                break
        if not found:
            found = [w for w in words if lo <= w < hi and after_call(w)]
        if found:
            name = names[bisect.bisect_right(addrs, found[0]) - 1]
            counts[name] += 1
            if len(found) > 1:
                callers[name + " <- " + names[bisect.bisect_right(addrs, found[1]) - 1]] += 1
            taken += 1
        time.sleep(0.01)
    print(f"{taken} samples, thread {thread}")
    for name, n in counts.most_common(30):
        print(f"{100 * n / taken:5.1f}%  {name}")
    print("-- with caller --")
    for name, n in callers.most_common(15):
        print(f"{100 * n / taken:5.1f}%  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
