"""Capture one bounded SM2 gameplay frame for a selected suit package."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from PIL import Image


def hash_tree(root: Path) -> dict[str, str]:
    return {
        path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(root.rglob("*"))
        if path.is_file()
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--suits", type=Path, required=True)
    parser.add_argument("--id", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()

    build, data, suits, out = (path.resolve() for path in (args.build, args.data, args.suits, args.out))
    if out.exists():
        parser.error("Choose a fresh evidence directory")
    out.mkdir(parents=True)
    source_suit = suits / args.id
    manifest = json.loads((source_suit / "suit.json").read_text(encoding="utf-8"))
    if manifest["id"] != args.id:
        raise ValueError("suit identity mismatch")

    runtime = out / ".runtime"
    fixture = out / ".suits"
    shutil.copytree(build, runtime, ignore=shutil.ignore_patterns("*.sav", "settings.json", "interface.ini", "mods", "logs", "shots*"))
    shutil.copytree(source_suit, fixture / args.id)
    (fixture / "selected-suit.txt").write_text(args.id + "\n", encoding="utf-8")
    (runtime / "settings.json").write_text(
        json.dumps({"CdPath": str(data), "CardAEnabled": False, "CardBEnabled": False, "Widescreen": True, "Muted": True}),
        encoding="utf-8",
    )
    (runtime / "interface.ini").write_text(
        "[RecompOne]\nFullscreen=False\nWindowWidth=1280\nWindowHeight=720\nRenderScale=4\nFxaa=True\nVSync=False\n",
        encoding="utf-8",
    )

    env = {key: value for key, value in os.environ.items() if not key.startswith(("SPIDEY_", "RECOMP_", "DOTNET_", "COMPlus_"))}
    env.update(
        RECOMP_CAPTURE_HIDDEN="1",
        RECOMP_RENDER_SCALE="4",
        SPIDEY_WIDE="1",
        SPIDEY_SCRIPT_EXCLUSIVE="1",
        SPIDEY_SCRIPT=(
            "title.bmr+80:start:10;title.bmr+280:cross:10;title.bmr+480:cross:10;title.bmr+700:cross:10;"
            "e1m0_t.trg+1200:cross:8;e1m0_t.trg+1500:cross:8;e1m0_t.trg+1800:cross:8;"
            "e1m0_t.trg+2100:cross:8;e1m0_t.trg+2400:cross:8"
        ),
        SPIDEY_LEVEL="e1m0",
        SPIDEY_BOOT_SKIP_UNTIL="title.bmr",
        SPIDEY_SUIT_MOD_DIR=str(fixture),
        SPIDEY_COSTUME=args.id,
        SPIDEY_SHOTS="e1m0_t.trg+3400",
        SPIDEY_SHOT_DIR=str(out),
        SPIDEY_CAPTURE_PRESENTED="1",
        SPIDEY_EXIT="e1m0_t.trg+3500",
        SPIDEY_LOG_DIR=str(out),
        SPIDEY_STALL_EXIT="1",
        SPIDEY_STALL="20",
        DOTNET_BUNDLE_EXTRACT_BASE_DIR=str(out / ".cache"),
    )
    executable = runtime / "SpiderMan2.exe"
    result = subprocess.run(
        [str(executable), str(data)],
        cwd=runtime,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=args.timeout,
        creationflags=subprocess.CREATE_NO_WINDOW,
        check=False,
    )
    console = result.stdout or ""
    (out / "console.log").write_text(console, encoding="utf-8")
    frames = sorted(out.glob("frame_*.png"))
    errors = []
    if result.returncode:
        errors.append(f"game exited {result.returncode}")
    if len(frames) != 1:
        errors.append(f"expected one frame, found {len(frames)}")
    if f"[suit-mod] active {args.id}:" not in console:
        errors.append("selected suit was not reported active")
    dimensions = None
    if frames:
        with Image.open(frames[0]) as image:
            dimensions = list(image.size)
            if min(image.size) < 720 or image.convert("RGB").getextrema() == ((0, 0), (0, 0), (0, 0)):
                errors.append(f"invalid frame: {image.size}")

    report = {
        "status": "pass" if not errors else "fail",
        "suit": args.id,
        "name": manifest["name"],
        "game": "Spider-Man 2",
        "capture": "Game framebuffer; process-local input only; no desktop capture or OS input",
        "executableSha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "packageFiles": hash_tree(source_suit),
        "image": frames[0].name if len(frames) == 1 else None,
        "dimensions": dimensions,
        "errors": errors,
    }
    (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    shutil.rmtree(runtime)
    shutil.rmtree(fixture)
    cache = out / ".cache"
    if cache.exists():
        shutil.rmtree(cache)
    print(json.dumps(report))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
