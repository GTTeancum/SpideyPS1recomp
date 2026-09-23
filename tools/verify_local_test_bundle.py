"""Verify the portable ZIP in isolation using only hidden process-local input."""
import argparse
import atexit
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import shutil
import time
import zipfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--zip', type=Path, default=root/'proof_render/OpenSpidey-Gateway-Chase-Test.zip')
parser.add_argument('--output', type=Path, default=root/'proof_render/gateway-bundle-verification')
parser.add_argument('--game', choices=['both', 'sm1', 'sm2'], default='both')
args = parser.parse_args()
base = args.output.resolve()
proof_root = (root/'proof_render').resolve()
if base == proof_root or not base.is_relative_to(proof_root):
    raise SystemExit('Verification output must be a fresh folder inside proof_render')
if shutil.disk_usage(proof_root).free < 5*1024**3:
    raise SystemExit('At least 5 GiB free is required for portable extraction and verification')
base.mkdir(exist_ok=False)
proc = None

def cleanup():
    if proc is not None and proc.poll() is None:
        proc.kill()
        proc.wait()
    for name in ['extracted', 'cache']:
        target = base/name
        if target.exists():
            resolved = target.resolve()
            if resolved.parent != base or not resolved.is_relative_to(proof_root):
                raise RuntimeError(f'Refusing cleanup outside owned verification folder: {resolved}')
            shutil.rmtree(target)

atexit.register(cleanup)
with zipfile.ZipFile(args.zip) as archive:
    for entry in archive.infolist():
        path = PurePosixPath(entry.filename.replace('\\', '/'))
        if path.is_absolute() or '..' in path.parts or ':' in entry.filename:
            raise ValueError(f'Unsafe ZIP entry: {entry.filename}')
    manifest = json.loads(archive.read('BUILD.json'))
    archive.extractall(base/'extracted')
for name, folder, executable, frames, shot in [
    ('sm1', 'Spider-Man', 'SpiderMan.exe', 1800, 'l5a1_t.trg+580'),
    ('sm2', 'Spider-Man 2', 'SpiderMan2.exe', 5000, 4500)]:
    if args.game not in ['both', name]:
        continue
    run = base/name
    run.mkdir()
    # Mute only the scratch verification copy, keeping the real mixer/device
    # path active. The packaged game stays audible for the user's own play.
    settings_path = base/'extracted'/folder/'settings.json'
    settings = json.loads(settings_path.read_text())
    settings['Muted'] = True
    settings_path.write_text(json.dumps(settings))
    launcher = next((base/'extracted').glob('1 - *.cmd' if name == 'sm1' else '2 - *.cmd'))
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SPIDEY_', 'RECOMP_', 'DOTNET_', 'COMPlus_'))}
    for key, value in re.findall(r'^set "([^=]+)=(.*)"$', launcher.read_text(), re.M):
        if value:
            env[key] = value
        else:
            env.pop(key, None)
    env.update(RECOMP_CAPTURE_HIDDEN='1', RECOMP_INSTALL_HEADLESS='1',
               RECOMP_AUDIO_STATE_TRACE=str(run/'audio-state.jsonl'),
               SPIDEY_SCRIPT_EXCLUSIVE='1', SPIDEY_EXIT=str(frames),
               SPIDEY_SHOTS=str(shot), SPIDEY_CAPTURE_PRESENTED='1',
               SPIDEY_SHOT_DIR=str(run), SPIDEY_LOG_DIR=str(run),
               DOTNET_BUNDLE_EXTRACT_BASE_DIR=str(base/'cache'))
    if name == 'sm2':
        env.update(SPIDEY_BOOT_SKIP_UNTIL='title.bmr', SPIDEY_SCRIPT=
                   'title.bmr+80:start:10;title.bmr+280:cross:10;title.bmr+480:cross:10;title.bmr+700:cross:10;'
                   'e1m0_t.trg+1200:cross:8;e1m0_t.trg+1500:cross:8;e1m0_t.trg+1800:cross:8;'
                   'e1m0_t.trg+2100:cross:8;e1m0_t.trg+2400:cross:8')
    (run/'test-environment.json').write_text(json.dumps({k:v for k,v in env.items() if k.startswith(('SPIDEY_', 'RECOMP_', 'DOTNET_'))}, indent=2))
    start = time.monotonic()
    with (run/'console.log').open('w') as log:
        proc = subprocess.Popen([str(base/'extracted'/folder/executable)], cwd=base, env=env,
                                stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        print(f'{name}: pid={proc.pid}', flush=True)
        try:
            code = proc.wait(timeout=210)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            raise
    actual_hash = json.loads((run/'performance-system.json').read_text())['executableSha256'].lower()
    expected_hash = manifest['files'][f'{folder}/{executable}']['sha256'].lower()
    result = {'exit':code, 'seconds':time.monotonic()-start, 'images':[p.name for p in run.glob('*.png')],
              'speaker_muted':True, 'requires_visual_review':True,
              'source_zip':str(args.zip.resolve()), 'executable_sha256':actual_hash,
              'matches_manifest':actual_hash == expected_hash}
    (run/'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)
    if code or not result['images'] or not result['matches_manifest']:
        raise RuntimeError(f'{name} did not complete; inspect logs')
