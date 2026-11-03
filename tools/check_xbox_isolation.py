#!/usr/bin/env python3
"""Fail if anything outside xbox/ refers to the Xbox port's folder.

The Windows and Linux builds must never compile, include, package or run code
from xbox/. The Xbox port may use shared code; nothing shared may depend on it.
Scans source, project and build files tracked or present outside xbox/.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKED = {".cs", ".csproj", ".props", ".targets", ".sln", ".py", ".sh", ".cmd", ".ps1",
           ".json", ".toml", ".mk", ".c", ".h", ".cpp", ".hpp", ".cmake", ".txt"}
SKIPPED_DIRS = {".git", "xbox", "private", "artifacts", "bin", "obj", "dist", "generated",
                "proof_render", "verification", "packs", "mods", "extracted", "__pycache__"}
# A path into the folder: "xbox/" or "xbox\" not preceded by a word character.
REFERENCE = re.compile(r"(?<![A-Za-z0-9_.-])xbox[/\\]", re.IGNORECASE)
SELF = Path(__file__).resolve()


def files():
    stack = [ROOT]
    while stack:
        folder = stack.pop()
        for entry in folder.iterdir():
            if entry.is_dir():
                if entry.name not in SKIPPED_DIRS:
                    stack.append(entry)
            elif (entry.suffix.lower() in CHECKED or entry.name == "Makefile") and entry.resolve() != SELF:
                yield entry


def main() -> int:
    hits = []
    count = 0
    for path in files():
        count += 1
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for number, line in enumerate(text.splitlines(), 1):
            if REFERENCE.search(line):
                hits.append(f"{path.relative_to(ROOT)}:{number}: {line.strip()[:160]}")
    if hits:
        print("Xbox code referenced outside xbox/:")
        print("\n".join(hits))
        return 1
    print(f"PASS: {count} files outside xbox/ make no reference to it")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
