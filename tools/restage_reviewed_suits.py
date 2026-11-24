"""Reapply the reviewed suit assets to the two root-level game installations.

Use --apply to synchronize changed files and move known excluded suits out of
the active catalogue. With no arguments, audit only. All work stays in this repo.
The current 70-ID smu-eligibility.json supersedes the historical 78-ID retarget
gate. Copy the reviewed models: rerunning foot shortening would compound it.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/retarget"))
SHARED = ROOT / "spiderman/port/mods/suits"
RECIPES = {
    "f16254b": "Reviewed colors, head seams, UVs, orientation, names and credits",
    "27d9cfa": "Eight SMU removals and matching SM2 reskin repairs",
    "8b729f8": "Shorter feet on twenty preserved-rig actors",
    "a76837c": "Captain Spider display name",
    "24e4fc1": "Iron Spider - MCU transparent eye lenses",
}
SOURCE_ROOTS = [SHARED, ROOT / "spiderman/port/legacy-suits", ROOT / "spiderman2/port/legacy-suits"]


def contained(path: Path) -> Path:
    for component in (path, *path.parents):
        if component == ROOT:
            break
        if component.is_symlink() or getattr(component, "is_junction", lambda: False)():
            raise ValueError(f"Refusing a linked path: {component}")
    resolved = path.resolve()
    if not resolved.is_relative_to(ROOT):
        raise ValueError(f"Path outside repository: {path}")
    return resolved


def sha(path: Path) -> str:
    with contained(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def git(*args: str) -> bytes:
    return subprocess.check_output(["git", *args], cwd=ROOT)


def reviewed_assets() -> dict:
    """Latest listed repair wins when two repairs touched the same asset."""
    latest = {}
    for revision in RECIPES:
        paths = git("diff-tree", "--no-commit-id", "--name-only", "--diff-filter=AM", "-r", revision).decode().splitlines()
        for relative in paths:
            path = ROOT / relative
            if path.is_file() and any(path.is_relative_to(source) for source in SOURCE_ROOTS):
                latest[relative] = revision
    result = {}
    for relative, revision in sorted(latest.items()):
        path = ROOT / relative
        recorded = git("show", f"{revision}:{relative}")
        actual = path.read_bytes()
        if actual != recorded and path.name == "actor.psx":
            # Replay the later RTG2 calibration against the actual reviewed bytes.
            # All of the earlier topology, texture and foot edits must survive.
            from head_seams import MASKS, repair_actor_head_uvs
            from suit_proportions import Rig, calibrated_blob, rig_offset
            offset, size = rig_offset(recorded)
            calibrated = calibrated_blob(recorded[offset:offset+size], path.parent.name)
            recorded = bytearray(recorded[:offset] + calibrated + recorded[offset+size:])
            repair_actor_head_uvs(recorded, path.parent.name, Rig(calibrated))
            if path.parent.name in MASKS:
                from native_asset import replace_diffuse_fallback
                from PIL import Image
                with Image.open(path.parent / 'textures/diffuse.png') as image:
                    replace_diffuse_fallback(recorded, image)
        if actual != recorded and path.name == 'diffuse.png':
            from head_seams import MASKS, reviewed_diffuse
            if path.parent.parent.name in MASKS:
                from io import BytesIO
                from PIL import Image
                with Image.open(BytesIO(recorded)) as original, Image.open(path) as current:
                    expected = reviewed_diffuse(original, path.parent.parent.name)
                    if expected.mode == current.mode and expected.size == current.size and expected.tobytes() == current.tobytes():
                        recorded = actual
        if actual != recorded and (path.suffix != ".json" or actual.splitlines() != recorded.splitlines()):
            raise ValueError(f"Source differs from recorded repair {revision}: {relative}")
        current = hashlib.sha256(actual).hexdigest()
        result[relative] = {"repair_commit": revision, "sha256": current}
    foot_models = [p for p, row in result.items() if row["repair_commit"] == "8b729f8" and p.endswith("/actor.psx")]
    if len(foot_models) != 20:
        raise ValueError(f"Expected twenty recorded foot repairs, found {len(foot_models)}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    allowed = set(json.loads((SHARED / "smu-eligibility.json").read_text())["eligibleIds"])
    active = {p.parent.name for p in SHARED.glob("*/suit.json")}
    if len(allowed) != 70 or active != allowed:
        raise ValueError("Source catalogue does not match the reviewed seventy-suit list")
    repairs = reviewed_assets()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    evidence = contained(ROOT / ".local/reviewed-suits" / stamp)
    report = {"source_commit": git("rev-parse", "HEAD").decode().strip(),
              "mode": "apply" if args.apply else "audit", "recipes": RECIPES,
              "reviewed_assets": repairs, "games": []}
    evidence.mkdir(parents=True)
    for title, project in [("Spider-Man", "spiderman"), ("Spider-Man 2", "spiderman2")]:
        stage = contained(ROOT / title)
        target = contained(stage / "mods/suits")
        if not target.is_dir():
            raise FileNotFoundError(f"Stage the game first: {target}")
        expected = {}
        for source in [SHARED, ROOT / project / "port/legacy-suits"]:
            for path in sorted(source.rglob("*")):
                if path.is_file():
                    relative = path.relative_to(source)
                    if relative in expected:
                        raise ValueError(f"Duplicate source asset: {relative}")
                    expected[relative] = contained(path)
        excluded = {p.name for base in [ROOT / "spiderman/port/mods/inactive-suits", ROOT / project / "port/legacy-inactive-suits"]
                    for p in base.iterdir() if p.is_dir()}
        excluded_active = [p for p in target.iterdir() if p.is_dir() and p.name in excluded]
        removed = []
        for folder in excluded_active:
            if args.apply:
                source = contained(folder)
                destination = contained(stage / "mods/inactive-suits" / folder.name)
                if destination.exists():
                    destination = contained(evidence / "excluded" / title / folder.name)
                for child in source.rglob("*"):
                    contained(child)
                destination.parent.mkdir(parents=True, exist_ok=True)
                source.rename(destination)
            removed.append(folder.name)
        changed, verified = [], []
        for relative, source in expected.items():
            destination = contained(target / relative)
            expected_hash = sha(source)
            matches = destination.is_file() and sha(destination) == expected_hash
            if not matches:
                changed.append(relative.as_posix())
                if args.apply:
                    if destination.exists():
                        if not destination.is_file():
                            raise ValueError(f"Expected a file: {destination}")
                        backup = contained(evidence / "backups" / title / relative)
                        backup.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(destination, backup)
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    temporary = contained(destination.with_name(destination.name + f".{stamp}.tmp"))
                    if temporary.exists():
                        raise FileExistsError(temporary)
                    try:
                        shutil.copy2(source, temporary)
                        temporary.replace(destination)
                    finally:
                        if temporary.exists():
                            temporary.unlink()
            if destination.is_file() and sha(destination) == expected_hash:
                verified.append(relative.as_posix())
        installed_smu = {p.parent.name for p in target.glob("smu-*/suit.json")}
        row = {"game": title, "source_files": len(expected), "verified_files": len(verified),
               "files_updated": changed if args.apply else [], "files_needing_update": [] if args.apply else changed,
               "excluded_suits_found": removed, "active_smu": len(installed_smu),
               "exact_reviewed_smu_set": installed_smu == allowed}
        row["passed"] = len(verified) == len(expected) and installed_smu == allowed and (args.apply or not removed)
        report["games"].append(row)
        print(json.dumps(row), flush=True)
    report["passed"] = all(game["passed"] for game in report["games"])
    (evidence / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Report: {evidence / 'report.json'}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
