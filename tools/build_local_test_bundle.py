"""Build the user's private, portable two-game test ZIP from local game data.

No downloads, uploads, or source-data changes. WAD entries come directly from
the local original archive, avoiding experimental loose override directories.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
args = parser.parse_args()
manifest = {'purpose': 'Private laptop test candidate; P0/P1 remain open',
            'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
            'uncommitted_changes': True, 'files': {}}


def safe_path(name):
    path = PurePosixPath(name.replace('\\', '/'))
    if path.is_absolute() or '..' in path.parts or ':' in name:
        raise ValueError(f'Unsafe archive path: {name}')
    return str(path)


def launcher(game, executable, chase=False):
    # Clear the whole diagnostic namespaces before applying this launcher's
    # settings. A fixed list silently misses newly added capture/input probes.
    clear = [f'for /f "tokens=1 delims==" %%V in (\'set {prefix} 2^>nul\') do set "%%V="'
             for prefix in ('SPIDEY_', 'RECOMP_')]
    env = dict(RECOMP_CAPTURE_HIDDEN='0', RECOMP_PERF_LOG='1', RECOMP_PERF_PHASES='1',
               RECOMP_AUDIO_STATE_TRACE='audio-state.jsonl', SPIDEY_SCRIPT_EXCLUSIVE='0')
    if chase:
        env.update(SPIDEY_LEVEL='l5a1', SPIDEY_BOOT_SKIP_UNTIL='title.bmr',
                   SPIDEY_SCRIPT='title.bmr+120:start:12;title.bmr+420:cross:12;'
                                 'title.bmr+720:cross:12;title.bmr+1100:cross:12')
    return '\r\n'.join(['@echo off', 'setlocal', f'cd /d "%~dp0{game}"'] + clear +
        [f'set "{key}={value}"' for key, value in env.items()] +
        [f'start "" "{executable}"', 'endlocal', ''])


with zipfile.ZipFile(args.output, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=3) as archive:
    def add(name, source):
        name = safe_path(name)
        if name.casefold() in names:
            raise ValueError(f'Duplicate path: {name}')
        names.add(name.casefold())
        digest = hashlib.sha256()
        length = 0
        with (source.open('rb') if isinstance(source, Path) else io.BytesIO(source)) as inp:
            with archive.open(name, 'w', force_zip64=True) as out:
                while block := inp.read(1024*1024):
                    digest.update(block)
                    length += len(block)
                    out.write(block)
        manifest['files'][name] = {'bytes': length, 'sha256': digest.hexdigest()}

    names = set()
    for project, folder, short, executable in [
        ('spiderman', 'Spider-Man', 'sm1', 'SpiderMan.exe'),
        ('spiderman2', 'Spider-Man 2', 'sm2', 'SpiderMan2.exe')]:
        print(f'Packaging {folder}', flush=True)
        source = ROOT/project/'extracted'
        add(f'{folder}/{executable}', ROOT/'proof_render/performance-repairs'/short/executable)
        add(f'{folder}/game/recompone-disc.json', source/'recompone-disc.json')
        disc = json.loads((source/'recompone-disc.json').read_text())
        for entry in disc['files']:
            relative = safe_path(entry['path'])
            add(f'{folder}/game/{relative}', source/relative)
        # Match the original extractor's last-entry-wins filename semantics.
        hed = (source/'CD.HED').read_bytes()
        offset, entries = 0, {}
        while offset < len(hed):
            end = hed.find(b'\0', offset)
            if end < 0 or end == offset:
                break
            name = safe_path(hed[offset:end].decode('latin-1'))
            record = offset + ((end-offset+4) & ~3)
            if record+8 > len(hed):
                raise ValueError('Truncated CD.HED record')
            position, length = struct.unpack_from('<II', hed, record)
            entries[name.casefold()] = (name, position, length)
            offset = record+8
        with (source/'CD.WAD').open('rb') as wad:
            for name, position, length in entries.values():
                wad.seek(position)
                data = wad.read(length)
                if len(data) != length:
                    raise ValueError(f'Truncated WAD entry: {name}')
                add(f'{folder}/game/wad/{name}', data)
        payload = ROOT/project/'port/bundled/runtime-assets.zip'
        with zipfile.ZipFile(payload) as bundled:
            for entry in bundled.infolist():
                if not entry.is_dir():
                    add(f'{folder}/assets/builtin/{safe_path(entry.filename)}', bundled.read(entry))
        add(f'{folder}/assets/builtin/.payload-sha256', hashlib.sha256(payload.read_bytes()).hexdigest().upper().encode())
        add(f'{folder}/settings.json', json.dumps({'CdPath': 'game', 'Widescreen': True,
            'CardAEnabled': False, 'CardBEnabled': False}).encode())
        add(f'{folder}/interface.ini', b'[RecompOne]\r\nFullscreen=False\r\nWindowWidth=1280\r\nWindowHeight=720\r\nRenderScale=4\r\nFxaa=True\r\nVSync=False\r\nPanels.Output=True\r\n')
    add('1 - Play Chase Venom.cmd', launcher('Spider-Man', 'SpiderMan.exe', chase=True).encode())
    add('2 - Play Spider-Man 2.cmd', launcher('Spider-Man 2', 'SpiderMan2.exe').encode())
    add('3 - Play Spider-Man normally.cmd', launcher('Spider-Man', 'SpiderMan.exe').encode())
    add('START HERE.txt', (
        'Extract this entire ZIP to a writable folder.\r\n\r\n'
        'Open "1 - Play Chase Venom.cmd". Startup advances automatically into Chase Venom; '
        'wait for the chase scene, then play normally. No follower or forced triggers are enabled.\r\n'
        'Open "2 - Play Spider-Man 2.cmd" for the second game, or launcher 3 for normal SM1 startup.\r\n'
        'Your local game data and bundled assets are included. No disc selection or installation is needed.\r\n\r\n'
        'These test copies start fresh with memory-card saves disabled. '
        'Use a controller or the configured keyboard controls (arrows, Z/X/A/S, Q/W/E/R, Enter).\r\n'
        'The shared cap is 30 FPS; initial settings are 4x rendering, FXAA, 1280x720 window, widescreen.\r\n'
        'Detailed performance logs appear beside each executable. After a problem, note the time and action, '
        'exit, and preserve performance-*.json*, audio-state.jsonl, spidey.log, and your notes before another launch.\r\n'
        'This is a diagnostic candidate. Remaining stalls and Chase issues are still being investigated.\r\n'
    ).encode())
    archive.writestr('BUILD.json', json.dumps(manifest, indent=2))

print('Verifying every packaged file', flush=True)
with zipfile.ZipFile(args.output) as archive:
    for name, expected in manifest['files'].items():
        digest = hashlib.sha256()
        length = 0
        with archive.open(name) as stream:
            while block := stream.read(1024*1024):
                digest.update(block)
                length += len(block)
        if digest.hexdigest() != expected['sha256'] or length != expected['bytes']:
            raise ValueError(f'Archive verification failed: {name}')
print(json.dumps({'zip': str(args.output.resolve()), 'bytes': args.output.stat().st_size,
                  'files_verified': len(manifest['files'])}), flush=True)
