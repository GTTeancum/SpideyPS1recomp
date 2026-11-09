/*
 * The recomp's reserved blocks in the 8 MB guest RAM window, above what either game
 * can reach (retail code, data, heap and stack live below 0x80200000; the code
 * overlays at 0x80200000-0x80280000). Nothing the game allocates comes from here, and
 * these are the only addresses the patches use for their own data -- the same layout
 * as the PC's (LooseWadOverrides.ArenaLo/Hi, TextureRegistry.Table, Costume.ViewerTable,
 * Costume.ScratchName).
 */
#pragma once

/* Loose WAD files and the frame-packet pools (wad_overrides.c's allocator). */
#define GUEST_ARENA_LO          0x80300000u
#define GUEST_ARENA_HI          0x80780000u
/* SM1's relocated texture registry: 8-byte records (tools/patch_generated_sm1.py). */
#define GUEST_TEXTURE_REGISTRY  0x80780000u
/* The costume viewer: 12-byte rows, the names at +0x8000, the descriptions at
 * +0x10000, up to GUEST_VIEWER_END (rewritten each time the viewer opens). */
#define GUEST_VIEWER_TABLE      0x807C0000u
#define GUEST_VIEWER_END        0x807F0000u
/* Short file names passed to the game's own loaders (32 bytes each). */
#define GUEST_SCRATCH_NAMES     0x807F0000u
#define GUEST_SCRATCH_END       0x807F0100u
