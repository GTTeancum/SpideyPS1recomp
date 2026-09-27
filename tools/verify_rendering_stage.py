"""Run staged executables in place with hidden, process-local menu fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stage', type=Path)
parser.add_argument('evidence', type=Path)
parser.add_argument('--game', choices=['sm1', 'sm2', 'both'], default='both')
args = parser.parse_args()
stage = args.stage.resolve(strict=True)
evidence = args.evidence.resolve()
manifest = json.loads((stage / 'BUILD.json').read_text())

def verify_files():
    for rel, expected in manifest['files'].items():
        path = (stage / rel).resolve(strict=True)
        if not path.is_relative_to(stage):
            raise ValueError(f'Path outside stage: {rel}')
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        if digest != expected['sha256'] or path.stat().st_size != expected['bytes']:
            raise ValueError(f'Stage mismatch: {rel}')

verify_files()
evidence.mkdir(parents=True, exist_ok=False)
for game, folder, exe, frames, shots, script, level in [
    ('sm1', 'Spider-Man', 'SpiderMan.exe', 4400,
     '1736,2050,2140,2380,2600,3500,4200',
     'title.bmr+120:start:12;title.bmr+420:cross:12;title.bmr+720:cross:12;'
     'title.bmr+1100:cross:12;2260:down:10;2340:up:10;2420:cross:10;'
     '2800:start:10;3100:cross:10;3300:up:60', 'l5a1'),
    ('sm2', 'Spider-Man 2', 'SpiderMan2.exe', 5200,
     '4500,4800,5100',
     'title.bmr+80:start:10;title.bmr+280:cross:10;title.bmr+480:cross:10;'
     'title.bmr+700:cross:10;e1m0_t.trg+1200:cross:8;'
     'e1m0_t.trg+1500:cross:8;e1m0_t.trg+1800:cross:8;'
     'e1m0_t.trg+2100:cross:8;e1m0_t.trg+2400:cross:8;'
     '4600:start:10;4900:cross:10', 'e1m0'),
]:
    if args.game not in ['both', game]:
        continue
    target = stage / folder
    run = evidence / game
    run.mkdir()
    settings_path = target / 'settings.json'
    original = settings_path.read_bytes()
    interface_path = target / 'interface.ini'
    original_interface = interface_path.read_bytes()
    settings = json.loads(original)
    settings['Muted'] = True
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('SPIDEY_', 'RECOMP_', 'DOTNET_', 'COMPlus_'))}
    env.update(RECOMP_CAPTURE_HIDDEN='1', SPIDEY_SCRIPT_EXCLUSIVE='1',
               SPIDEY_BOOT_SKIP_UNTIL='title.bmr', SPIDEY_LEVEL=level,
               SPIDEY_SCRIPT=script, SPIDEY_SHOTS=shots, SPIDEY_EXIT=str(frames),
               SPIDEY_CAPTURE_PRESENTED='1', SPIDEY_SHOT_DIR=str(run),
               SPIDEY_LOG_DIR=str(run), DOTNET_BUNDLE_EXTRACT_BASE_DIR=str(run / 'runtime'))
    (run / 'environment.json').write_text(json.dumps({k: v for k, v in env.items()
        if k.startswith(('SPIDEY_', 'RECOMP_', 'DOTNET_'))}, indent=2))
    result = {'visual_review': 'pending', 'exe': str(target / exe)}
    try:
        settings_path.write_text(json.dumps(settings))
        with (run / 'console.log').open('w') as log:
            with subprocess.Popen([str(target / exe)], cwd=target, env=env,
                                  stdout=log, stderr=subprocess.STDOUT,
                                  creationflags=subprocess.CREATE_NO_WINDOW) as proc:
                print(f'{game}: pid={proc.pid}', flush=True)
                try:
                    result['exit'] = proc.wait(timeout=300)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    result.update(exit=proc.wait(), timeout=True)
    finally:
        settings_path.write_bytes(original)
        interface_path.write_bytes(original_interface)
        (run / 'result.json').write_text(json.dumps(result, indent=2))
    verify_files()
    if result['exit'] != 0:
        raise SystemExit(f'{game} did not exit cleanly')
    print(f'{game}: exit 0; all staged input hashes unchanged; visual review pending', flush=True)
