#!/usr/bin/env python3
"""Apply SM2's generated-source transforms to the Xbox's C output.

    python -I xbox/tools/patch_generated_sm2.py [xbox/generated/spiderman2]

Run after RecompOne's --c output and before building (the counterpart of
patch_generated_sm1.py).

Vblank countdown: the game sets the counter at 0x800C2430 (80 vblanks, for instance)
and spins until the vblank interrupt has counted it down to zero. Here interrupts
arrive only with a 60 Hz tick, and a loop that only reads memory never delivers one,
so each loop calls rt_idle (the PC catches the same loops by counting memory reads,
PSMemory.TrackRead). The same loops as SM1's at 0x800B5464.

Pad reads: the boot and title code poll the pads (func_80076730 calls the pad read
func_80076498) in loops that call no VSync; rt_idle after the read delivers a tick
when one is due. The counterpart of SM1's PadTrace.AfterRead hook, done here because
SM2's shared configuration has no such hook (the PC catches these loops by counting
memory reads).

Movies: the game's player (func_80030350) needs LibCdStream and the MDEC, which the
Xbox runtime does not have; it calls src/movies_sm2.c after noting the movie as seen,
which plays the PS1 movie and skips the player to its exit (as SM1's movie hook does).

Costume viewer (the PC's spiderman2/tools/patch_costume_viewer.py): shell:func_80249280
lists src/costume_sm2.c's rows -- the nineteen stock costumes, then the mod suits -- from
the recomp's viewer table instead of the retail one at 0x80251560: its row count, unlock
test, selected costume (read and write) and both table bases go through costume_sm2_*.
The viewer's list is allocated for VIEWER_ROWS rows (retail 40), the rows past forty set
up after the retail constructor, and the two list helpers (func_80018074,
func_80018278) clamp to the list's own row count instead of forty. The selected byte
the texture loader and the exit read stays the retail one (costume_sm2.c keeps it a
stock profile).

Idempotent: an already transformed file is left alone.
"""
import re
import sys
from pathlib import Path

SPIN_MARK = "rt_idle(); /* vblank countdown */"
# Movies: the runtime plays them (src/movies_sm2.c) from inside the game's player,
# after it has noted the movie as seen, and the player's own body is skipped to its
# exit (L800309D4, which returns v0).
MOVIE_AT = "        L800303EC: ;\n"
MOVIE_CALL = "        if (movie_sm2_play(c)) goto L800309D4; /* runtime movie player */\n"
MOVIE_DECL = "int movie_sm2_play(CpuContext *c);   /* src/movies_sm2.c */\n"
PAD_CALL = "        SpiderMan2_func_80076498(c);\n"
PAD_MARK = "rt_idle(); /* pad read */"
# The C output carries vertex tags (c->t[n]) only in functions that use the GTE, so a
# pattern may appear in either form. As patch_generated_sm1.py's SPIN, at 0x800C2430.
SPIN = re.compile(r"(\n        (L[0-9A-F]{8}): ;\n)(        c->r\[2\] = 0x800C0000u;(?: c->t\[2\] = 0;)?\n"
                  r"        c->r\[2\] = mem_r32(?:_t\(c, 2, |\()\(c->r\[2\] \+ 0x2430u\)\);\n"
                  r"        if \(c->r\[2\] != 0u\) \{\n(?:            [^\n]*\n)?            goto \2;\n)")
LOOPS = 6

VIEWER_ROWS = 253                       # src/settings.h
VIEWER_MARK = "/* costume viewer: mod rows */"
VIEWER_DECL = ("#include \"../../src/guest_layout.h\"   /* GUEST_VIEWER_TABLE */\n"
               "uint32_t costume_sm2_viewer_count(void);   /* src/costume_sm2.c */\n"
               "int costume_sm2_is_unlocked(uint32_t index);\n"
               "uint32_t costume_sm2_read_selected(void);\n"
               "void costume_sm2_write_selected(int selected);\n"
               "void costume_sm2_prepare_viewer(CpuContext *c);\n"
               "void costume_sm2_configure_viewer_list(uint32_t list);\n"
               "void costume_sm2_align_viewer_frame(uint32_t list);\n")
LIST_MARK = "/* list rows: the list's own count */"


def exact(text, old, new, count=1):
    if text.count(old) != count:
        raise SystemExit(f"costume viewer: expected {count} of {old!r}, found {text.count(old)}")
    return text.replace(old, new)


def viewer(root: Path) -> None:
    path = root / "SpiderMan2_shell.c"
    text = path.read_text(encoding="utf-8")
    if VIEWER_MARK in text:
        print("costume viewer: already transformed")
        return
    start = text.index("void SpiderMan2_func_80249280(CpuContext *c)\n{")
    end = text.index("void SpiderMan2_func_80249F04(CpuContext *c)\n{", start)
    f = text[start:end]
    f = exact(f, "        c->r[4] = 0x00000528u;\n",
              f"        c->r[4] = 0x{0x28 + 32 * VIEWER_ROWS:08X}u; {VIEWER_MARK}\n")
    # The charbio pointers are ready once func_8023D074 returns; the rows copy them.
    entry = "        c->r[31] = 0x802492B4u;\n        SpiderMan2_func_8023D074(c);\n"
    f = exact(f, entry, entry + "        costume_sm2_prepare_viewer(c);\n")
    f = exact(f, "< 0x00000013u ? 1u : 0u;", "< costume_sm2_viewer_count() ? 1u : 0u;", 2)
    unlock = "        c->r[3] = c->r[3] & c->r[2];\n"
    if f.count(unlock) != 2:
        raise SystemExit("costume viewer: unlock tests not found")
    for reg in (16, 18):
        f = f.replace(unlock, f"        c->r[3] = costume_sm2_is_unlocked(c->r[{reg}]) ? 1u : 0u;\n", 1)
    for offset, base, expected in (("1560", "GUEST_VIEWER_TABLE", 4), ("1568", "(GUEST_VIEWER_TABLE + 8u)", 1)):
        f, count = re.subn(r"c->r\[(\d+)\] = c->r\[\d+\] \+ 0x" + offset + r"u;",
                           lambda m: f"c->r[{m[1]}] = {base};", f)
        if count != expected:
            raise SystemExit(f"costume viewer: table 0x8025{offset}: {count}, expected {expected}")
    reads = re.compile(r"c->r\[(\d+)\] = mem_r8\(\(c->r\[\d+\] \+ 0x72u\)\);")
    if len(reads.findall(f)) != 6:
        raise SystemExit(f"costume viewer: expected six selected-byte reads, found {len(reads.findall(f))}")
    # a0/a1 (the texture loader's costume, the exit's powers) keep the retail byte.
    f = reads.sub(lambda m: m[0] if m[1] in ("4", "5") else f"c->r[{m[1]}] = costume_sm2_read_selected();", f)
    f = exact(f, "mem_w8((c->r[7] + 0x72u), (uint8_t)c->r[18]);", "costume_sm2_write_selected((int)c->r[18]);")
    row_count = "        mem_w8((c->r[21] + 0x15u), (uint8_t)c->r[3]);\n"
    f = exact(f, row_count, row_count + "        costume_sm2_configure_viewer_list(c->r[21]);\n")
    frame = "        SpiderMan2_func_80017EF8(c);\n"
    f = exact(f, frame, frame + "        costume_sm2_align_viewer_frame(c->r[21]);\n")
    text = text[:start] + VIEWER_DECL + f + text[end:]
    path.write_text(text, encoding="utf-8", newline="\n")
    print(f"costume viewer: nineteen stock rows + up to {VIEWER_ROWS - 19} mod suits")


def list_helpers(text: str) -> str:
    if LIST_MARK in text:
        print("list helpers: already transformed")
        return text
    for address, reg in (("80018074", 24), ("80018278", 6)):
        start = text.index(f"void SpiderMan2_func_{address}(CpuContext *c)\n{{\n")
        end = text.index("\n}\n", start)
        f = text[start:end]
        brace = f.index("{\n") + 2
        # Read before the body advances a0 through the rows; every retail list holds at
        # most forty, so only the costume viewer's goes past.
        f = (f[:brace] + "        int32_t list_rows = (int32_t)mem_r8(c->r[4] + 0x14u); " + LIST_MARK + "\n"
             + "        if (list_rows < 40) list_rows = 40;\n"
             + f"        if (list_rows > {VIEWER_ROWS}) list_rows = {VIEWER_ROWS};\n" + f[brace:])
        f = exact(f, f"(int32_t)c->r[{reg}] < 40 ?", f"(int32_t)c->r[{reg}] < list_rows ?")
        text = text[:start] + f + text[end:]
    print("list helpers: clamp to the list's own row count")
    return text


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "generated" / "spiderman2")
    path = root / "SpiderMan2_main.c"
    text = path.read_text(encoding="utf-8")
    if SPIN_MARK in text:
        print("vblank countdown: already transformed")
    else:
        text, count = SPIN.subn(lambda m: m[1] + "        " + SPIN_MARK + "\n" + m[3], text)
        if count != LOOPS:
            raise SystemExit(f"expected {LOOPS} vblank countdown loops, found {count}")
        print(f"vblank countdown: {count} loops call rt_idle")
    if PAD_MARK in text:
        print("pad read: already transformed")
    else:
        if text.count(PAD_CALL) != 1:
            raise SystemExit(f"expected 1 call of the pad read, found {text.count(PAD_CALL)}")
        text = text.replace(PAD_CALL, PAD_CALL + "        " + PAD_MARK + "\n")
        print("pad read: rt_idle after it")
    if MOVIE_CALL in text:
        print("movies: already transformed")
    else:
        start = text.index("void SpiderMan2_func_80030350(CpuContext *c)\n{")
        end = text.index("\n}\n", start)
        body = text[start:end]
        if body.count(MOVIE_AT) != 1 or "L800309D4: ;" not in body:
            raise SystemExit("movie player: entry label or exit not found")
        body = body.replace(MOVIE_AT, MOVIE_AT + MOVIE_CALL)
        text = text[:start] + MOVIE_DECL + text[start:start] + body + text[end:]
        print("movies: the runtime plays them from func_80030350")
    text = list_helpers(text)
    path.write_text(text, encoding="utf-8", newline="\n")
    viewer(root)
    return 0


if __name__ == "__main__":
    sys.exit(main())
