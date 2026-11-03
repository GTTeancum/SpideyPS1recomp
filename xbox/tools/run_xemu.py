#!/usr/bin/env python3
"""Boot the Xbox build in XEMU, hidden and silent, and take native screenshots.

    python -I xbox/tools/run_xemu.py --shots 5,10

The disc image goes to C:/Games/Emulators/Xemu/OpenSpidey.iso, replacing the
previous one: XEMU rejects disc images on a network share (an assertion on the
request alignment), and that folder holds exactly one OpenSpidey image. Settings,
the EEPROM copy, captures and logs stay in the run folder.

Packs xbox/build/release/default.xbe into a disc image with nxdk's extract-xiso,
writes an isolated xemu.toml (the user's boot ROM, BIOS and HDD image are only
read; HDD writes are discarded with -snapshot and the EEPROM is a copy), starts
XEMU with its window hidden and audio sent to a file, takes XEMU's own
screenshot at each requested second, then stops that emulator.
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

XBOX = Path(__file__).resolve().parents[1]
ROOT = XBOX.parent
SCREENSHOT = XBOX / "tools" / "xemu_native_screenshot.py"


def toml_path(path: Path) -> str:
    return str(path).replace("\\", "/")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--xemu", type=Path, default=ROOT / "private/work/xemu/xemu.exe")
    p.add_argument("--system", type=Path, default=Path("C:/Games/Emulators/Xemu"),
                   help="folder holding the user's MCPX, BIOS, eeprom.bin and HDD image (read only)")
    p.add_argument("--nxdk", type=Path, default=Path("C:/nxdk"))
    p.add_argument("--xbe", type=Path, default=XBOX / "build/release/default.xbe")
    p.add_argument("--out", type=Path, default=ROOT / "private/work/xbox-run",
                   help="run folder (settings, EEPROM copy, captures, logs)")
    p.add_argument("--iso", type=Path, default=Path("C:/Games/Emulators/Xemu/OpenSpidey.iso"),
                   help="local-drive disc image, replaced on every run")
    p.add_argument("--shots", default="8", help="comma-separated seconds after launch")
    p.add_argument("--scale", type=int, default=1)
    a = p.parse_args()

    out = a.out.resolve()
    if out.exists():
        shutil.rmtree(out)
    disc = out / "disc"
    disc.mkdir(parents=True)
    (out / "captures").mkdir()
    shutil.copy2(a.xbe, disc / "default.xbe")

    iso = a.iso.resolve()
    iso.unlink(missing_ok=True)
    xiso = a.nxdk / "tools/extract-xiso/build/extract-xiso.exe"
    r = subprocess.run([str(xiso), "-c", str(disc), str(iso)], cwd=out, capture_output=True, text=True)
    if r.returncode or not iso.exists():
        print(r.stdout, r.stderr, file=sys.stderr)
        return 1

    eeprom = out / "eeprom.bin"
    shutil.copy2(a.system / "eeprom.bin", eeprom)
    config = out / "xemu.toml"
    config.write_text(f"""[general]
show_welcome = false
skip_boot_anim = true
screenshot_dir = '{toml_path(out / "captures")}'

[display.quality]
surface_scale = {a.scale}

[display.window]
startup_size = '960x720'

[display.ui]
fit = 'scale'
aspect_ratio = '4x3'

[sys]
mem_limit = '64'

[sys.files]
bootrom_path = '{toml_path(a.system / "MCPX/mcpx_1.0.bin")}'
flashrom_path = '{toml_path(a.system / "BIOS/xbox-4627_debug.bin")}'
eeprom_path = '{toml_path(eeprom)}'
hdd_path = '{toml_path(a.system / "HDD/xbox_hdd.qcow2")}'
dvd_path = '{toml_path(iso)}'
""", encoding="utf-8")

    hidden = subprocess.STARTUPINFO()
    hidden.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    hidden.wShowWindow = 0  # SW_HIDE
    args = [str(a.xemu), "-config_path", str(config), "-snapshot",
            "-audio", f"driver=wav,path={toml_path(out / 'audio.wav')}"]
    with open(out / "stdout.log", "w") as so, open(out / "stderr.log", "w") as se:
        proc = subprocess.Popen(args, cwd=out, stdout=so, stderr=se, startupinfo=hidden,
                                creationflags=subprocess.CREATE_NO_WINDOW)
    started = time.monotonic()
    results = []
    try:
        for second in sorted(float(s) for s in a.shots.split(",") if s.strip()):
            delay = started + second - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            if proc.poll() is not None:
                results.append({"second": second, "ok": False, "detail": f"xemu exited {proc.returncode}"})
                break
            # XEMU sometimes drops a request while the previous PNG is being written.
            for attempt in range(3):
                shot = subprocess.run([sys.executable, "-I", str(SCREENSHOT), "--pid", str(proc.pid),
                                       "--xemu-exe", str(a.xemu), "--screenshot-dir", str(out / "captures"),
                                       "--timeout", "8"], capture_output=True, text=True)
                try:
                    record = json.loads(shot.stdout.strip().splitlines()[-1])
                except (ValueError, IndexError):
                    record = {"ok": False, "detail": (shot.stdout + shot.stderr).strip()}
                if record.get("ok"):
                    break
                time.sleep(1)
            record["second"] = second
            results.append(record)
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
    (out / "captures.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results))
    return 0 if results and all(r.get("ok") for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
