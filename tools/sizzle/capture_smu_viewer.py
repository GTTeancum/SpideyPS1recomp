#!/usr/bin/env python3
"""Capture the active SMU catalogue in one native costume-viewer run."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_STAGE = ROOT / "proof_render" / "user-facing-stage" / "ready" / "Spider-Man"
DEFAULT_OUTPUT = ROOT / "proof_render" / "sizzle-reel" / "native-viewer"

ROUTE = (
    "title.bmr+120:start:12",
    "title.bmr+300:right:12",
    "title.bmr+500:down:12",
    "title.bmr+700:down:12",
    "title.bmr+900:cross:12",
    "title.bmr+1200:cross:12",
)
FIRST_CAPTURE = 1450
FIRST_ADVANCE = 1650
STEP_INTERVAL = 300
CROSS_DELAY = 70
CAPTURE_DELAY = 220


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", type=Path, default=DEFAULT_STAGE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--timeout", type=int, default=600)
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def ensure_one_process() -> None:
    result = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq SpiderMan.exe", "/FO", "CSV", "/NH"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode == 0 and re.search(r'"SpiderMan\.exe"', result.stdout, re.I):
        raise RuntimeError("refusing to launch while SpiderMan.exe is already running")


def read_roster(suit_root: Path) -> list[dict[str, str]]:
    eligibility = json.loads((suit_root / "smu-eligibility.json").read_text(encoding="utf-8"))
    allowed = set(eligibility["eligibleIds"])
    roster: list[dict[str, str]] = []
    for folder in sorted(suit_root.iterdir(), key=lambda path: path.name):
        manifest = folder / "suit.json"
        if not folder.is_dir() or not manifest.is_file() or folder.name not in allowed:
            continue
        data = json.loads(manifest.read_text(encoding="utf-8"))
        roster.append({"id": data["id"], "name": data["name"]})
    if len(roster) != 78 or {entry["id"] for entry in roster} != allowed:
        raise ValueError(f"expected exact 78-suit active SMU roster, found {len(roster)}")
    return roster


def build_script(count: int) -> str:
    steps = list(ROUTE)
    for index in range(1, count):
        advance = FIRST_ADVANCE + (index - 1) * STEP_INTERVAL
        steps.append(f"title.bmr+{advance}:down:12")
        steps.append(f"title.bmr+{advance + CROSS_DELAY}:cross:12")
    return ";".join(steps)


def shot_offsets(count: int) -> list[int]:
    return [FIRST_CAPTURE] + [
        FIRST_ADVANCE + (index - 1) * STEP_INTERVAL + CAPTURE_DELAY
        for index in range(1, count)
    ]


def verify_image(path: Path) -> dict[str, object]:
    with Image.open(path) as opened:
        opened.load()
        image = opened.convert("RGB")
        extrema = image.getextrema()
        colors = image.getcolors(maxcolors=image.width * image.height)
    dynamic_range = max(high - low for low, high in extrema)
    color_count = len(colors) if colors is not None else image.width * image.height
    if image.width < 960 or image.height < 540 or dynamic_range < 32 or color_count < 64:
        raise ValueError(
            f"non-reviewable capture {path.name}: size={image.size}, "
            f"range={dynamic_range}, colors={color_count}"
        )
    return {
        "size": list(image.size),
        "dynamicRange": dynamic_range,
        "colorCount": color_count,
        "sha256": sha256(path),
    }


def main() -> None:
    args = parse_args()
    stage = args.stage.resolve()
    output = args.output.resolve()
    exe = stage / "SpiderMan.exe"
    suit_root = stage / "mods" / "suits"
    if not exe.is_file() or not suit_root.is_dir():
        raise FileNotFoundError(f"incomplete staged game: {stage}")

    roster = read_roster(suit_root)
    ensure_one_process()
    output.mkdir(parents=True, exist_ok=True)
    for old in output.glob("frame_*.png"):
        old.unlink()

    selection = suit_root / "selected-suit.txt"
    prior_selection = selection.read_bytes() if selection.exists() else None
    selection.write_text(roster[0]["id"], encoding="ascii")
    offsets = shot_offsets(len(roster))
    exit_offset = offsets[-1] + 180

    env = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith(("SPIDEY_", "RECOMP_"))
    }
    env.update(
        {
            "RECOMP_CAPTURE_HIDDEN": "1",
            "RECOMP_RENDER_SCALE": "4",
            "SPIDEY_DATA": str(stage / "game"),
            "SPIDEY_ASSET_DIR": str(stage / "assets" / "builtin"),
            "SPIDEY_SUIT_MOD_DIR": str(suit_root),
            "SPIDEY_BOOT_SKIP_UNTIL": "title.bmr",
            "SPIDEY_CHEATS": "everything,viewers",
            "SPIDEY_HZ": "60",
            "SPIDEY_SCRIPT_EXCLUSIVE": "1",
            "SPIDEY_SCRIPT": build_script(len(roster)),
            "SPIDEY_SHOTS": ",".join(f"title.bmr+{offset}" for offset in offsets),
            "SPIDEY_SHOT_DIR": str(output),
            "SPIDEY_CAPTURE_PRESENTED": "1",
            "SPIDEY_EXIT": f"title.bmr+{exit_offset}",
            "SPIDEY_LOG_DIR": str(output),
            "SPIDEY_STALL_EXIT": "1",
        }
    )

    try:
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
    finally:
        if prior_selection is None:
            selection.unlink(missing_ok=True)
        else:
            selection.write_bytes(prior_selection)

    console = result.stdout or ""
    (output / "console.log").write_text(console, encoding="utf-8")
    frames = sorted(output.glob("frame_*.png"))
    resolved = [
        int(frame)
        for frame in re.findall(
            r"\[capture\] 'title\.bmr' at frame \d+: archive shot resolved to frame (\d+)",
            console,
            re.I,
        )
    ]
    active = re.findall(r"\[suit-mod\] active ([^:]+):", console)
    errors: list[str] = []
    captures: list[dict[str, object]] = []
    if result.returncode != 0:
        errors.append(f"game exited {result.returncode}")
    if len(frames) != len(roster):
        errors.append(f"expected {len(roster)} frames, found {len(frames)}")
    if len(resolved) != len(roster):
        errors.append(f"expected {len(roster)} resolved shots, found {len(resolved)}")
    if active[: len(roster)] != [entry["id"] for entry in roster]:
        errors.append("active-suit sequence did not match metadata roster")

    for index, entry in enumerate(roster):
        try:
            frame = output / f"frame_{resolved[index]:05d}.png"
            captures.append({"index": index, **entry, "path": str(frame), **verify_image(frame)})
        except Exception as error:
            errors.append(f"{entry['id']}: {error}")

    report = {
        "status": "pass" if not errors else "fail",
        "stage": str(stage),
        "executableSha256": sha256(exe),
        "captureMode": "hidden native renderer with process-local input",
        "count": len(captures),
        "captures": captures,
        "errors": errors,
    }
    (output / "capture.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({"status": report["status"], "count": len(captures), "errors": errors}))
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
