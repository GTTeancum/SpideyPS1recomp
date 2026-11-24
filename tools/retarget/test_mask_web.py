"""Check mask registration on the packaged native faces and the filtered HD texels."""
from collections import defaultdict
from pathlib import Path

import numpy as np
from PIL import Image

from native_asset import encode_palette8
from proof_render import load_asset


def sample(image, uv):
    xy = uv * 8 - .5  # GL samples at texel centers on the 1024px replacement.
    base = np.floor(xy).astype(int)
    frac = xy - base
    return sum(image[base[:, 1]+y, base[:, 0]+x] *
               ((frac[:, 0] if x else 1-frac[:, 0]) *
                (frac[:, 1] if y else 1-frac[:, 1]))[:, None]
               for x in (0, 1) for y in (0, 1))


def check(suits):
    cases = {
        'smu-earth62412': (78, (78, 49), (75.26, 47.17), 8),
        'smu-spiderman-newdesign': (98, (98, 7), (93, 4), 12),
        'smu-uk': (98, (98, 7), (93, 4), 12),
    }
    for suit, (axis, tip, next_point, minimum_edges) in cases.items():
        parsed, rig = load_asset(suits/suit/'actor.psx')
        with Image.open(suits/suit/'textures/diffuse.png') as diffuse:
            image = np.asarray(diffuse.convert('RGB'), dtype=float)
            palette, pixels = encode_palette8(diffuse, False)
        palette = palette[pixels]
        rgb5 = np.stack([palette >> shift & 31 for shift in (0, 5, 10)], axis=2)
        assert np.array_equal(parsed['textures'][0][:,:,:3], (rgb5 << 3) | (rgb5 >> 2)), suit

        world, _ = rig.evaluate()
        edges = defaultdict(set)
        centered = []
        for mesh, ids in zip(parsed['meshes'], rig.packets()):
            for face in mesh['faces']:
                if face['slot'] != 0:
                    continue
                points = world[ids[face['indices']], :3]
                for i, j in ((0, 1), (1, 2), (2, 0)):
                    a, b = points[[i, j]]
                    if max(abs(a[0]), abs(b[0])) >= 4 or max(a[1], b[1]) >= (-1360 if axis == 78 else -1200):
                        continue
                    uv = [tuple(face['uv'][k]) for k in (i, j)]
                    if max(a[2], b[2]) < -165 and max(abs(u-axis) for u,v in uv) <= 2:
                        centered.extend(u for u,v in uv)
                    if tuple(a) > tuple(b):
                        a, b = b, a
                        uv.reverse()
                    edges[tuple(a), tuple(b)].add(tuple(uv))
        assert centered and set(centered) == {axis}, (suit, 'forehead and nose', centered)
        checked = 0
        for halves in edges.values():
            if len(halves) != 2:
                continue
            left, right = sorted(halves)
            a = np.array(left)
            b = np.array(right)
            if not np.all(a[:,0] + b[:,0] == 2*axis) or not np.array_equal(a[:,1], b[:,1]):
                continue
            colors = [sample(image, np.linspace(row[0], row[1], 65)) for row in (a, b)]
            assert np.max(np.abs(colors[0]-colors[1])) <= 1.1, (suit, left, right)
            checked += 1
        assert checked >= minimum_edges, (suit, 'paired crown edges', checked)
        # The previous assets broke the dark radial web in this front-facing span.
        span = np.linspace(tip, next_point, 65)[5:-5]
        web = sample(image, span)[:, 0]
        panel = sample(image, span + [0, 1])[:, 0]
        assert np.max(web) < 105 and np.median(panel-web) > 95, (suit, 'forehead web')
        print(f'{suit}: facial axis {axis*8}px, {checked} continuous UV edges, forehead web and fallback verified')


if __name__ == '__main__':
    check(Path(__file__).resolve().parents[2]/'spiderman/port/mods/suits')
