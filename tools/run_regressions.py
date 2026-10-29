#!/usr/bin/env python3
"""Build the shared tools and run regressions that require no retail game data."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
TESTS = ROOT / "tools" / "RecompOne" / "tests"
DOTNET_SUITES = (
    "BundledAssetsRegression",
    "CustomSuitModelRegression",
    "DiscInstallRegression",
    "InteriorHookRegression",
    "MemoryDiagnosticsRegression",
    "Sm1AnimationRegression",
    "VideoSetupRegression",
)
PYTHON_SUITES = {
    "Sm1ActorAnimationLayoutConverterRegression": "dreamcast/tools/test_sm1_actor_animation_layout.py",
}
SUITES = (*DOTNET_SUITES, *PYTHON_SUITES)


def run_step(name: str, command: list[str], output: Path) -> dict:
    log_path = output / f"{name}.log"
    print(f"\n[{name}] {' '.join(command)}", flush=True)
    start = time.monotonic()
    with log_path.open("w", encoding="utf-8") as log:
        try:
            process = subprocess.Popen(
                command,
                cwd=ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
            assert process.stdout is not None
            with process.stdout:
                for line in process.stdout:
                    log.write(line)
                    print(line, end="", flush=True)
            exit_code = process.wait()
        except OSError as error:
            message = f"Could not execute {command[0]}: {error}\n"
            log.write(message)
            print(message, end="", file=sys.stderr)
            exit_code = 127
    result = {
        "name": name,
        "command": command,
        "exit_code": exit_code,
        "status": "passed" if exit_code == 0 else "failed",
        "seconds": round(time.monotonic() - start, 3),
        "log": log_path.name,
    }
    print(f"[{name}] {result['status']} ({result['seconds']:.3f}s)", flush=True)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dotnet", default="dotnet", help=".NET 10 SDK executable")
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    parser.add_argument("--suite", choices=SUITES, action="append", help="Run only these suites (repeatable)")
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts" / "regressions")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    selected = list(dict.fromkeys(args.suite or SUITES))

    results = [run_step("shared-build", [
        args.dotnet, "build", "tools/RecompOne/RecompOne.sln",
        "--configuration", args.configuration, "--verbosity", "minimal",
    ], output)]
    shared_build_passed = results[0]["exit_code"] == 0
    for suite in selected:
        if suite in PYTHON_SUITES:
            results.append(run_step(suite, [sys.executable, PYTHON_SUITES[suite]], output))
        elif shared_build_passed:
            project = (TESTS / suite / f"{suite}.csproj").relative_to(ROOT)
            results.append(run_step(suite, [
                args.dotnet, "run", "--project", str(project),
                "--configuration", args.configuration, "--no-launch-profile",
                "--verbosity", "minimal",
            ], output))
        else:
            results.append({"name": suite, "status": "skipped", "reason": "shared build failed"})

    passed = all(result["status"] == "passed" for result in results)
    report = {
        "scope": "Shared runtime/recompiler build and synthetic regressions; no gameplay verification",
        "configuration": args.configuration,
        "status": "passed" if passed else "failed",
        "steps": results,
    }
    report_path = output / "results.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"\nAsset-free gate: {report['status']}. Results: {report_path}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
