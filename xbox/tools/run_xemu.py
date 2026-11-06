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
import re
import shutil
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

XBOX = Path(__file__).resolve().parents[1]
ROOT = XBOX.parent
SCREENSHOT = XBOX / "tools" / "xemu_native_screenshot.py"


def keep_hidden(pid: int, stop: threading.Event) -> None:
    """Hide every window the process shows, for as long as it runs.

    The SW_HIDE start-up hint only covers a program's first ShowWindow with the default
    show command; XEMU (SDL) shows its window explicitly, so the hint alone can let it
    appear on the user's screen."""
    import ctypes
    from ctypes import wintypes
    user32 = ctypes.windll.user32
    enum_proc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    def hide(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            user32.ShowWindow(hwnd, 0)  # SW_HIDE
        return True

    callback = enum_proc(hide)
    while not stop.is_set():
        user32.EnumWindows(callback, 0)
        stop.wait(0.05)


def toml_path(path: Path) -> str:
    return str(path).replace("\\", "/")


def read_status(port: int, map_path: Path, target: Path) -> str | None:
    """Copy the runtime's status text (g_status_text) out through the XEMU monitor."""
    if not map_path.exists():
        return None
    m = re.search(r"_g_status_text\s+([0-9a-fA-F]+)", map_path.read_text(errors="replace"))
    if not m:
        return None
    raw = target.with_suffix(".bin")
    raw.unlink(missing_ok=True)
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=5) as monitor:
            monitor.settimeout(5)
            monitor.recv(4096)
            monitor.sendall(f'memsave 0x{int(m[1], 16):x} 16384 "{toml_path(raw)}"\n'.encode())
            time.sleep(1)
    except OSError as error:
        return f"monitor: {error}"
    if not raw.exists():
        return None
    text = raw.read_bytes().split(b"\0", 1)[0].decode("ascii", "replace")
    raw.unlink()
    target.write_text(text, encoding="utf-8")
    return str(target)


def save_ram(port: int, map_path: Path, target: Path) -> None:
    """Copy the PlayStation's RAM (recomp_ram, 8 MB with the expanded area) out through the XEMU monitor."""
    m = re.search(r"_recomp_ram\s+([0-9a-fA-F]+)", map_path.read_text(errors="replace")) if map_path.exists() else None
    if not m:
        return
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=5) as monitor:
            monitor.settimeout(5)
            monitor.recv(4096)
            monitor.sendall(f'memsave 0x{int(m[1], 16):x} 8388608 "{toml_path(target)}"\n'.encode())
            time.sleep(2)
    except OSError:
        pass


def monitor_command(port: int, command: str, target: Path) -> None:
    """Run one XEMU monitor command and save what it prints."""
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=5) as monitor:
            monitor.settimeout(2)
            monitor.recv(4096)
            monitor.sendall((command + "\n").encode())
            time.sleep(1)
            chunks = []
            try:
                while True:
                    chunk = monitor.recv(65536)
                    if not chunk:
                        break
                    chunks.append(chunk)
            except OSError:
                pass
        target.write_bytes(b"".join(chunks))
    except OSError as error:
        target.write_text(f"monitor: {error}\n")


def enable_hd(eeprom: Path, allow_720p: bool) -> None:
    """Allow 480p (and 720p) and mark the TV widescreen in an EEPROM copy.

    The user settings section is 0x60..0xBF: a checksum, then the settings it covers;
    the video flags are at 0x94."""
    data = bytearray(eeprom.read_bytes())
    flags = int.from_bytes(data[0x94:0x98], "little") & ~0x00020000
    flags |= 0x00010000 | 0x00080000 | (0x00020000 if allow_720p else 0)
    data[0x94:0x98] = flags.to_bytes(4, "little")
    total = 0
    for i in range(0x64, 0xC0, 4):
        total += int.from_bytes(data[i:i + 4], "little")
        if total > 0xFFFFFFFF:
            total = (total & 0xFFFFFFFF) + 1
    data[0x60:0x64] = (~total & 0xFFFFFFFF).to_bytes(4, "little")
    eeprom.write_bytes(bytes(data))


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
    p.add_argument("--monitor-port", type=int, default=9251)
    p.add_argument("--include", type=Path, action="append", default=[],
                   help="file or folder copied to the disc root (the player's own game files; repeatable)")
    p.add_argument("--scale", type=int, default=1)
    p.add_argument("--registers", action="store_true", help="also save the CPU registers at each shot")
    p.add_argument("--hd480", action="store_true",
                   help="HD cable and a widescreen TV allowing 480p but not 720p (16:9 at 640x480)")
    p.add_argument("--hd", action="store_true",
                   help="HD cable and a widescreen TV allowing 480p and 720p (set on the EEPROM copy)")
    p.add_argument("--ram", action="store_true", help="also save PlayStation RAM at each shot (ram-<second>.bin)")
    a = p.parse_args()

    out = a.out.resolve()
    if out.exists():
        shutil.rmtree(out)
    disc = out / "disc"
    disc.mkdir(parents=True)
    (out / "captures").mkdir()
    shutil.copy2(a.xbe, disc / "default.xbe")
    for item in a.include:
        if item.is_dir():
            shutil.copytree(item, disc / item.name)
        else:
            shutil.copy2(item, disc / item.name)

    iso = a.iso.resolve()
    iso.unlink(missing_ok=True)
    xiso = a.nxdk / "tools/extract-xiso/build/extract-xiso.exe"
    r = subprocess.run([str(xiso), "-c", str(disc), str(iso)], cwd=out, capture_output=True, text=True)
    if r.returncode or not iso.exists():
        print(r.stdout, r.stderr, file=sys.stderr)
        return 1

    eeprom = out / "eeprom.bin"
    shutil.copy2(a.system / "eeprom.bin", eeprom)
    if a.hd or a.hd480:
        enable_hd(eeprom, allow_720p=a.hd)
    config = out / "xemu.toml"
    config.write_text(f"""[general]
show_welcome = false
skip_boot_anim = true
screenshot_dir = '{toml_path(out / "captures")}'

[display.quality]
surface_scale = {a.scale}

[display.window]
startup_size = '{"1280x720" if a.hd else "960x720"}'

[display.ui]
fit = 'scale'
aspect_ratio = '{"16x9" if a.hd or a.hd480 else "4x3"}'

[sys]
mem_limit = '64'
{"avpack = 'hdtv'" if a.hd or a.hd480 else ""}

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
            "-monitor", f"tcp:127.0.0.1:{a.monitor_port},server,nowait",
            "-audio", f"driver=wav,path={toml_path(out / 'audio.wav')}"]
    with open(out / "stdout.log", "w") as so, open(out / "stderr.log", "w") as se:
        proc = subprocess.Popen(args, cwd=out, stdout=so, stderr=se, startupinfo=hidden,
                                creationflags=subprocess.CREATE_NO_WINDOW)
    stop_hiding = threading.Event()
    threading.Thread(target=keep_hidden, args=(proc.pid, stop_hiding), daemon=True).start()
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
            record["status"] = read_status(a.monitor_port, a.xbe.parent.parent / "OpenSpidey.map",
                                           out / "captures" / f"status-{int(second)}.txt")
            if a.registers:
                monitor_command(a.monitor_port, "info registers", out / "captures" / f"registers-{int(second)}.txt")
                monitor_command(a.monitor_port, "x /64wx $esp", out / "captures" / f"stack-{int(second)}.txt")
            if a.ram:
                save_ram(a.monitor_port, a.xbe.parent.parent / "OpenSpidey.map", out / "captures" / f"ram-{int(second)}.bin")
            results.append(record)
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        stop_hiding.set()
    (out / "captures.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results))
    return 0 if results and all(r.get("ok") for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
