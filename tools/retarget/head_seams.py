"""Reviewed crown and rear UV coordinates for the 128-texel native suit atlases."""
import struct

from native_asset import U
from mask_web import MASKS, align_face, register_art


# Old left, old right, repaired left, repaired right, in the order the edges
# meet along the skull. Each atlas has its own island shape and mirror axis.
SEAM_PAIRS = {
    'smu-civilwarpeterparker': (
        ((45, 44), (46, 44), (45, 45), (46, 45)),
        ((42, 41), (48, 41), (42, 42), (48, 42)),
        ((40, 39), (51, 39), (40, 40), (51, 40)),
        ((36, 38), (55, 38), (36, 39), (55, 39)),
        ((32, 38), (59, 38), (32, 39), (59, 39)),
        ((29, 39), (61, 39), (30, 40), (61, 40)),
        ((26, 41), (64, 41), (27, 42), (63, 42)),
        ((24, 44), (66, 44), (25, 44), (65, 44)),
        ((23, 47), (68, 46), (24, 47), (66, 47)),
        ((22, 49), (69, 49), (23, 49), (67, 49)),
        ((21, 52), (69, 52), (22, 52), (68, 52)),
    ),
    'smu-earthx': (
        # Stop where the separate modeled fastening begins.
        ((94, 2), (106, 2), (95, 3), (108, 5)),
        ((89, 2), (111, 1), (91, 3), (111, 3)),
        ((84, 3), (116, 2), (85, 4), (117, 5)),
        ((80, 5), (121, 5), (82, 6), (119, 8)),
        ((78, 7), (122, 6), (79, 7), (124, 7)),
    ),
    'smu-earth62412': (
        ((74, 46), (81, 46), (75, 47), (81, 48)),
        ((70, 45), (85, 45), (70, 46), (86, 46)),
        ((66, 45), (89, 46), (68, 46), (88, 46)),
        ((63, 47), (92, 48), (64, 48), (92, 48)),
        ((61, 50), (94, 51), (62, 51), (94, 51)),
        ((59, 54), (95, 54), (61, 54), (95, 54)),
        ((59, 57), (96, 57), (60, 57), (96, 57)),
        ((58, 58), (97, 58), (60, 58), (96, 58)),
    ),
    'smu-futurefoundation': (
        ((92, 3), (102, 3), (92, 4), (102, 4)),
        ((86, 1), (108, 1), (86, 2), (108, 2)),
        ((80, 1), (114, 1), (80, 2), (114, 2)),
        ((75, 3), (119, 3), (76, 4), (118, 4)),
        ((71, 6), (123, 6), (72, 7), (122, 7)),
        ((69, 10), (126, 10), (70, 10), (124, 10)),
        *(((68, v), (126, v), (70, v), (124, v)) for v in (15, 19, 23, 26, 29, 32)),
    ),
}

# These masks use the same native head island. The 2020 atlas starts one
# texel lower at the crown and its rest pose is tilted to the player's left.
SEAM_PAIRS['smu-modernscarlet'] = SEAM_PAIRS['smu-futurefoundation']
SEAM_PAIRS['smu-spiderman-2020'] = (
    ((92, 4), (102, 4), (92, 5), (102, 5)),
    ((86, 2), (108, 2), (86, 3), (108, 3)),
    ((80, 2), (114, 2), (80, 3), (114, 3)),
    *SEAM_PAIRS['smu-futurefoundation'][3:],
)
SEAM_PAIRS['smu-spider-spiner'] = (
    ((96, 96), (101, 96), (96, 97), (101, 97)),
    ((94, 95), (103, 95), (94, 96), (103, 96)),
    ((91, 94), (106, 94), (91, 95), (106, 95)),
    ((88, 93), (108, 93), (89, 95), (108, 95)),
    ((84, 93), (112, 93), (85, 95), (112, 95)),
    ((81, 95), (115, 95), (82, 96), (115, 96)),
    ((78, 97), (118, 97), (80, 98), (117, 98)),
    ((76, 99), (120, 99), (78, 100), (119, 100)),
    ((75, 103), (121, 103), (77, 103), (120, 103)),
    ((75, 105), (121, 105), (77, 105), (120, 105)),
    ((75, 108), (122, 108), (76, 108), (121, 108)),
    ((74, 109), (122, 109), (76, 109), (121, 109)),
    ((73, 112), (124, 112), (74, 112), (123, 112)),
)
SEAM_PAIRS['smu-scarlet-spiderham-benreilly'] = (
    ((63, 43), (68, 43), (63, 44), (68, 44)),
    ((60, 41), (70, 41), (60, 42), (71, 42)),
    ((57, 39), (74, 39), (57, 41), (74, 41)),
    ((52, 39), (78, 39), (53, 40), (78, 40)),
    ((48, 40), (82, 40), (49, 41), (82, 41)),
    ((45, 41), (86, 41), (46, 43), (85, 43)),
    ((42, 44), (88, 44), (43, 45), (88, 45)),
    ((40, 47), (90, 47), (41, 48), (90, 48)),
    ((39, 48), (92, 48), (40, 49), (91, 49)),
    ((38, 48), (93, 49), (39, 50), (92, 50)),
)
SEAM_PAIRS['smu-spiderman-surveillancesuit'] = (
    ((102, 1), (111, 1), (102, 2), (113, 2)),
    ((97, 1), (116, 1), (98, 2), (116, 2)),
    ((93, 4), (121, 2), (94, 5), (121, 3)),
    ((90, 8), (124, 6), (92, 8), (123, 7)),
    ((89, 13), (125, 11), (92, 13), (124, 11)),
    ((88, 16), (126, 15), (90, 16), (126, 15)),
    ((88, 18), (127, 17), (90, 18), (125, 17)),
)

# The red masks share Future Foundation's UV layout. Their painted web lines
# cross it at different points, so their seam samples are paired separately.
MASK_WEB_COORDS = {
    'smu-spidermanhomecoming': (
        ((93, 4), (104, 5)), ((86, 2), (108, 2)), ((81, 2), (115, 2)),
        ((77, 4), (118, 4)), ((73, 7), (123, 7)), ((70, 10), (125, 10)),
        ((70, 15), (126, 15)), ((70, 19), (125, 19)), ((70, 23), (124, 23)),
        ((70, 26), (125, 26)), ((71, 29), (124, 29)), ((71, 32), (124, 32)),
    ),
    'smu-spiderman-newdesign': (
        ((92, 4), (104, 6)), ((86, 2), (109, 2)), ((80, 2), (116, 2)),
        ((76, 4), (118, 4)), ((73, 7), (122, 7)), ((71, 10), (125, 10)),
        ((71, 15), (124, 15)), ((70, 19), (125, 19)), ((70, 23), (124, 23)),
        ((70, 26), (124, 26)), ((71, 29), (124, 29)), ((71, 32), (124, 32)),
    ),
    'smu-uk': (
        ((93, 4), (103, 5)), ((86, 2), (110, 2)), ((80, 2), (115, 2)),
        ((77, 4), (118, 6)), ((73, 7), (122, 8)), ((70, 10), (124, 11)),
        ((70, 15), (124, 15)), ((71, 19), (124, 19)), ((71, 23), (125, 23)),
        ((70, 26), (124, 26)), ((71, 29), (124, 29)), ((70, 32), (125, 32)),
    ),
}
for suit, destinations in MASK_WEB_COORDS.items():
    SEAM_PAIRS[suit] = tuple((left, right, a, b) for (left, right, _, _), (a, b)
                             in zip(SEAM_PAIRS['smu-futurefoundation'], destinations, strict=True))

SEAM_UVS = {
    suit: {old: bytes(new) for a, b, c, d in pairs
           for old, new in ((a, c), (b, d)) if old != new}
    for suit, pairs in SEAM_PAIRS.items()
}
# The shared tip of the forehead cut falls on the gutter on these two atlases.
SEAM_UVS['smu-spider-spiner'][(98, 98)] = bytes((98, 99))
SEAM_UVS['smu-scarlet-spiderham-benreilly'][(65, 46)] = bytes((65, 48))


def repair_head_uvs(mesh, positions, suit):
    """Update the native face corners on the centerline in the rig's rest pose."""
    mapping = SEAM_UVS.get(suit)
    if mapping is None:
        return 0
    nv, nn, nf = struct.unpack_from('<HHH', mesh, 2)
    if len(positions) != nv:
        raise ValueError('Packet vertex count differs from the preserved rig')
    # The short Ben Reilly Spider-Ham has its skull near the rig origin.
    half_width, max_y = {
        'smu-scarlet-spiderham-benreilly': (6, 280),
        'smu-spiderman-2020': (12, -1300),
    }.get(suit, (4, -1200))
    on_skull = (abs(positions[:, 0]) < half_width) & (positions[:, 1] < max_y)
    if not on_skull.any():
        return 0
    face = 28 + 8 * (nv + nn)
    changed = 0
    for _ in range(nf):
        flags, size = struct.unpack_from('<HH', mesh, face)
        if flags != 0x1f or size != 36:
            raise ValueError('Expected a native retargeted triangle packet')
        for corner in range(3):
            if on_skull[mesh[face + 4 + corner]]:
                offset = face + 20 + 2 * corner
                replacement = mapping.get(tuple(mesh[offset:offset + 2]))
                if replacement is not None:
                    mesh[offset:offset + 2] = replacement
                    changed += 1
        face += size
    return changed


def repair_actor_head_uvs(actor, suit, rig):
    if suit in MASKS:
        return align_face(actor, suit, rig, SEAM_PAIRS[suit])
    if suit not in SEAM_UVS:
        return 0
    table = 12 + 36 * U(actor, 8)
    packets = rig.packets()
    if U(actor, table) != len(packets):
        raise ValueError('Mesh table differs from the preserved rig')
    positions, _ = rig.evaluate()
    view = memoryview(actor)
    changed = 0
    for index, ids in enumerate(packets):
        offset = U(actor, table + 4 + 4 * index)
        changed += repair_head_uvs(view[offset:], positions[ids], suit)
    return changed


def reviewed_diffuse(image, suit):
    return register_art(image, suit, SEAM_PAIRS[suit]) if suit in MASKS else image


def repair_description(suit):
    return 'align both mask halves and join forehead web' if suit in MASKS else 'inset native skull centerline UVs'
