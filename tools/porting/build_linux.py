#!/usr/bin/env python3
"""Publish native Linux x64 game apphosts (Linux build host).

Uses the checked-in generated game code. Does not regenerate it, change gameplay
pacing, install system packages, or include a disc. Stops on any failed command.
"""
from __future__ import annotations
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]

def run(argv: list[str], log) -> None:
    print("+ " + " ".join(map(str, argv)), flush=True)
    log.write("+ " + " ".join(map(str, argv)) + "\n"); log.flush()
    with subprocess.Popen(argv, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True) as p:
        assert p.stdout is not None
        for line in p.stdout:
            print(line, end="", flush=True); log.write(line); log.flush()
        code = p.wait()
        if code:
            raise subprocess.CalledProcessError(code, argv)

def verify_elf(path: Path) -> None:
    data = path.read_bytes()[:20]
    if len(data) < 20 or data[:6] != b"\x7fELF\x02\x01" or int.from_bytes(data[18:20], "little") != 62:
        raise RuntimeError(f"not an x86-64 ELF: {path}")

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--game", choices=["1", "2", "both"], default="both")
    ap.add_argument("--output", type=Path)
    ap.add_argument("--no-restore", action="store_true", help="use an already restored linux-x64 SDK/package cache")
    args = ap.parse_args()
    if platform.system() != "Linux" or platform.machine().lower() not in ("x86_64", "amd64"):
        ap.error("this helper builds on Linux x64; Windows hosts can use BUILD-LINUX.cmd")
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        ap.error(".NET 10 SDK is not installed; no game build was attempted")
    version = subprocess.check_output([dotnet, "--version"], cwd=ROOT, text=True).strip()
    if version.split(".")[0] != "10":
        ap.error("this source requires the .NET 10 SDK; found " + version)
    out = args.output or ROOT / "dist" / ("Linux-06-" + datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d-%H%M%S"))
    out = out.resolve()
    if out.exists():
        ap.error("output must not already exist (previous builds are never overwritten)")
    out.mkdir(parents=True)
    results = {"sdk": version, "games": [], "gameplay_tested": False}
    try:
        with (out / "build.log").open("w", encoding="utf8") as log:
            run([sys.executable, str(ROOT / "tools/retarget/build_native.py"), "--target", "linux-x64"], log)
            run([sys.executable, str(ROOT / "tools/movies/build_native.py"), "--target", "linux-x64"], log)
            bundles = [str(ROOT / g / "port/bundled/runtime-assets.zip") for g in ("spiderman", "spiderman2")]
            command = [dotnet, "run", "--project", str(ROOT / "tools/RecompOne/tests/PortabilityRegression"), "-c", "Release"]
            if args.no_restore: command += ["--no-restore"]
            run(command + ["--"] + bundles, log)
            for test in ("MovieOverrideRegression", "MovieInstructionHookRegression"):
                command = [dotnet, "run", "--project", str(ROOT / "tools/RecompOne/tests" / test), "-c", "Release"]
                if args.no_restore: command += ["--no-restore"]
                run(command, log)
            for number, name, assembly in [("1", "spiderman", "SpiderMan"), ("2", "spiderman2", "SpiderMan2")]:
                if args.game not in (number, "both"): continue
                dest = out / name
                command = [dotnet, "publish", str(ROOT / name / "port" / (assembly + ".csproj")),
                           "-c", "Release", "-r", "linux-x64", "--self-contained", "true", "--nologo", "-o", str(dest)]
                if args.no_restore: command += ["--no-restore"]
                run(command, log)
                binary = dest / assembly
                verify_elf(binary)
                verify_elf(dest / "libOpenSpideySfd.so")
                if not (dest / "licenses/OpenSpideySfd-LICENSE-PL_MPEG.txt").is_file():
                    raise RuntimeError("Missing SFD license in publication")
                binary.chmod(binary.stat().st_mode | 0o111)
                launcher = dest / ("Run-" + assembly + ".sh")
                launcher.write_text('#!/usr/bin/env bash\nset -euo pipefail\nhere="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"\nexec "$here/' + assembly + '" "$@"\n')
                launcher.chmod(0o755)
                if any(dest.glob("*vcruntime*.dll")) or any(dest.glob("*msvcp*.dll")) or ((dest / "OpenSpideyRetarget.dll").exists() or (dest / "OpenSpideySfd.dll").exists()):
                    raise RuntimeError("Windows native dependencies leaked into the Linux output")
                results["games"].append({"game": name, "apphost_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(), "elf_x64": True})
        results["build_succeeded"] = True
        (out / "BUILD-RESULTS.json").write_text(json.dumps(results, indent=2) + "\n")
        print("Linux publication complete: " + str(out))
        print("Build success is not gameplay verification. Run the apphost with your own disc/data.")
        return 0
    except Exception as exc:
        results["build_succeeded"] = False; results["error"] = str(exc)
        (out / "BUILD-RESULTS.json").write_text(json.dumps(results, indent=2) + "\n")
        print("BUILD FAILED; see " + str(out / "build.log"), file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
