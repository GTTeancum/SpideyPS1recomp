#!/usr/bin/env python3
"""Evaluate both game projects without restoring, compiling, or using disc data."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[3]
PROJECTS = (
    ROOT / "spiderman" / "port" / "SpiderMan.csproj",
    ROOT / "spiderman2" / "port" / "SpiderMan2.csproj",
)
NATIVE_DIRECTORY = ROOT / "tools" / "RecompOne" / "native" / "win-x64"
WINDOWS_LIBRARIES = {"msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"}


class GamePortConfigurationRegression(unittest.TestCase):
    dotnet = "dotnet"
    evaluations = 0

    def evaluate(self, project: Path, *, publishing: bool = False,
                 sdk_rid: str | None = None, runtime: str | None = None,
                 publish_dir: Path | None = None) -> dict:
        command = [
            self.dotnet, "msbuild", str(project), "-nologo",
            "-p:Configuration=Release",
            "-getProperty:RuntimeIdentifier,NETCoreSdkRuntimeIdentifier,PublishDir,"
            "OutputPath,SelfContained,PublishSingleFile,IncludeAllContentForSelfExtract,"
            "DefineConstants",
            "-getItem:Content",
        ]
        if publishing:
            # dotnet publish supplies this global property before project evaluation.
            command.append("-p:_IsPublishing=true")
        if sdk_rid is not None:
            # Simulate the other SDK host RID; this does not execute that platform.
            command.append(f"-p:NETCoreSdkRuntimeIdentifier={sdk_rid}")
        if runtime is not None:
            command.append(f"-p:RuntimeIdentifier={runtime}")
        if publish_dir is not None:
            command.append(f"-p:PublishDir={publish_dir}")
        result = subprocess.run(
            command, cwd=ROOT, capture_output=True, text=True,
            encoding="utf-8", stdin=subprocess.DEVNULL, timeout=60,
        )
        type(self).evaluations += 1
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return json.loads(result.stdout)

    def assert_native_content(self, evaluated: dict, effective_rid: str) -> None:
        items = [
            item for item in evaluated["Items"]["Content"]
            if Path(item["Identity"]).resolve().parent == NATIVE_DIRECTORY.resolve()
        ]
        expected = WINDOWS_LIBRARIES if effective_rid == "win-x64" else set()
        self.assertEqual({Path(item["Identity"]).name for item in items}, expected)
        for item in items:
            self.assertEqual(item["CopyToOutputDirectory"], "PreserveNewest")
            self.assertEqual(item["CopyToPublishDirectory"], "PreserveNewest")

    def assert_published(self, evaluated: dict, project: Path, runtime: str,
                         publish_dir: Path | None = None) -> None:
        properties = evaluated["Properties"]
        self.assertEqual(properties["RuntimeIdentifier"], runtime)
        self.assertEqual(properties["SelfContained"], "true")
        self.assertEqual(properties["PublishSingleFile"], "true")
        self.assertEqual(properties["IncludeAllContentForSelfExtract"], "true")
        self.assertIn("STANDALONE_DISTRIBUTION", properties["DefineConstants"].split(";"))
        expected_directory = publish_dir or project.parent / "dist" / runtime
        self.assertEqual(Path(properties["PublishDir"]).resolve(), expected_directory.resolve())
        self.assert_native_content(evaluated, runtime)

    def test_default_build_keeps_portable_output_and_selects_host_native_content(self):
        for project in PROJECTS:
            for sdk_rid in (None, "linux-x64", "win-x64"):
                with self.subTest(project=project.stem, sdk_rid=sdk_rid or "actual host"):
                    evaluated = self.evaluate(project, sdk_rid=sdk_rid)
                    properties = evaluated["Properties"]
                    self.assertEqual(properties["RuntimeIdentifier"], "")
                    self.assertEqual(properties["SelfContained"], "false")
                    self.assertNotIn("STANDALONE_DISTRIBUTION", properties["DefineConstants"].split(";"))
                    self.assertEqual(properties["OutputPath"].replace("\\", "/"), "bin/Release/net10.0/")
                    self.assert_native_content(evaluated, properties["NETCoreSdkRuntimeIdentifier"])

    def test_default_publish_uses_host_rid_and_separate_directory(self):
        for project in PROJECTS:
            for sdk_rid in (None, "linux-x64", "win-x64"):
                with self.subTest(project=project.stem, sdk_rid=sdk_rid or "actual host"):
                    evaluated = self.evaluate(project, publishing=True, sdk_rid=sdk_rid)
                    self.assert_published(evaluated, project, evaluated["Properties"]["NETCoreSdkRuntimeIdentifier"])

    def test_explicit_rid_overrides_host_for_build_and_publish(self):
        for project in PROJECTS:
            for sdk_rid, runtime in (("linux-x64", "win-x64"), ("win-x64", "linux-x64")):
                for publishing in (False, True):
                    with self.subTest(project=project.stem, sdk_rid=sdk_rid,
                                      runtime=runtime, publishing=publishing):
                        evaluated = self.evaluate(project, publishing=publishing,
                                                  sdk_rid=sdk_rid, runtime=runtime)
                        self.assertEqual(evaluated["Properties"]["RuntimeIdentifier"], runtime)
                        if publishing:
                            self.assert_published(evaluated, project, runtime)
                        else:
                            self.assert_native_content(evaluated, runtime)

    def test_explicit_publish_directory_is_preserved(self):
        for project in PROJECTS:
            with self.subTest(project=project.stem):
                publish_dir = ROOT / "artifacts" / "regressions" / "custom publish" / project.stem
                evaluated = self.evaluate(project, publishing=True, runtime="linux-x64",
                                          publish_dir=publish_dir)
                self.assert_published(evaluated, project, "linux-x64", publish_dir)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dotnet", default="dotnet", help=".NET 10 SDK executable")
    args = parser.parse_args()
    GamePortConfigurationRegression.dotnet = args.dotnet
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(GamePortConfigurationRegression)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    print(f"Evaluated {GamePortConfigurationRegression.evaluations} game project configurations; no build or restore.")
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
