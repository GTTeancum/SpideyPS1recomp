"""Compare the shipped suits to their pre-repair actors and the native player's feet and back."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from animation_bank import read
from head_seams import MASKS, repair_actor_head_uvs
from PIL import Image
from native_asset import parse, replace_diffuse_fallback
from suit_proportions import Rig, SUITS, calibrated_blob, matrices, rig_offset, uses_human_calibration


def reaches(rig, world):
    result = []
    names = rig.provenance["boneNames"]
    records = np.frombuffer(rig.blob, dtype=[("bone", "<u4"), ("weight", "<f4")],
                            count=rig.h[5], offset=rig.h[9])
    spans = np.ndarray((rig.count, 2), "<u4", buffer=rig.blob, offset=rig.h[8]+24, strides=(32, 4))
    for side, part in (("L", 17), ("R", 14)):
        owned = np.array([name == f"Clown001{side}LegAnkle" or name.startswith(f"Clown001{side}LegDigit")
                          for name in names])
        records_in_foot = owned[records["bone"]] & (records["weight"] > 0)
        foot = np.logical_or.reduceat(records_in_foot, spans[:, 0])
        result.append(float(rig.rest_driver()[part, 2, 3] - world[foot, 2].min()))
    return np.array(result)


def measure(path, originals, donor_reach):
    suit = path.parent.name
    old = (originals / suit / "actor.psx").read_bytes()
    new = path.read_bytes()
    offset, size = rig_offset(old)
    original = Rig(old[offset:offset+size])
    rig = Rig(new[offset:offset+size])
    calibrated = calibrated_blob(original.blob, suit)
    expected = bytearray(old[:offset] + calibrated + old[offset+size:])
    count = repair_actor_head_uvs(expected, suit, Rig(calibrated))
    if suit in MASKS:
        with Image.open(path.parent/'textures/diffuse.png') as image:
            replace_diffuse_fallback(expected, image)
    assert new == expected, suit
    if count:
        report = json.loads(path.with_name("conversion-report.json").read_text())
        assert report["headUvRepair"]["corners"] == count, suit
    digest = hashlib.sha256(new).hexdigest()
    for stage in ("Spider-Man", "Spider-Man 2"):
        assert (SUITS.parents[3] / stage / "mods/suits" / suit / path.name).read_bytes() == new, (stage, suit)
    if not uses_human_calibration(suit):
        assert rig.blob == original.blob, suit
        return {"suit": suit, "preserved": True, "sha256": digest}
    assert json.loads(path.with_name("conversion-report.json").read_text())["outputSha256"] == digest, suit
    before, old_bones = original.evaluate()
    world, bones = rig.evaluate()
    left_right_before = reaches(original, before)
    left_right = reaches(rig, world)
    assert np.max(np.abs(left_right - donor_reach)) <= 2, (suit, left_right)
    # The foot work must not lift either sole. The body work must preserve all link lengths.
    assert abs(world[:, 1].max() - before[:, 1].max()) < .005, suit
    names = rig.provenance["boneNames"]
    pelvis, waist, lower_chest, chest, head = [
        names.index("Clown001" + name) for name in ("Pelvis", "Spine1", "Spine2", "Spine3", "Head")
    ]
    drivers = np.ndarray((rig.bones,), "<i4", rig.blob, rig.h[7] + 4, strides=(196,))
    assert drivers[[pelvis, waist, lower_chest, chest, head]].tolist() == [0, 2, 1, 1, 7], suit
    native = rig.rest_driver()[:, :, 3]
    target = native[7] - native[0]
    actual = bones[head, :, 3] - bones[pelvis, :, 3]
    assert np.allclose(actual / np.linalg.norm(actual), target / np.linalg.norm(target), atol=2e-5), suit
    inv = matrices(rig.blob, rig.h, 16)
    assert np.allclose(bones[head, :, :3] @ inv[head, :3, :3], np.eye(3), atol=2e-5), suit
    for i, parent in enumerate(rig.provenance["sourceParents"]):
        if parent < 0:
            continue
        old_length = np.linalg.norm(old_bones[i, :, 3] - old_bones[parent, :, 3])
        new_length = np.linalg.norm(bones[i, :, 3] - bones[parent, :, 3])
        assert abs(old_length - new_length) < .005, (suit, names[i])
    # The stored influences, native pose bank, materials and rig provenance remain byte-exact.
    assert rig.blob[rig.h[9]:] == original.blob[original.h[9]:], suit
    if np.max(left_right_before - donor_reach) <= 2:
        assert np.array_equal(original.source_vertices(), rig.source_vertices()), suit
    assert calibrated_blob(rig.blob, suit) == rig.blob, suit
    return {"suit": suit, "toeBefore": left_right_before.tolist(), "toeAfter": left_right.tolist(),
            "sha256": digest}


def native_torso(actor, driver):
    triangles = []
    for mesh in actor["meshes"][:3]:  # Pelvis, chest and abdomen; seam vertices have individual owners.
        owners = np.asarray(mesh["owners"])
        positions = mesh["positions"] - actor["origins"][owners]
        posed = np.einsum("vij,vj->vi", driver[owners, :, :3], positions) + driver[owners, :, 3]
        for face in mesh["faces"]:
            ids = face["indices"]
            triangles.append(posed[[ids[0], ids[2], ids[1]]])
            if len(ids) == 4:
                triangles.append(posed[[ids[1], ids[2], ids[3]]])
    return np.asarray(triangles)


def suit_torso_faces(rig):
    # Keep skin influenced by the trunk. An arm crossing the line of sight
    # should not become the measured edge of the back.
    torso = {"Pelvis", "Spine1", "Spine2", "Spine3", "Ribcage", "Neck", "RArmCollarbone", "LArmCollarbone"}
    members = np.array([name.removeprefix("Clown001") in torso for name in rig.provenance["boneNames"]])
    influences = np.frombuffer(rig.blob, dtype=[("bone", "<u4"), ("weight", "<f4")],
                               count=rig.h[5], offset=rig.h[9])
    spans = np.ndarray((rig.count, 2), "<u4", rig.blob, rig.h[8]+24, strides=(32, 4))
    weight = np.add.reduceat(influences["weight"] * members[influences["bone"]], spans[:, 0])
    faces = rig.faces()
    return faces[weight[faces].min(axis=1) > .21]


def back_profile(triangles, head_y, pelvis_y):
    """Rear outline in side view, measured between the shoulders and belt."""
    # Omit the ends where the neck and hips obscure the torso silhouette.
    heights = np.linspace(head_y, pelvis_y, 17)[3:15]
    start = triangles.reshape(-1, 3)
    end = np.roll(triangles, -1, axis=1).reshape(-1, 3)
    rear = []
    for y in heights:
        crosses = (start[:, 1] > y) != (end[:, 1] > y)
        a, b = start[crosses], end[crosses]
        z = a[:, 2] + (y - a[:, 1]) * (b[:, 2] - a[:, 2]) / (b[:, 1] - a[:, 1])
        rear.append(z.max())
    return (np.asarray(rear) - rear[-1]) / (heights[-1] - heights[0])


def check_bank_back(path, stock, clips):
    data = path.read_bytes()
    offset, size = rig_offset(data)
    rig = Rig(data[offset:offset+size])
    faces = suit_torso_faces(rig)
    head, hip = [rig.provenance["boneNames"].index("Clown001" + name) for name in ("Head", "Pelvis")]
    results = {}
    # Clip/frame pairs come from SM1 bank RAM captures. The fast turn on 293
    # has a shorter visible trunk and wider variation between costume meshes.
    for clip, frame, tolerance in ((0, 4, .10), (19, 21, .10), (228, 34, .10), (293, 52, .16)):
        driver = clips[clip][frame]
        vertices, bones = rig.evaluate(driver)
        actual = back_profile(vertices[faces, :3], bones[head, 1, 3], bones[hip, 1, 3])
        target = back_profile(native_torso(stock, driver), driver[7, 1, 3], driver[0, 1, 3])
        error = float(np.sqrt(np.mean((target - actual) ** 2)))
        assert error < tolerance, (path.parent.name, clip, frame, error)
        if clip == 0:
            # The stock idle leans continuously from belt to shoulders. The
            # regressed back reversed direction halfway up the torso.
            assert np.min(np.diff(actual)) >= -.01, (path.parent.name, actual)
            chord = np.linspace(actual[0], actual[-1], len(actual))
            stock_chord = np.linspace(target[0], target[-1], len(target))
            assert np.abs(actual - chord).max() <= np.abs(target - stock_chord).max() + .02
        results[f"{clip}/{frame}"] = error
    return results


def check_animation_bank(path, clips):
    data = path.read_bytes()
    offset, size = rig_offset(data)
    rig = Rig(data[offset:offset+size])
    head = rig.provenance["boneNames"].index("Clown001Head")
    inverse_head = matrices(rig.blob, rig.h, 16)[head, :3, :3]
    for clip in clips:
        for driver in clip:
            world, bones = rig.evaluate(driver)
            assert np.isfinite(world).all() and np.abs(world[:, :3]).max() < 32760
            assert np.allclose(bones[head, :, :3] @ inverse_head, driver[7, :, :3], atol=2e-5)
    return sum(map(len, clips))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original-suits", required=True, type=Path)
    parser.add_argument("--donor", required=True, type=Path)
    parser.add_argument("--animation-bank", type=Path)
    args = parser.parse_args()
    donor = parse(args.donor.read_bytes())
    target = np.array([donor["origins"][i, 2] - donor["meshes"][i]["positions"][:, 2].min()
                       for i in (17, 14)])
    results = [measure(path, args.original_suits, target) for path in sorted(SUITS.glob("smu-*/actor.psx"))]
    animated = {}
    outlines = {}
    if args.animation_bank:
        stock = parse((SUITS.parents[3] / "Spider-Man/assets/builtin/spidey.psx").read_bytes())
        clips = read(args.animation_bank)
        for suit in ("smu-earth62412", "smu-earthx"):
            path = SUITS / suit / "actor.psx"
            outlines[suit] = check_bank_back(path, stock, clips)
            animated[suit] = check_animation_bank(path, clips)
    print(json.dumps({"donorToeReach": target.tolist(), "actors": results, "animatedFrames": animated,
                      "bankBackProfileRms": outlines}, indent=2))
