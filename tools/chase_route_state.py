"""Read ordinary SM1 Chase route state from native RAM snapshots; never write game RAM."""
import argparse
import json
import math
from pathlib import Path
import re
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('run', type=Path)
args = parser.parse_args()
console = args.run / 'console.log'
anchors = sorted({int(t) for t in re.findall(
    r"'l5a1_t\.trg' (?:load #\d+ )?at frame (\d+)",
    console.read_text(errors='replace') if console.exists() else '')})
rows = []
for path in sorted(args.run.glob('ram_*.bin')):
    tick = int(path.stem.removeprefix('ram_'))
    anchor = max((t for t in anchors if t <= tick), default=None)
    data = path.read_bytes()
    def valid(address, size=4):
        return 0x80000000 <= address and address+size <= 0x80000000+len(data)
    def unpack(fmt, address):
        if not valid(address, struct.calcsize(fmt)):
            raise ValueError(f'Invalid snapshot address {address:08x}')
        return struct.unpack_from(fmt, data, address-0x80000000)
    def u32(address):
        return unpack('<I', address)[0]
    def actor_state(address):
        if not 0x80000000 <= address < 0x80200000 or not valid(address, 0x324):
            return None
        return {'pointer': hex(address), 'xyz': [v/4096 for v in unpack('<iii', address+4)],
                'rotation_raw': unpack('<hhh', address+0x10)}
    player_ptr = u32(0x800B5268)
    player = actor_state(player_ptr)
    if player:
        player['script_active'] = u32(player_ptr+0x1A8)
    if player and valid(player_ptr, 0xF54):
        # Native fields are read only. CA4 can retain an older contact, so it
        # must not be interpreted as the current blocking face without a join.
        player['native_state'] = hex(u32(player_ptr+0xF3C))
        player['wall_attached'] = u32(player_ptr+0xBC4)
        player['surface_normal'] = unpack('<3h', player_ptr+0xA4)
        player['last_contact_pointer'] = hex(u32(player_ptr+0xCA4))
        player['yaw'] = unpack('<h', player_ptr+0x12)[0]
        player['input_angle'] = unpack('<h', player_ptr+0xF52)[0]
        cursor = u32(player_ptr+0x1B4)
        player['script_cursor'] = hex(cursor)
        if valid(cursor, 26):
            player['script_cursor_words'] = unpack('<13H', cursor)
    camera_ptr = u32(0x800B5834)
    camera = None
    if valid(camera_ptr, 0x23C):
        camera = {'pointer': hex(camera_ptr),
                  'input_reference_yaw': unpack('<h', camera_ptr+0x23A)[0]}
    actor = u32(0x800B5234)
    venom = None
    visited = set()
    for _ in range(1024):
        if actor in visited or not 0x80000000 <= actor < 0x80200000 or not valid(actor, 0x324):
            break
        visited.add(actor)
        if unpack('<H', actor+0x34)[0] == 0x139:
            venom = actor_state(actor)
            script, task = u32(actor+0x31C), u32(actor+0x320)
            venom.update(script=hex(script), task=hex(task))
            if valid(script, 12):
                venom['script_words'] = unpack('<6H', script)
            if valid(task, 16):
                venom['task_words'] = unpack('<4I', task)
            break
        actor = u32(actor+0x1C)
    buttons = {}
    for name, index in [('left', 8), ('right', 9), ('up', 10), ('down', 11),
                        ('cross', 3), ('r1', 6), ('r2', 7)]:
        # 8006B514 packs pad byte 2 into the HIGH byte before masking, unlike
        # Controller.State. Its records are face buttons first, then shoulders,
        # directions and system buttons; do not use the host's mask ordering.
        held, pressed = unpack('<BB', 0x800A4DF4+index*16)
        buttons[name] = {'held': held, 'pressed': pressed}
    rows.append({'snapshot': path.name, 'tick': tick, 'level_anchor_tick': anchor,
                 'level_tick': tick-anchor if anchor is not None else None,
                 'game_counter': u32(0x800B4F38),
                 'player': player, 'camera': camera, 'venom': venom, 'native_buttons': buttons,
                 'distance': math.dist(player['xyz'], venom['xyz']) if player and venom else None})
if not rows:
    raise SystemExit('No native snapshots found')
(args.run/'route-state.json').write_text(json.dumps(rows, indent=2))
print(json.dumps(rows, indent=2))
