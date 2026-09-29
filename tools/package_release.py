"""Package and verify the exact files in a reviewed release manifest; never upload."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('manifest', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--version', required=True)
args = parser.parse_args()
manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
stage = Path(manifest['source_stage']).resolve(strict=True)
if args.output.exists():
    raise SystemExit('Output already exists; refusing to overwrite release artifacts')
args.output.mkdir(parents=True)
for game in manifest['games']:
    folder = game['folder']
    sm1 = folder == 'Spider-Man'
    short = 'sm1' if sm1 else 'sm2'
    title = 'Spider-Man' if sm1 else 'Spider-Man 2: Enter Electro'
    stem = f"{'Spider-Man' if sm1 else 'Spider-Man-2'}-{args.version}-Windows-x64"
    archive = args.output / (stem + '.zip')
    rows = []
    payload = {}
    for row in manifest['files']:
        if not row['path'].startswith(folder + '/'):
            continue
        path = (stage / row['path']).resolve(strict=True)
        if not path.is_relative_to(stage / folder):
            raise ValueError(f'Unsafe manifest path: {path}')
        relative = path.relative_to(stage / folder).as_posix()
        if relative != game['executable'] and not relative.startswith(('licenses/', 'mods/suits/')):
            raise ValueError(f'Unapproved release payload: {relative}')
        data = path.read_bytes()
        if len(data) != row['bytes'] or hashlib.sha256(data).hexdigest() != row['sha256']:
            raise ValueError(f'Stage changed since manifest: {path}')
        payload[relative] = data
        rows.append({**row, 'path': relative})
    changes = [f"{game['external_suit_count']} external suits, with per-suit attribution and metadata.",
               'Expanded costume selection and preserved-rig suit support.',
               'Cumulative rendering, animation, web alignment, timing and audio repairs.',
               'Retry and Pause rendering repairs.',
               'Self-contained Windows x64 executable; no separate runtime installation.']
    if sm1:
        changes.append('Repaired Venom disappearance and chase-meter splitting during HUD transitions.')
    notes = f"# {title} {args.version}\n\n## Included\n\n" + ''.join(f'- {s}\n' for s in changes)
    notes += f"""
## Installation

1. Extract the ZIP to a writable folder. Do not launch from inside the ZIP.
2. Keep `mods` and `licenses` beside `{game['executable']}`.
3. Launch `{game['executable']}` and select your own USA PlayStation CUE/BIN or ISO for this game.
4. Let first-run setup install the disc data. Raw CUE/BIN dumps preserve XA audio/video.
5. Select costumes from SPECIAL > {'COSTUME VIEWER' if sm1 else 'COSTUMES'}.

Back up existing saves and settings before upgrading. For a clean upgrade, extract
to a new folder; merging over 1.0 can leave obsolete suit directories behind.
Retail disc images, extracted retail data, saves and local settings are not included.
Runtime dependencies and built-in replacement assets are embedded in the executable.
Author credits remain in the suit metadata and accompanying files.

## Verification and Limits

Archive contents are SHA256-verified against the approved staged payload.
Targeted native gameplay and rendering regressions passed; complete playthroughs
and every hardware configuration are not certified. Packaging does not constitute
a new clean-install gameplay test. See `release-manifest.json` for the file inventory.
"""
    if sm1:
        notes += '\nKnown visual limitation: the separate left-side web HUD can still separate briefly during HUD transitions. The Chase Venom meter fix does not address that artifact.\n'
    payload['README.txt'] = notes.encode('utf-8')
    rows.append({'path': 'README.txt', 'bytes': len(payload['README.txt']),
                 'sha256': hashlib.sha256(payload['README.txt']).hexdigest()})
    public = {'schema_version': 1, 'version': args.version, 'game': title,
              'platform': 'Windows x64', 'external_suit_count': game['external_suit_count'],
              'suits': game['suits'], 'files': rows,
              'note': 'Hashes identify the shipped payload. Manifest and checksum list exclude themselves.'}
    payload['release-manifest.json'] = (json.dumps(public, indent=2) + '\n').encode('utf-8')
    payload['SHA256SUMS.txt'] = ''.join(f"{r['sha256']}  {r['path']}\n" for r in rows).encode('utf-8')
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zipped:
        for name, data in sorted(payload.items()):
            zipped.writestr(f'{folder}/{name}', data)
    with zipfile.ZipFile(archive) as zipped:
        if zipped.testzip() is not None or len(zipped.namelist()) != len(payload):
            raise ValueError(f'Archive integrity failure: {archive}')
        for name, data in payload.items():
            if hashlib.sha256(zipped.read(f'{folder}/{name}')).digest() != hashlib.sha256(data).digest():
                raise ValueError(f'Archived content mismatch: {name}')
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    (args.output / f'{stem}.sha256').write_text(f'{digest}  {archive.name}\n', encoding='utf-8')
    (args.output / f'{short}-release-notes.md').write_text(notes, encoding='utf-8')
    print(json.dumps({'archive': str(archive), 'bytes': archive.stat().st_size,
                      'files': len(payload), 'sha256': digest, 'verified': True}), flush=True)
