#!/usr/bin/env python3
"""Build the unchanged retarget ABI for a selected target; no game code is generated."""
import argparse
import ctypes
import hashlib
import json
import pathlib
import platform
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent

def commands_for(target, out, cxx="clang++", linker="lld-link", freestanding_linux=False):
    src = ROOT / "native" / "retarget_core.cpp"
    base = [cxx, "-O2", "-std=c++17", "-fno-exceptions", "-fno-rtti",
            "-fno-math-errno", "-ffp-contract=off", "-fno-stack-protector"]
    result = []
    if target in ("all", "linux-x64"):
        linux_flags = ["-nostdlib", "-fuse-ld=lld", "-Wl,--no-undefined"] if freestanding_linux else []
        result.append(base + ["--target=x86_64-unknown-linux-gnu", "-shared", "-fPIC",
                            *linux_flags, str(src), "-o", str(out / "libOpenSpideyRetarget.so")])
    if target in ("all", "win-x64"):
        result.append(base + ["--target=x86_64-pc-windows-msvc", "-c", str(src),
                              "-o", str(out / "retarget_core.obj")])
        result.append([linker, "/dll", "/noentry", "/nodefaultlib", "/machine:x64",
                       str(out / "retarget_core.obj"), "/out:" + str(out / "OpenSpideyRetarget.dll")])
    return result

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--target", choices=["auto", "all", "linux-x64", "win-x64"], default="auto")
    ap.add_argument("--out", type=pathlib.Path, default=ROOT / "native" / "bin")
    ap.add_argument("--freestanding-linux", action="store_true", help="Use LLVM ELF linker without a sysroot; inspect exports, do not imply Linux execution")
    args = ap.parse_args()
    target = args.target
    if target == "auto":
        if platform.machine().lower() not in ("x86_64", "amd64"):
            ap.error("auto target requires an x86-64 host")
        target = {"Linux": "linux-x64", "Windows": "win-x64"}.get(platform.system())
        if target is None:
            ap.error("unsupported native host; select a configured cross toolchain explicitly")
    cxx = shutil.which("clang++")
    linker = shutil.which("lld-link")
    if not cxx:
        ap.error("clang++ is required")
    if target in ("all", "win-x64") and not linker:
        ap.error("lld-link is required for the Windows target")
    if args.freestanding_linux and target not in ("all", "linux-x64"):
        ap.error("--freestanding-linux requires a Linux target")
    inspector = shutil.which("llvm-readobj")
    if args.freestanding_linux and (not shutil.which("ld.lld") or not inspector):
        ap.error("freestanding Linux cross-build requires ld.lld and llvm-readobj")
    if target in ("all", "linux-x64") and platform.system() != "Linux" and not args.freestanding_linux:
        ap.error("Linux core rebuild requires a Linux toolchain/sysroot; use the supplied .so for cross publication")
    args.out.mkdir(parents=True, exist_ok=True)
    for cmd in commands_for(target, args.out.resolve(), cxx, linker, args.freestanding_linux):
        print(" ".join(str(s) for s in cmd), flush=True)
        subprocess.run(cmd, check=True)
    if target in ("all", "linux-x64"):
        library = args.out.resolve() / "libOpenSpideyRetarget.so"
        inspection = None
        if args.freestanding_linux:
            inspection = json.loads(subprocess.check_output([inspector, "--elf-output-style=JSON",
                "--needed-libs", "--dyn-symbols", str(library)], text=True))[0]
            symbols = [item['Symbol'] for item in inspection['DynamicSymbols'] if item['Symbol']['Name']['Name']]
            expected = {'rtg_abi', 'rtg_validate', 'rtg_pose', 'rtg_evaluate', 'rtg_to_part', 'rtg_web_target', 'rtg_web_segment'}
            if (inspection['FileSummary']['Format'] != 'elf64-x86-64' or inspection['NeededLibraries'] or
                {s['Name']['Name'] for s in symbols} != expected or any(s['Section']['Value'] == 0 for s in symbols)):
                raise RuntimeError('Invalid freestanding core exports/dependencies')
        abi = None
        if platform.system() == 'Linux':
            core = ctypes.CDLL(str(library)); core.rtg_abi.restype = ctypes.c_uint32
            abi = '0x%08x' % core.rtg_abi()
        project = ROOT.parents[1]
        source = ROOT / "native" / "retarget_core.cpp"
        paths = [source, library]
        # A custom build directory gets local provenance, not a manifest falsely
        # claiming that the default shipping path was rebuilt.
        entries = [{"path": str(p.relative_to(project)) if p.is_relative_to(project) else str(p),
                    "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths]
        metadata = {"target": "linux-x64", "abi": abi, "runtimeExecuted": abi is not None,
                    "freestanding": args.freestanding_linux, "inspection": inspection,
                    "boundary": "Native library build only; not a complete game build.", "files": entries}
        (args.out / "build-provenance.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print("Built native core target: " + target)

if __name__ == "__main__":
    main()
