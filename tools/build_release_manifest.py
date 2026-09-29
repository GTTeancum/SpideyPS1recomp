"""Inventory release candidates without copying local disc data or user state."""
import argparse
import hashlib
import json
import json5
from datetime import datetime, timezone
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stage', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
stage = args.stage.resolve(strict=True)
output = args.output.resolve()
if output.exists():
    raise SystemExit('Choose a new manifest directory; existing manifests are preserved')
root = Path(__file__).resolve().parents[1]
manifest = {
    'schema_version': 1,
    'status': 'draft inventory; no release archive created or uploaded',
    'release_version': None,
    'created_utc': datetime.now(timezone.utc).isoformat(),
    'inventory_source_head': subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    'source_stage': str(stage),
    'provenance_note': 'Source HEAD describes inventory time, not necessarily both executable builds. SHA256 identifies the actual staged binaries.',
    'excluded': {
        'game/': 'Locally extracted retail disc data; installed by the end user.',
        'assets/': 'Generated from the executable bundled payload.',
        '*.sav': 'User memory cards.',
        'settings.json': 'User settings and local disc paths.',
        'interface.ini': 'User window and renderer settings.',
        'BUILD.json': 'Historical test-stage inventory, superseded for release by this manifest.',
        'logs, captures, diagnostics, selected-suit.txt': 'Local test or user state.',
    },
    'games': [], 'files': [],
}
for folder, exe in [('Spider-Man', 'SpiderMan.exe'), ('Spider-Man 2', 'SpiderMan2.exe')]:
    game = stage / folder
    if not (game / exe).is_file() or not (game / 'mods/suits').is_dir() or not (game / 'licenses').is_dir():
        raise SystemExit(f'Incomplete stage: {game}')
    suits = []
    for directory in sorted((game / 'mods/suits').iterdir()):
        if not directory.is_dir():
            continue
        # Existing suit manifests include comments and trailing commas.
        metadata = json5.loads((directory / 'suit.json').read_text(encoding='utf-8-sig'))
        suits.append({'directory': directory.name, 'id': metadata['id'], 'name': metadata['name']})
    paths = [game / exe]
    for subtree in ['mods/suits', 'licenses']:
        paths.extend(p for p in (game / subtree).rglob('*') if p.is_file()
                     and p.name != 'selected-suit.txt')
    rows = []
    for path in sorted(paths):
        if path.is_symlink() or not path.resolve().is_relative_to(game.resolve()):
            raise SystemExit(f'Payload escapes stage: {path}')
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        rows.append({'path': path.relative_to(stage).as_posix(),
                     'bytes': path.stat().st_size, 'sha256': digest})
    manifest['files'].extend(rows)
    manifest['games'].append({'folder': folder, 'executable': exe,
                             'external_suit_count': len(suits), 'suits': suits,
                             'file_count': len(rows), 'bytes': sum(r['bytes'] for r in rows)})
output.mkdir(parents=True)
(output / 'release-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
(output / 'SHA256SUMS.txt').write_text(''.join(f"{r['sha256']}  {r['path']}\n" for r in manifest['files']), encoding='utf-8')
lines = ['# Release Manifest', '', 'Draft Windows x64 inventory of the current staged builds.',
         'No release archive was created or uploaded. Version is not assigned.', '',
         '## Package Layout', '', '```text', 'OpenSpidey-release/',
         '  release-manifest.json', '  SHA256SUMS.txt']
for game in manifest['games']:
    lines += [f"  {game['folder']}/", f"    {game['executable']}", '    licenses/',
              '    mods/', f"      suits/  ({game['external_suit_count']} external suits)",
              '        smu-eligibility.json', '        <suit-id>/', '          suit.json',
              '          ...model, textures and attribution files as supplied...']
lines += ['```', '', '## Inventory', '']
for game in manifest['games']:
    lines.append(f"- {game['folder']}: {game['file_count']} files, {game['bytes']:,} bytes, {game['external_suit_count']} external suits. Base-game costumes are not included in this count.")
lines += ['', '## Excluded From Package', '']
lines.extend(f'- `{path}`: {reason}' for path, reason in manifest['excluded'].items())
lines += ['', '## Release Gates', '', '- Assign a version and release notes.',
          '- Review redistribution permissions and attribution for bundled and external mod content.',
          '- Package the inventoried files and verify their SHA256 values.',
          '- Validate first launch from a clean extraction; this manifest does not certify that test.',
          '', 'The executables embed runtime dependencies; no loose runtime DLLs are listed.',
          'The user supplies the appropriate retail disc on first launch.', '']
(output / 'RELEASE-MANIFEST.md').write_text('\n'.join(lines), encoding='utf-8')
print(json.dumps({'output': str(output), 'games': [{k: g[k] for k in
      ('folder', 'external_suit_count', 'file_count', 'bytes')} for g in manifest['games']]}, indent=2))
