"""Stage tested native publishes with local disc data; never replace an old stage."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('publish_root', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
source = args.publish_root.resolve(strict=True)
output = args.output.resolve()
if output.exists() or output == source or output.is_relative_to(source):
    raise SystemExit('Choose a new stage outside the publish folder')

def relative(name):
    p = PurePosixPath(name.replace('\\', '/'))
    if p.is_absolute() or '..' in p.parts or ':' in name:
        raise ValueError(f'Unsafe payload path: {name}')
    return Path(*p.parts)

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

plans = []
for short, project, folder, exe in [
    ('sm1', 'spiderman', 'Spider-Man', 'SpiderMan.exe'),
    ('sm2', 'spiderman2', 'Spider-Man 2', 'SpiderMan2.exe')]:
    publish = source / short
    disc = ROOT / project / 'extracted'
    if not (publish / exe).is_file() or not (publish / 'mods/suits').is_dir():
        raise SystemExit(f'Incomplete publish: {publish}')
    metadata = json.loads((disc / 'recompone-disc.json').read_text())
    files = [(disc / relative(item['path']), relative(item['path']))
             for item in metadata['files']]
    if any(not path.is_file() for path, _ in files):
        raise SystemExit(f'Incomplete local disc: {disc}')
    plans.append((publish, disc, folder, files))

output.mkdir(parents=True)
report = {'status': 'candidate; native stage validation pending',
          'source_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT,
                                                 text=True).strip(),
          'publish_root': str(source), 'files': {}}
for publish, disc, folder, files in plans:
    target = output / folder
    shutil.copytree(publish, target)
    for path, rel in [(disc / 'recompone-disc.json', Path('recompone-disc.json'))] + files:
        dest = target / 'game' / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, dest)
        if digest(path) != digest(dest):
            raise ValueError(f'Disc copy mismatch: {rel}')
    (target / 'settings.json').write_text(json.dumps({
        'CdPath': 'game', 'Widescreen': True, 'CardAEnabled': False, 'CardBEnabled': False,
    }, indent=2) + '\n')
    (target / 'interface.ini').write_text(
        '[RecompOne]\nFullscreen=False\nWindowWidth=1280\nWindowHeight=720\n'
        'RenderScale=4\nFxaa=True\nVSync=False\nPanels.Output=True\n')
    for path in sorted(target.rglob('*')):
        if not path.is_file():
            continue
        checksum = digest(path)
        rel = path.relative_to(output).as_posix()
        report['files'][rel] = {'bytes': path.stat().st_size, 'sha256': checksum}
        original = publish / path.relative_to(target)
        if original.is_file():
            if digest(original) != checksum:
                raise ValueError(f'Publish copy mismatch: {rel}')
(output / 'BUILD.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'stage': str(output), 'files': len(report['files'])}))
