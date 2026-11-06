#!/usr/bin/env python3
"""Apply SM1's generated-source transforms to the Xbox's C output.

    python -I xbox/tools/patch_generated_sm1.py [xbox/generated/spiderman]

Run after RecompOne's --c output and before building. The C counterpart of
spiderman/tools/patch_texture_registry.py (which transforms the PC's C# output); a
change to either belongs in both.

Texture registry: retail stores 8-byte texture records at 0x80099A18, and only 17 fit
before the animation-event table, so Dreamcast and Spider-Man Unlimited actors (which
register more) overwrite animation data. The five uses of the retail base move to the
recomp-only table at 0x80780000 (TextureRegistry.Table on the PC). And retail registers
a model's texture pages only when their hashes are among the 13 built-in Spider-Man
materials; converted and mod actors have other hashes, so the player's model slots
(2 in the shell, 12 in gameplay) register every texture.

Idempotent: an already transformed file is left alone.
"""
import sys
from pathlib import Path

TABLE = "0x80780000u /* SM1 texture registry, relocated */"
MARK = "SM1 texture registry, relocated"


def replace_exact(text: str, old: str, new: str, expected: int) -> str:
    count = text.count(old)
    if count != expected:
        raise SystemExit(f"expected {expected} occurrence(s) of {old!r}, found {count}")
    return text.replace(old, new)


def function(text: str, name: str) -> tuple[int, int]:
    start = text.index(f"void SpiderMan_{name}(CpuContext *c)\n{{")
    end = text.index("\n}\n", start) + 3
    return start, end


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "generated" / "spiderman")
    path = root / "SpiderMan_main.c"
    text = path.read_text(encoding="utf-8")
    if MARK in text:
        print("texture registry: already transformed")
        return 0

    # One writer and two readers construct the retail base five times.
    text = replace_exact(text, "c->r[7] = c->r[7] - 0x65E8u;", f"c->r[7] = {TABLE};", 1)
    text = replace_exact(text, "c->r[5] = mem_r32((c->r[4] - 0x65E8u));", f"c->r[5] = mem_r32({TABLE});", 1)
    text = replace_exact(text, "c->r[17] = c->r[4] - 0x65E8u;", f"c->r[17] = {TABLE};", 1)
    text = replace_exact(text, "c->r[16] = mem_r32((c->r[5] - 0x65E8u));", f"c->r[16] = mem_r32({TABLE});", 1)
    text = replace_exact(text, "c->r[19] = c->r[5] - 0x65E8u;", f"c->r[19] = {TABLE};", 1)

    # Register every texture of the player model slots (TextureRegistry.IsPlayerModelSlot).
    start, end = function(text, "func_80068BB0_Impl")  # the hooked function's body
    body = replace_exact(
        text[start:end],
        "        c->r[30] = c->r[6] + 0x14u;\n        c->r[2] = c->r[2] < c->r[3] ? 1u : 0u;\n",
        "        c->r[30] = c->r[6] + 0x14u;\n"
        "        if (mem_r32(c->r[29] + 0x78u) == 2u || mem_r32(c->r[29] + 0x78u) == 12u) /* player model slot */\n"
        "            goto L80069040;\n"
        "        c->r[2] = c->r[2] < c->r[3] ? 1u : 0u;\n",
        1,
    )
    text = text[:start] + body + text[end:]
    path.write_text(text, encoding="utf-8", newline="\n")
    print("texture registry: 17 -> 32768 records at 0x80780000; player slots register every texture")
    return 0


if __name__ == "__main__":
    sys.exit(main())
