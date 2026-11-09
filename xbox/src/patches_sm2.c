/*
 * Spider-Man 2: Enter Electro patches, ported from spiderman2/patches (C#). Keep both
 * in step: a gameplay fix for every platform goes in both (see xbox/README.md).
 * Shared with Spider-Man 1: src/patches_common.c (GpuPatches, PadPatches).
 *
 * Ported: overlays, the vblank wait, the menu packet order, level switching
 * (level_sm2.c), and the Dreamcast and suit models (costume_sm2.c, wad_overrides.c).
 * Not yet: the world subpixel and frustum hooks, movies, shadows and the test hooks.
 */
#include "runtime.h"
#include "settings.h"
#include "wad_overrides.h"
#include "SpiderMan2.h"

#define A0 c->r[4]
#define V0 c->r[2]

/* ---- OverlayPatches (spiderman2/patches/OverlayPatches.cs): code overlays live at the
 * fixed addresses they were recompiled for. The allocation LoadOverlay (0x8001E8E4)
 * makes right after finding "<name>.bin" in CD.WAD is answered with that address
 * instead of a heap block, and the overlay activated. ------------------------------- */
#define REGION_LO 0x80200000u
#define REGION_HI 0x80280000u

static const RecompOverlay *g_pending_overlay;
static uint32_t g_pending_size;

static const RecompOverlay *overlay_named(const char *name)
{
    const RecompGame *g = rt_game();
    for (int i = 0; i < g->overlay_count; i++)
        if (g->overlays[i]->base && rt_strcasecmp(g->overlays[i]->name, name) == 0) return g->overlays[i];
    return 0;
}

int hook_OverlayPatches_CdWadFind(CpuContext *c)
{
    level_sm2_redirect(c);
    char name[64];
    int n = 0;
    for (uint32_t a = A0; n < 63; a++) { char ch = (char)mem_r8(a); if (!ch) break; name[n++] = ch; }
    name[n] = 0;
    g_pending_overlay = 0;
    if (n > 4 && rt_strcasecmp(name + n - 4, ".bin") == 0) {
        name[n - 4] = 0;
        g_pending_overlay = overlay_named(name);
        name[n - 4] = '.';
    }
    /* A prepared actor answers in place of the disc's entry; an alias the WAD does not
     * hold is answered here and the game's own lookup skipped. */
    return !costume_sm2_find(c, name);
}

void hook_OverlayPatches_CdWadFindExit(CpuContext *c)
{
    wad_find_exit(c);
    if (g_pending_overlay) g_pending_size = V0;
}

int hook_OverlayPatches_HeapAlloc(CpuContext *c)
{
    if (g_pending_overlay && A0 == g_pending_size) {
        V0 = g_pending_overlay->base;
        rt_overlay_load(g_pending_overlay->name);
        g_pending_overlay = 0;
        return 0;
    }
    if (costume_sm2_heap_alloc(c) || wad_heap_alloc(c)) return 0;
    return 1;
}

/* The overlay region and the expanded-RAM arena are not heap: their blocks never reach
 * the retail free lists; an arena block is freed back to the arena. */
int hook_OverlayPatches_HeapFree(CpuContext *c)
{
    if (wad_free(A0)) return 0;
    return !(A0 >= REGION_LO && A0 < REGION_HI);
}
/* An arena block keeps its sector-rounded tail until it is freed. */
int hook_OverlayPatches_HeapShrink(CpuContext *c) { return !wad_owns(A0); }

/* Pre CdWadRead (LooseWadOverrides.Read): read a pending override instead. */
int hook_LooseWadOverrides_Read(CpuContext *c) { return !wad_read(c); }

/* ---- Menu packet order (MenuPacketOrder.SingleMetadataTail): func_80075438 has SM1's
 * single-DR_AREA alias; left alone the ordering table becomes a cycle and DrawOTag
 * never returns. As SM1's (src/patches_sm1.c), with SM2's ramp counter at gp+0x1010. */
int hook_MenuPacketOrder_SingleMetadataTail(CpuContext *c)
{
    const uint32_t mask = 0xFFFFFF;
    uint32_t anchor = c->r[8] & mask, tail = c->r[10] & mask;
    if (tail != (c->r[7] & mask) || tail == anchor || tail > 0x7FFFF4 || anchor > 0x7FFFFC || ((tail | anchor) & 3))
        return 0;
    uint32_t header = mem_r32(tail);
    if (header != 0x02FFFFFFu || mem_r32(tail + 4) >> 24 != 0xE3 || mem_r32(tail + 8) >> 24 != 0xE4) return 0;
    /* Retain clipping, backdrop insertion, and the foreground anchor once each. */
    mem_w32(anchor, (mem_r32(anchor) & 0xFF000000u) | mask);
    mem_w32(tail, (header & 0xFF000000u) | anchor);
    uint32_t ramp_address = c->r[28] + 0x1010u, ramp = mem_r32(ramp_address) + 0x60u;
    mem_w32(ramp_address, (int32_t)ramp > 0x300 ? 0x300u : ramp);
    return 1;
}

/* ---- Timing: retail 80069124 waits on the counter the game's vblank IRQ increments. */
void hook_Timing_WaitVBlanks(CpuContext *c) { rt_wait_counter(c, c->r[28] + 0xCD0u); }
