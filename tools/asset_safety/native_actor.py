"""Validate the native PSX animation stride before an override reaches guest RAM.

The native loader counts terminal LOD meshes as bones. Extra terminal meshes
change the animation stride and can write beyond the original pose scratch.
This does NOT alter, strip, reweight, or rebuild a model.
"""
from __future__ import annotations
import struct
from dataclasses import dataclass

@dataclass(frozen=True)
class ActorLayout:
    objects: int
    meshes: int
    terminal_lods: int
    animated: bool


def inspect_actor_lods(data: bytes, name: str = "actor.psx", max_unpacked_bones: int = 0) -> ActorLayout:
    if max_unpacked_bones < 0: raise ValueError("negative unpacked animation capacity")
    def need(offset: int, size: int) -> None:
        if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
            raise ValueError(f"{name}: native actor record is out of bounds ({offset}+{size}/{len(data)})")
    def u32(offset: int) -> int:
        need(offset, 4)
        return struct.unpack_from("<I", data, offset)[0]
    need(0, 12)
    objects = u32(8)
    if objects == 0:
        return ActorLayout(0, 0, 0, False)  # texture-only companions
    table = 12 + objects * 36
    need(12, objects * 36)
    metadata = u32(4)
    if metadata < table + 4:
        raise ValueError(f"{name}: metadata overlaps the native object table")
    need(metadata, 4)
    cursor = metadata
    animated = False
    unpacked = False
    blocks = 0
    while True:
        tag = u32(cursor)
        cursor += 4
        if tag == 0xFFFFFFFF:
            break
        blocks += 1
        if blocks > 4096:
            raise ValueError(f"{name}: too many native metadata blocks")
        size = u32(cursor)
        cursor += 4
        need(cursor, size)
        animated |= tag in (0x2A, 0x2C)
        unpacked |= tag == 0x2A
        cursor += size
    if not animated:
        return ActorLayout(objects, 0, 0, False)
    count = u32(table)
    if count > (metadata - table - 4) // 4:
        raise ValueError(f"{name}: mesh pointer table overlaps metadata")
    need(table + 4, count * 4)
    first_mesh = table + 4 + count * 4
    terminal = 0
    for i in range(count):
        mesh = u32(table + 4 + 4 * i)
        if mesh < first_mesh or mesh > metadata - 28:
            raise ValueError(f"{name}: native mesh header is outside its section")
        terminal += struct.unpack_from("<H", data, mesh + 26)[0] == 0xFFFF
    # Some static one-mesh props carry an animation tag with no terminal link.
    # Do not force equality or invent a new bone count for those stock assets.
    if terminal > objects:
        raise ValueError(f"{name}: {terminal} terminal LODs for {objects} animation bones")
    if max_unpacked_bones > 0 and unpacked and terminal > max_unpacked_bones:
        raise ValueError(f"{name}: {terminal} unpacked animation bones exceed the game profile capacity {max_unpacked_bones}")
    return ActorLayout(objects, count, terminal, True)
