"""Bounded game-process capture using the existing in-process input harness."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--suits', type=Path, required=True)
    parser.add_argument('--id', required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--level', default='l1a1')
    parser.add_argument('--frames', type=int, default=2400)
    parser.add_argument('--script', default='')
    parser.add_argument('--shots', default='495,800')
    args = parser.parse_args()
    offsets=sorted(set(int(offset) for offset in args.shots.split(',')))
    # Bound legacy batch requests too: one swing frame and one post-landing frame.
    if len(offsets)>2:
        offsets=[min(offsets,key=lambda offset:abs(offset-495)),offsets[-1]]
        print('Capture budget: retaining only offsets '+','.join(map(str,offsets)),flush=True)
    build, data, suits, out = (p.resolve() for p in (args.build, args.data, args.suits, args.out))
    if out.exists():
        parser.error('Choose a fresh evidence directory')
    manifest = json.loads((suits / args.id / 'suit.json').read_text())
    if manifest['id'] != args.id:
        parser.error('Suit identity mismatch')
    runtime = out / 'runtime'
    shutil.copytree(build, runtime, ignore=shutil.ignore_patterns('*.sav', 'settings.json', 'interface.ini', 'mods'))
    fixture = out / 'suits'
    shutil.copytree(suits / args.id, fixture / args.id)
    (fixture / 'selected-suit.txt').write_text(args.id + '\n')
    (runtime / 'settings.json').write_text(json.dumps(dict(CdPath=str(data), CardAEnabled=False,
        CardBEnabled=False, Widescreen=True, Muted=True)))
    (runtime / 'interface.ini').write_text('[RecompOne]\nFullscreen=False\nWindowWidth=1280\nWindowHeight=720\nRenderScale=3\nFxaa=True\nVSync=False\n')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SPIDEY_', 'RECOMP_'))}
    script = 'title.bmr+120:start:12;title.bmr+420:cross:12;title.bmr+720:cross:12'
    if args.script:
        script += ';' + args.script
    env.update(RECOMP_CAPTURE_HIDDEN='1', RECOMP_RENDER_SCALE='3', SPIDEY_WIDE='1',
        SPIDEY_SCRIPT_EXCLUSIVE='1', SPIDEY_SCRIPT=script, SPIDEY_LEVEL=args.level,
        SPIDEY_BOOT_SKIP_UNTIL='title.bmr', SPIDEY_SUIT_MOD_DIR=str(fixture),
        SPIDEY_RETARGET_TRACE='1', SPIDEY_MOD_TRACE='1', SPIDEY_SHOT_DIR=str(out),
        SPIDEY_SHOTS=','.join(f'{args.level}_t.trg+{offset}' for offset in offsets),
        SPIDEY_CAPTURE_PRESENTED='1', SPIDEY_EXIT=str(args.frames), SPIDEY_LOG_DIR=str(out),
        SPIDEY_STALL='20')
    executable = runtime / 'SpiderMan.exe'
    record = dict(suit=args.id, level=args.level, executableSha256=hashlib.sha256((runtime / 'SpiderMan.dll').read_bytes()).hexdigest(),
        actorSha256=hashlib.sha256((fixture / args.id / 'actor.psx').read_bytes()).hexdigest(),
        capture='Game framebuffer; process-local input only; no desktop capture or OS input', environment=env)
    # Record only this harness's variables, never the user's inherited environment.
    record['environment'] = {k: v for k, v in env.items() if k.startswith(('SPIDEY_', 'RECOMP_'))}
    start = time.monotonic()
    with (out / 'game.log').open('w') as log:
        process = subprocess.Popen([str(executable), str(data)], cwd=runtime, env=env,
            stdout=log, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        try:
            record['exitCode'] = process.wait(timeout=180)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            record.update(exitCode=process.returncode, timedOut=True)
    diagnostics=out/'spidey.log'
    record['slowRunWarnings']=diagnostics.read_text(errors='replace').count('======== STALL ========') if diagnostics.exists() else 0
    record.update(seconds=time.monotonic() - start, images=[p.name for p in out.glob('*.png')],
        acceptance='Capture pending visual and gameplay review')
    (out / 'run.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps({k: v for k, v in record.items() if k != 'environment'}, indent=2))
    return 0 if record['exitCode'] == 0 else 1


if __name__ == '__main__':
    raise SystemExit(main())
