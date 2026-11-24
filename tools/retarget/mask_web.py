"""Register the two sides of the three reviewed webbed masks against their painted center."""
from collections import defaultdict
import struct

import numpy as np
from PIL import Image

from native_asset import U


# Atlas coordinates are on the 128-texel native grid. The seed is on the nose.
# The outer edge starts at the forehead, then runs over the crown to the nape.
# The final short segment of the forehead web ends at an existing transverse web.
MASKS = {
    'smu-earth62412': ((77, 60), (78, 49), (75.26, 47.17)),
    'smu-spiderman-newdesign': ((97, 17), (98, 7), (92.43, 3.81)),
    'smu-uk': ((97, 17), (98, 7), (92.43, 3.81)),
}


def left_edge(pairs):
    return [(max(old[0]+1, inset[0]), inset[1]) for old, _, inset, _ in pairs]


def align_face(actor, suit, rig, pairs):
    seed, (center_u, _), _ = MASKS[suit]
    shift_v = int(suit == 'smu-earth62412')
    aligned_seed = (center_u, seed[1] + shift_v)
    positions, _ = rig.evaluate()
    table = 12 + 36 * U(actor, 8)
    corners = {}
    adjacent = defaultdict(list)
    start = None
    for i, ids in enumerate(rig.packets()):
        mesh = U(actor, table + 4 + i*4)
        nv, nn, nf = struct.unpack_from('<HHH', actor, mesh+2)
        face = mesh + 28 + 8*(nv+nn)
        for _ in range(nf):
            flags, length = struct.unpack_from('<HH', actor, face)
            if flags != 0x1f or length != 36:
                raise ValueError('Expected a native retargeted triangle packet')
            if U(actor, face+16) == 0:
                points = positions[ids[list(actor[face+4:face+7])], :3]
                if points[:,1].max() < -1100:
                    keys = [(tuple(pos), tuple(actor[face+20+2*k:face+22+2*k]))
                            for k, pos in enumerate(points)]
                    corners[face] = keys
                    for pos, uv in keys:
                        adjacent[pos, uv].append(face)
                        if abs(pos[0]) < 4 and pos[2] < -150 and uv in (seed, aligned_seed):
                            start = face
            face += length
    if start is None:
        raise ValueError(f'Missing {suit} mask UV island')
    # The collar overlaps this rectangle in the Earth-62412 atlas. Traverse the
    # connected mask in both model and UV space so the collar keeps its own UVs.
    mask = {start}
    todo = [start]
    while todo:
        for key in corners[todo.pop()]:
            for neighbor in adjacent[key]:
                if neighbor not in mask:
                    mask.add(neighbor)
                    todo.append(neighbor)

    halves = [{}, {}]
    sides = {}
    for face in mask:
        keys = corners[face]
        side = int(sum(pos[0] for pos, _ in keys)/3 > -4)
        sides[face] = side
        halves[side].update(keys)
    left, right = halves
    a, b = np.array(list(left)), np.array(list(right))
    mid = np.median(a[abs(a[:,0]) < 4, 0])
    reflected = b.copy()
    reflected[:,0] = 2*mid-b[:,0]
    distances = np.linalg.norm(reflected[:,None]-a, axis=2)
    match = distances.argmin(axis=1)
    if len(a) != len(b) or len(set(match)) != len(a) or distances.min(axis=1).max() > 1:
        raise ValueError(f'Mask geometry is no longer bilaterally paired for {suit}')
    if any(uv == aligned_seed and abs(pos[0]) < 4 and pos[2] < -150
           for pos, uv in left.items()):
        for pos, idx in zip(b, match):
            u, v = left[tuple(a[idx])]
            if right[tuple(pos)] != (2*center_u-u, v):
                raise ValueError(f'Partially registered mask UVs for {suit}')
        return 0
    # Accept both the first converted assets and the earlier rear-edge UV inset.
    edge = {uv: dest for (old, _, prev, _), dest in zip(pairs, left_edge(pairs))
            for uv in (old, prev)}
    for pos, (u, v) in left.items():
        dest = (u+1, v+shift_v)
        if abs(pos[0]-mid) < 2:
            dest = edge.get((u, v), dest)
        left[pos] = dest
    for pos, idx in zip(b, match):
        u, v = left[tuple(a[idx])]
        right[tuple(pos)] = (2*center_u-u, v)
    changed = 0
    for face in mask:
        for k, (pos, uv) in enumerate(corners[face]):
            dest = halves[sides[face]][pos]
            if uv != dest:
                actor[face+20+2*k:face+22+2*k] = bytes(dest)
                changed += 1
    return changed


def distance_to_path(xy, points):
    distance = np.full(xy.shape[:2], np.inf)
    for a, b in zip(points, points[1:]):
        a = np.asarray(a)
        delta = np.asarray(b)-a
        t = np.clip(((xy-a)*delta).sum(axis=2)/(delta@delta), 0, 1)
        distance = np.minimum(distance, np.linalg.norm(xy-a-t[:,:,None]*delta, axis=2))
    return distance


def register_art(image, suit, pairs):
    """Keep the original eyes and match colors across the narrow crown UV border."""
    _, tip, crossing = MASKS[suit]
    earth = suit == 'smu-earth62412'
    edge = [tip, *left_edge(pairs)]
    if image.size != (1024, 1024):
        raise ValueError(f'Unexpected diffuse atlas size for {suit}: {image.size}')
    scale = image.width/128
    cx = int(tip[0]*scale)
    source = np.array(image)
    # Shift the painted facial center by the remaining fraction of a native UV
    # cell. Fade that shift before reaching either eye or the edge of the chart.
    x = np.arange(cx-48, cx+48)
    y0, y1 = (376,544) if earth else (48,256)
    offset = np.clip((48-np.abs(x+.5-cx))/24,0,1) * (.5 if earth else 1.5)
    sample = x-offset
    x0 = np.floor(sample).astype(int)
    frac = (sample-x0)[None,:,None]
    source[y0:y1,x] = np.rint((1-frac)*source[y0:y1,x0]+frac*source[y0:y1,x0+1])
    output = source.copy()

    xmin = max(int((min(u for u,v in edge)-3)*scale), 2*cx-image.width)
    ymin = max(0,int((min(v for u,v in edge)-3)*scale))
    ymax = int((max(v for u,v in edge)+3)*scale)+1
    if not earth:
        ymax = min(ymax, 260)  # The next clothing island begins below the mask.
    yy, xx = np.mgrid[ymin:ymax,xmin:cx]
    xy = np.stack(((xx+.5)/scale,(yy+.5)/scale),axis=2)
    # Along the two UV paths the samples are identical, even under filtering.
    blend = np.clip((2.2-distance_to_path(xy,edge))/1.25,0,1)[:,:,None]
    start = (tip[0], tip[1]+(.2 if earth else .3))
    line = [start, tip, crossing] if earth else [start,tip,edge[1],crossing]
    radius = .19 if earth else .15
    coverage = np.clip((radius+.5/scale-distance_to_path(xy,line))*scale,0,1)[:,:,None]
    ink = (55,31,29) if earth else (18,7,8)
    opposite = 2*cx-1-xx
    left = source[yy,xx].astype(float)
    right = source[yy,opposite]*(1-blend)+left*blend
    for pixels, x in ((left,xx),(right,opposite)):
        rgb = pixels[:,:,:3]
        output[yy,x,:3] = np.rint(rgb*(1-coverage)+np.minimum(rgb,ink)*coverage)
    return Image.fromarray(output)
