#!/usr/bin/env python3
"""Capture a dense native gameplay sequence for sizzle-reel editing."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_STAGE = ROOT / "proof_render" / "user-facing-stage" / "ready" / "Spider-Man"
DEFAULT_OUTPUT = ROOT / "proof_render" / "sizzle-reel" / "gameplay-spiderham"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", type=Path, default=DEFAULT_STAGE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--suit", default="smu-spiderham")
    parser.add_argument("--timeout", type=int, default=180)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    stage = args.stage.resolve()
    output = args.output.resolve()
    exe = stage / "SpiderMan.exe"
    suits = stage / "mods" / "suits"
    manifest = json.loads((suits / args.suit / "suit.json").read_text(encoding="utf-8"))
    if manifest["id"] != args.suit:
        raise ValueError("suit identity mismatch")

    output.mkdir(parents=True, exist_ok=True)
    for old in output.glob("frame_*.png"):
        old.unlink()
    offsets = list(range(420, 921, 3))
    env = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith(("SPIDEY_", "RECOMP_"))
    }
    env.update(
        {
            "RECOMP_CAPTURE_HIDDEN": "1",
            "RECOMP_RENDER_SCALE": "3",
            "SPIDEY_DATA": str(stage / "game"),
            "SPIDEY_ASSET_DIR": str(stage / "assets" / "builtin"),
            "SPIDEY_SUIT_MOD_DIR": str(suits),
            "SPIDEY_COSTUME": args.suit,
            "SPIDEY_LEVEL": "l5a3",
            "SPIDEY_WIDE": "1",
            "SPIDEY_BOOT_SKIP_UNTIL": "title.bmr",
            "SPIDEY_HZ": "60",
            "SPIDEY_SCRIPT_EXCLUSIVE": "1",
            "SPIDEY_SCRIPT": (
                "title.bmr+120:start:12;title.bmr+420:cross:12;title.bmr+720:cross:12;"
                "l5a3_t.trg+240:cross:12;l5a3_t.trg+500:down:24;"
                "l5a3_t.trg+610:up+r2:180;l5a3_t.trg+790:right+r2:100"
            ),
            "SPIDEY_SHOTS": ",".join(f"l5a3_t.trg+{offset}" for offset in offsets),
            "SPIDEY_SHOT_DIR": str(output),
            "SPIDEY_CAPTURE_PRESENTED": "1",
            "SPIDEY_EXIT": "l5a3_t.trg+980",
            "SPIDEY_LOG_DIR": str(output),
            "SPIDEY_STALL_EXIT": "1",
        }
    )

    result = subprocess.run(
        [str(exe)],
        cwd=stage,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=args.timeout,
        creationflags=subprocess.CREATE_NO_WINDOW,
        check=False,
    )
    console = result.stdout or ""
    (output / "console.log").write_text(console, encoding="utf-8")
    frames = sorted(output.glob("frame_*.png"))
    errors: list[str] = []
    if result.returncode != 0:
        errors.append(f"game exited {result.returncode}")
    if len(frames) != len(offsets):
        errors.append(f"expected {len(offsets)} frames, found {len(frames)}")
    if f"[suit-mod] active {args.suit}:" not in console:
        errors.append("requested suit did not become active")
    if not re.search(r"\[capture\] exit at frame \d+", console):
        errors.append("clean scripted exit not recorded")
    sizes: set[tuple[int, int]] = set()
    for frame in frames:
        with Image.open(frame) as image:
            image.load()
            sizes.add(image.size)
    if len(sizes) != 1 or not sizes or min(next(iter(sizes))) < 540:
        errors.append(f"inconsistent or undersized frames: {sorted(sizes)}")

    report = {
        "status": "pass" if not errors else "fail",
        "suit": args.suit,
        "captureMode": "hidden native renderer with process-local input",
        "frameCount": len(frames),
        "frameSizes": [list(size) for size in sorted(sizes)],
        "errors": errors,
    }
    (output / "capture.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report))
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
