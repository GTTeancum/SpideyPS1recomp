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

Vblank countdown: four loops spin on the counter at 0x800B5464, which the vblank
interrupt counts down (one runs when Spider-Man is knocked out in level 3). Here
interrupts arrive only with a 60 Hz tick, and a loop that only reads memory never
delivers one, so each loop calls rt_idle (the PC catches the same loops by counting
memory reads, PSMemory.TrackRead).

Costume viewer (the PC's spiderman/tools/patch_costume_viewer.py): shell:func_80261C70
lists src/assets_sm1.c's rows -- twenty stock costumes (retail's ten and ten imported
SM2 costumes), then the mod suits -- from the recomp's viewer table instead of the
retail one at 0x802693A8: its row count, unlock test, selected costume (read and write)
and the four table bases go through costume_sm1_*. The read feeding the retail costXX
resource table (+0x7480) keeps the 0..9 proxy byte. The list is allocated for
VIEWER_ROWS rows (retail 40) and the two list helpers (func_8001681C, func_80016A28)
clamp to the list's own row count instead of forty.

Idempotent: an already transformed file is left alone.
"""
import re
import sys
from pathlib import Path

TABLE = "GUEST_TEXTURE_REGISTRY /* SM1 texture registry, relocated */"
LAYOUT = '#include "SpiderMan.h"\n#include "../../src/guest_layout.h"   /* GUEST_TEXTURE_REGISTRY */\n'
MARK = "SM1 texture registry, relocated"
SPIN_MARK = "rt_idle(); /* vblank countdown */"
# The C output carries vertex tags (c->t[n]) only in functions that use the GTE, so a
# pattern may appear in either form.
SPIN = re.compile(r"(\n        (L[0-9A-F]{8}): ;\n)(        c->r\[2\] = 0x800B0000u;(?: c->t\[2\] = 0;)?\n"
                  r"        c->r\[2\] = mem_r32(?:_t\(c, 2, |\()\(c->r\[2\] \+ 0x5464u\)\);\n"
                  r"        if \(c->r\[2\] != 0u\) \{\n(?:            [^\n]*\n)?            goto \2;\n)")


def replace_exact(text: str, old, new: str, expected: int) -> str:
    """old: one string, or alternatives (plain and tagged forms) of which one matches."""
    for candidate in ([old] if isinstance(old, str) else old):
        if text.count(candidate) == expected:
            return text.replace(candidate, new)
    raise SystemExit(f"expected {expected} occurrence(s) of {old!r}")


def function(text: str, name: str) -> tuple[int, int]:
    start = text.index(f"void SpiderMan_{name}(CpuContext *c)\n{{")
    end = text.index("\n}\n", start) + 3
    return start, end


VIEWER_ROWS = 253                       # src/settings.h
VIEWER_MARK = "/* costume viewer: twenty stock rows and the mod suits */"
VIEWER_DECL = ("#include \"../../src/guest_layout.h\"   /* GUEST_VIEWER_TABLE */\n"
               "uint32_t costume_sm1_viewer_count(void);   /* src/assets_sm1.c */\n"
               "int costume_sm1_is_unlocked(uint32_t index);\n"
               "uint32_t costume_sm1_read_selected(void);\n"
               "void costume_sm1_write_selected(int selected);\n"
               "void costume_sm1_configure_viewer_list(uint32_t list);\n"
               "void costume_sm1_align_viewer_frame(uint32_t list);\n")
LIST_MARK = "/* list rows: the list's own count */"


def viewer(root: Path) -> None:
    """The PC's spiderman/tools/patch_costume_viewer.py, on shell:func_80261C70."""
    path = root / "SpiderMan_shell.c"
    text = path.read_text(encoding="utf-8")
    if VIEWER_MARK in text:
        print("costume viewer: already transformed")
        return
    start = text.index("static void SpiderMan_func_80261C70_Impl(CpuContext *c)\n{")
    end = text.index("\n}\n", start)
    f = text[start:end]
    f = replace_exact(f, "        c->r[4] = 0x00000488u;\n",
                      f"        c->r[4] = 0x{0x28 + 28 * VIEWER_ROWS:08X}u; {VIEWER_MARK}\n", 1)
    # The constructor's 10 is the row pitch, not the capacity; the loops' 10 is the count.
    f = replace_exact(f, "< 0x0000000Au ? 1u : 0u;", "< costume_sm1_viewer_count() ? 1u : 0u;", 2)
    for old, reg in (("        c->r[2] = c->r[2] & c->r[3];\n", 16), ("        c->r[3] = c->r[3] & c->r[2];\n", 19)):
        dst = old.split("]")[0].split("[")[1]
        f = replace_exact(f, old, old + f"        c->r[{dst}] = costume_sm1_is_unlocked(c->r[{reg}]) ? 1u : 0u;\n", 1)
    f = replace_exact(f, "        SpiderMan_func_80016424(c);\n",
                      "        SpiderMan_func_80016424(c);\n        costume_sm1_configure_viewer_list(c->r[21]);\n", 1)
    f = replace_exact(f, "        SpiderMan_func_800166A0(c);\n",
                      "        SpiderMan_func_800166A0(c);\n        costume_sm1_align_viewer_frame(c->r[21]);\n", 1)
    f, count = re.subn(r"c->r\[(\d+)\] = c->r\[\d+\] - 0x6C58u;", lambda m: f"c->r[{m[1]}] = GUEST_VIEWER_TABLE;", f)
    if count != 4:
        raise SystemExit(f"costume viewer: expected 4 table bases, found {count}")
    # Four reads are the selected row (the extended one); the fifth feeds the retail
    # costXX resource-name table (+0x7480) and keeps the 0..9 proxy byte.
    lines = f.split("\n")
    read = re.compile(r"^(\s*)c->r\[(\d+)\] = mem_r8\(\(c->r\[\d+\] \+ 0x7Cu\)\);$")
    reads = [i for i, l in enumerate(lines) if read.match(l)]
    retail = [i for i in reads if "0x7480u" in "\n".join(lines[i:i + 8])]
    if len(reads) != 5 or len(retail) != 1:
        raise SystemExit(f"costume viewer: selected-byte reads {len(reads)}, retail {len(retail)}")
    for i in reads:
        if i != retail[0]:
            m = read.match(lines[i])
            lines[i] = f"{m[1]}c->r[{m[2]}] = costume_sm1_read_selected();"
    f = "\n".join(lines)
    f, count = re.subn(r"mem_w8\(\(c->r\[\d+\] \+ 0x7Cu\), \(uint8_t\)c->r\[19\]\);",
                       "costume_sm1_write_selected((int)(uint8_t)c->r[19]);", f)
    if count != 1:
        raise SystemExit(f"costume viewer: expected 1 selected-byte write, found {count}")
    text = text[:start] + f + text[end:]
    decl_at = text.index("static void SpiderMan_func_80261C70_Impl(CpuContext *c);\n")
    text = text[:decl_at] + VIEWER_DECL + text[decl_at:]
    path.write_text(text, encoding="utf-8", newline="\n")
    print(f"costume viewer: twenty stock rows + up to {VIEWER_ROWS - 20} mod suits")


def list_helpers(text: str) -> str:
    if LIST_MARK in text:
        print("list helpers: already transformed")
        return text
    for address, reg in (("8001681C", 24), ("80016A28", 6)):
        start = text.index(f"void SpiderMan_func_{address}(CpuContext *c)\n{{\n")
        end = text.index("\n}\n", start)
        f = text[start:end]
        brace = f.index("{\n") + 2
        # Read before the body advances a0 through the rows; every retail list holds at
        # most forty, so only the costume viewer's goes past.
        f = (f[:brace] + "        int32_t list_rows = (int32_t)mem_r8(c->r[4] + 0x14u); " + LIST_MARK + "\n"
             + "        if (list_rows < 40) list_rows = 40;\n"
             + f"        if (list_rows > {VIEWER_ROWS}) list_rows = {VIEWER_ROWS};\n" + f[brace:])
        f = replace_exact(f, f"(int32_t)c->r[{reg}] < 40 ?", f"(int32_t)c->r[{reg}] < list_rows ?", 1)
        text = text[:start] + f + text[end:]
    print("list helpers: clamp to the list's own row count")
    return text


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "generated" / "spiderman")
    path = root / "SpiderMan_main.c"
    text = path.read_text(encoding="utf-8")
    if SPIN_MARK not in text:
        text, count = SPIN.subn(lambda m: m[1] + "        " + SPIN_MARK + "\n" + m[3], text)
        if count != 4:
            raise SystemExit(f"expected 4 vblank countdown loops, found {count}")
        path.write_text(text, encoding="utf-8", newline="\n")
        print("vblank countdown: 4 loops call rt_idle")
    if MARK in text:
        print("texture registry: already transformed")
    else:
        # One writer and two readers construct the retail base five times.
        text = replace_exact(text, "c->r[7] = c->r[7] - 0x65E8u;", f"c->r[7] = {TABLE};", 1)
        text = replace_exact(text, ["c->r[5] = mem_r32((c->r[4] - 0x65E8u));", "c->r[5] = mem_r32_t(c, 5, (c->r[4] - 0x65E8u));"],
                             f"c->r[5] = mem_r32({TABLE});", 1)
        text = replace_exact(text, ["c->r[17] = c->r[4] - 0x65E8u;", "c->r[17] = c->r[4] - 0x65E8u; c->t[17] = c->t[4];"],
                             f"c->r[17] = {TABLE};", 1)
        text = replace_exact(text, ["c->r[16] = mem_r32((c->r[5] - 0x65E8u));", "c->r[16] = mem_r32_t(c, 16, (c->r[5] - 0x65E8u));"],
                             f"c->r[16] = mem_r32({TABLE});", 1)
        text = replace_exact(text, ["c->r[19] = c->r[5] - 0x65E8u;", "c->r[19] = c->r[5] - 0x65E8u; c->t[19] = c->t[5];"],
                             f"c->r[19] = {TABLE};", 1)

        # Register every texture of the player model slots (TextureRegistry.IsPlayerModelSlot).
        start, end = function(text, "func_80068BB0_Impl")  # the hooked function's body
        slot = ("        if (mem_r32(c->r[29] + 0x78u) == 2u || mem_r32(c->r[29] + 0x78u) == 12u) /* player model slot */\n"
                "            goto L80069040;\n")
        body = text[start:end]
        for first, second in (("        c->r[30] = c->r[6] + 0x14u;\n", "        c->r[2] = c->r[2] < c->r[3] ? 1u : 0u;\n"),
                              ("        c->r[30] = c->r[6] + 0x14u; c->t[30] = c->t[6];\n",
                               "        c->r[2] = c->r[2] < c->r[3] ? 1u : 0u; c->t[2] = 0;\n")):
            if body.count(first + second) == 1:
                body = body.replace(first + second, first + slot + second)
                break
        else:
            raise SystemExit("player model slot test: pattern not found")
        text = text[:start] + body + text[end:]
        text = replace_exact(text, '#include "SpiderMan.h"\n', LAYOUT, 1)
        path.write_text(text, encoding="utf-8", newline="\n")
        print("texture registry: 17 -> 32768 records at 0x80780000; player slots register every texture")
    text = list_helpers(text)
    path.write_text(text, encoding="utf-8", newline="\n")
    viewer(root)
    return 0


if __name__ == "__main__":
    sys.exit(main())
