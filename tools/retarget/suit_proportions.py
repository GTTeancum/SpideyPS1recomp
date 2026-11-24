"""Calibrate reviewed SMU actors against the original player's neutral pose."""
from __future__ import annotations

import json
from pathlib import Path
import struct

import numpy as np

from native_asset import U
from rig_blob import MAGIC, Rig
from scene import align

SUITS = Path(__file__).resolve().parents[2] / "spiderman/port/mods/suits"
# The reviewed twenty boots fall within two integer mesh units of stock.
FOOT_ROUNDING_TOLERANCE = 2.0


def uses_human_calibration(suit_id):
    eligible = json.loads((SUITS / "smu-eligibility.json").read_text())["eligibleIds"]
    return suit_id in eligible and "spiderham" not in suit_id and "porker" not in suit_id


def rig_offset(actor):
    offset = U(actor, 4)
    while U(actor, offset) != 0xffffffff:
        size = U(actor, offset + 4)
        if U(actor, offset) == MAGIC:
            return offset + 8, size
        offset += 8 + size
    raise ValueError("Actor has no RTG2 rig")


def matrices(blob, header, field):
    result = np.tile(np.eye(4), (header[3], 1, 1))
    result[:, :3] = np.ndarray((header[3], 3, 4), "<f4", buffer=blob,
                               offset=header[7] + field, strides=(196, 16, 4))
    return result


def correct_upper_body(blob, rig):
    names = rig.provenance["boneNames"]
    parents = rig.provenance["sourceParents"]
    pelvis, waist, lower_chest, head = [names.index("Clown001" + name)
                                       for name in ("Pelvis", "Spine1", "Spine2", "Head")]
    rest = matrices(blob, rig.h, 64)
    bind = np.linalg.inv(matrices(blob, rig.h, 16))
    native = rig.rest_driver()[:, :, 3]
    # Align the trunk as a unit. Independently aiming each short vertebra at
    # a different native segment bakes a bend into even the neutral torso.
    trunk = align(bind[head, :3, 3] - bind[pelvis, :3, 3], native[7] - native[0])
    # In donor rest space Spine1 is at the abdomen pivot (2), Spine2 at the
    # upper-torso pivot (1). Spine3 already uses that same chest rotation.
    struct.pack_into("<i", blob, rig.h[7] + waist * 196 + 4, 2)
    struct.pack_into("<i", blob, rig.h[7] + lower_chest * 196 + 4, 1)
    upper_body = set()
    for i, parent in enumerate(parents):
        if i != waist and parent not in upper_body:
            continue
        upper_body.add(i)
        offset = rig.h[7] + i * 196
        driver, = struct.unpack_from("<i", blob, offset + 4)
        if i == waist:
            rest[i, :3, 3] = rest[pelvis, :3, 3] + trunk @ (bind[i, :3, 3] - bind[pelvis, :3, 3])
        else:
            source_offset = np.linalg.solve(bind[parent, :3, :3], bind[i, :3, 3] - bind[parent, :3, 3])
            rest[i, :3, 3] = rest[parent, :3, 3] + rest[parent, :3, :3] @ source_offset
        if driver in (1, 2):
            rest[i, :3, :3] = trunk @ bind[i, :3, :3]
        elif i == head:
            rest[i, :3, :3] = bind[i, :3, :3]
        elif driver < 0:
            relative_basis = np.linalg.solve(bind[parent, :3, :3], bind[i, :3, :3])
            rest[i, :3, :3] = rest[parent, :3, :3] @ relative_basis
        # Independently driven arms retain their calibrated rotation. Fingertip
        # articulation is encoded separately in the original fist matrices.
        blob[offset+64:offset+112] = rest[i, :3].astype("<f4").tobytes()
        relative = np.linalg.solve(rest[parent], rest[i])
        blob[offset+112:offset+160] = relative[:3].astype("<f4").tobytes()


def correct_boot_reach(blob, rig, target=407.0):
    """Move only weighted forefoot vertices in bind space. Keep sole heights."""
    world, bones = rig.evaluate()
    source = np.ndarray((rig.count, 3), "<f4", buffer=blob, offset=rig.h[8], strides=(32, 4))
    spans = np.ndarray((rig.count, 2), "<u4", buffer=blob, offset=rig.h[8]+24, strides=(32, 4))
    records = np.frombuffer(blob, dtype=[("bone", "<u4"), ("weight", "<f4")],
                            count=rig.h[5], offset=rig.h[9])
    weights = np.zeros((rig.count, rig.bones))
    for vertex, (start, count) in enumerate(spans):
        entries = records[start:start+count]
        np.add.at(weights[vertex], entries["bone"], entries["weight"])
    inverse_bind = matrices(blob, rig.h, 16)
    rotations = bones[:, :, :3] @ inverse_bind[:, :3, :3]
    names = rig.provenance["boneNames"]
    for side, part in (("L", 17), ("R", 14)):
        prefix = "Clown001" + side + "Leg"
        ankle = names.index(prefix + "Ankle")
        foot = [i for i, name in enumerate(names) if i == ankle or name.startswith(prefix + "Digit")]
        influence = weights[:, foot].sum(axis=1)
        vertex_ids = np.flatnonzero(influence > 0)
        limit_z = rig.rest_driver()[part, 2, 3] - target
        points = world[vertex_ids, :3]
        if points[:, 2].min() >= limit_z - FOOT_ROUNDING_TOLERANCE:
            continue
        toe = prefix + "Digit01"
        if toe in names:
            tip = bones[names.index(toe), :, 3]
        else:
            # Aikman has ankle weights but no toe bone. Use its forward boot rim.
            tip = points[points[:, 2] <= points[:, 2].min() + 1].mean(axis=0)
        forward = (tip - bones[ankle, :, 3]).astype(float)
        forward[1] = 0
        forward /= np.linalg.norm(forward)
        reach = np.maximum(0., (points - bones[ankle, :, 3]) @ forward)
        move = -np.outer(reach * influence[vertex_ids], forward)
        long = points[:, 2] < limit_z
        if np.any(move[long, 2] <= 0):
            raise ValueError("Toe lies outside the weighted forefoot")
        fraction = np.max((limit_z - points[long, 2]) / move[long, 2])
        if fraction >= 1:
            raise ValueError("Correction would invert the foot")
        active = move[:, 2] > 0
        ids = vertex_ids[active]
        skin = np.einsum("vb,bij->vij", weights[ids], rotations)
        # Invert the actual weighted skin transform so the neutral sole doesn't
        # lift off the floor. Future frames still use the original skin weights.
        delta = np.linalg.solve(skin, (fraction * move[active])[..., None])[:, :, 0]
        source[ids] += delta


def calibrated_blob(original, suit_id):
    if not uses_human_calibration(suit_id):
        return original
    rig = Rig(original)
    result = bytearray(original)
    correct_upper_body(result, rig)
    correct_boot_reach(result, rig)
    return bytes(result)
