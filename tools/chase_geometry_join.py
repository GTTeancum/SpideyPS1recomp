"""Join the existing Chase palette-230 trace to stock geometry without new images.

This fixture uses full-frame draw origins (0, 0/256). GPU submission can lag the
source trace by console ticks. Matches retain ambiguity; matching projected
positions/depths does not prove the entire scene or camera transform correct.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('run', type=Path)
parser.add_argument('--asset', type=Path, default=Path('spiderman/extracted/wad/l5a1_g.psx'))
args = parser.parse_args()
sources = [json.loads(line) for line in (args.run/'geometry.jsonl.sources').read_text().splitlines()]
# The selected material uses native 32-byte textured quad records, whose CLUT
# has already been resolved into word 4 by the loader.
sources = [s for s in sources if 'vertices' in s and len(s['words']) == 8 and s['words'][4] >> 16 == 230]
triangles = [json.loads(line) for line in (args.run/'geometry.jsonl').read_text().splitlines()]
triangles = [t for t in triangles if t['Clut'] == 230]

def coord(word, shift):
    value = (word >> shift) & 2047
    return value - 2048 if value & 1024 else value

index = defaultdict(set)
for i, source in enumerate(sources):
    for v in source['vertices']:
        index[(source['frame'], coord(v['packed'], 0), coord(v['packed'], 16))].add(i)

joined = []
selected_sources = {}
for t in triangles:
    if t['ClipX0'] != 0 or t['ClipY0'] not in (0, 256):
        raise ValueError('This join requires the verified full-frame Chase draw origins')
    matches = []
    for lag in range(-2, 3):
        keys = [(t['frame']-lag, v['NativeX'], v['NativeY']-t['ClipY0']) for v in t['vertices']]
        for i in sorted(set.intersection(*(index[key] for key in keys))):
            s = sources[i]
            deltas = []
            for v, key in zip(t['vertices'], keys):
                depths = [w['Depth'] for w in s['vertices']
                          if (coord(w['packed'], 0), coord(w['packed'], 16)) == key[1:]]
                deltas.append(min(abs(z-v['Z']) for z in depths))
            if max(deltas) >= .02:
                continue
            matches.append({'source_frame': s['frame'], 'lag': lag,
                            'pointer': s['pointer'], 'caller': s['caller'], 'max_depth_delta': max(deltas)})
            selected_sources[s['pointer']] = s
    span = max(max(v[k] for v in t['vertices'])-min(v[k] for v in t['vertices']) for k in ('X', 'Y'))
    joined.append({'frame': t['frame'], 'span': span, 'vertices': t['vertices'], 'matches': matches})

data = args.asset.read_bytes()
def u32(offset):
    return struct.unpack_from('<I', data, offset)[0]

# Loader changes UV/CLUT/lighting fields, so determine a common load base from
# primitive length and all four vertex indices, then verify mesh vertices below.
votes = defaultdict(set)
for pointer, s in selected_sources.items():
    needle = struct.pack('<I', s['words'][1])
    offset = data.find(needle)
    while offset != -1:
        if offset >= 4 and offset % 4 == 0 and u32(offset-4) >> 16 == 32:
            votes[pointer-(offset-4)].add(pointer)
        offset = data.find(needle, offset+1)
ranked = sorted(votes, key=lambda base: len(votes[base]), reverse=True)
if not ranked or len(votes[ranked[0]]) != len(selected_sources) or (len(ranked)>1 and len(votes[ranked[1]])==len(selected_sources)):
    raise ValueError('No unique asset load base covering every matched primitive')
base = ranked[0]
selected_offsets = {pointer-base for pointer in selected_sources}
objects = u32(8)
table = 12+objects*36
mesh_count = u32(table)
ram_paths = list(args.run.glob('ram_*.bin'))
if len(ram_paths) != 1:
    raise ValueError('Expected exactly one matching native RAM snapshot')
ram = ram_paths[0].read_bytes()
meshes = []
for mesh in range(mesh_count):
    offset = u32(table+4+mesh*4)
    vertices, normals, faces = struct.unpack_from('<HHH', data, offset+2)
    cursor = offset+28+8*(vertices+normals)
    hits = []
    for face in range(faces):
        if cursor in selected_offsets:
            hits.append(face)
        size = struct.unpack_from('<H', data, cursor+2)[0]
        if size < 8 or cursor+size > len(data):
            raise ValueError('Invalid source face boundary')
        cursor += size
    if hits:
        vertex_bytes = data[offset+28:offset+28+vertices*8]
        live_offset = base-0x80000000+offset+28
        live = ram[live_offset:live_offset+len(vertex_bytes)]
        meshes.append({'mesh': mesh, 'offset': offset, 'vertices': vertices, 'faces': faces,
                       'matched_faces': hits, 'vertex_bytes_unchanged': vertex_bytes == live})
summary = {'triangles': len(joined), 'matched_triangles': sum(bool(t['matches']) for t in joined),
           'lags': dict(Counter(m['lag'] for t in joined for m in t['matches'])),
           'callers': dict(Counter(hex(m['caller']) for t in joined for m in t['matches'])),
           'asset': str(args.asset), 'asset_sha256': hashlib.sha256(data).hexdigest(),
           'asset_load_base': hex(base), 'source_primitives': len(selected_sources), 'meshes': meshes}
report = {'summary': summary, 'triangles': joined,
          'limits': 'Only matched palette-230 quads in this bounded fixture. Ambiguous coincident faces retained. '
                    'Does not establish camera correctness, unmatched geometry, texture sampling, ordering or ordinary Chase completion.'}
(args.run/'geometry-source-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(summary, indent=2))
