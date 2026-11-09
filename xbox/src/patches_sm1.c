/*
 * Spider-Man 1 patches, ported from spiderman/patches (C#). Keep both in step: a
 * gameplay fix for every platform goes in both (see xbox/README.md).
 */
#include <string.h>
#include "gpu.h"
#include "settings.h"
#include "runtime.h"
#include "SpiderMan.h"

#define A0 c->r[4]
#define A1 c->r[5]
#define V0 c->r[2]

/* GpuPatches and PadPatches are shared with Spider-Man 2: src/patches_common.c. */

/* ---- OverlayPatches: code overlays live at the fixed addresses they were recompiled
 * for. The allocation LoadOverlay makes right after finding "<name>.bin" in CD.WAD is
 * answered with that address instead of a heap block, and the overlay activated. ---- */
#define REGION_LO 0x80200000u
#define REGION_HI 0x802A0000u

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
    level_sm1_redirect(c);
    char name[64];
    int n = 0;
    for (uint32_t a = A0; n < 63; a++) { char ch = (char)mem_r8(a); if (!ch) break; name[n++] = ch; }
    name[n] = 0;
    g_pending_overlay = 0;
    assets_sm1_find(c, name);
    if (n > 4 && rt_strcasecmp(name + n - 4, ".bin") == 0) {
        name[n - 4] = 0;
        g_pending_overlay = overlay_named(name);
    }
    return 1;
}

void hook_OverlayPatches_CdWadFindExit(CpuContext *c)
{
    assets_sm1_find_exit(c);
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
    return !assets_sm1_heap_alloc(c);
}

int hook_OverlayPatches_HeapFree(CpuContext *c)
{
    if (assets_sm1_heap_free(c)) return 0;
    return !(A0 >= REGION_LO && A0 < REGION_HI);
}
int hook_OverlayPatches_HeapShrink(CpuContext *c) { return !assets_sm1_owns(A0); }

/* ---- Timing ------------------------------------------------------------------------------ */
/* Retail 8005E748 waits on the counter the game's vblank IRQ increments.
 * TODO(xbox): the Venom chase pacing in Timing.WaitVBlanks (gameplay fix). */
void hook_Timing_WaitVBlanks(CpuContext *c) { rt_wait_counter(c, c->r[28] + 0xC74u); }

/* ---- Pad reads: the title screen polls the pad in a loop without VSync. ---------- */
void hook_PadTrace_AfterRead(CpuContext *c) { (void)c; rt_idle(); }
void hook_ArmorTest_AfterPadRead(CpuContext *c) { (void)c; }

/* ---- Menu packet order (MenuPacketOrder.SingleMetadataTail): func_8006A21C moves
 * OT[0] behind the last polygon; with only a terminal DR_AREA its fallback aliases
 * that packet and the ordering table becomes a cycle. --------------------------------- */
int hook_MenuPacketOrder_SingleMetadataTail(CpuContext *c)
{
    const uint32_t mask = 0xFFFFFF;
    uint32_t anchor = c->r[8] & mask, tail = c->r[10] & mask;
    if (tail != (c->r[7] & mask) || tail == anchor || tail > 0x7FFFF4 || anchor > 0x7FFFFC || ((tail | anchor) & 3))
        return 0;
    uint32_t header = mem_r32(tail);
    if (header != 0x02FFFFFFu || mem_r32(tail + 4) >> 24 != 0xE3 || mem_r32(tail + 8) >> 24 != 0xE4) return 0;
    /* Keep the clip packet, then the backdrop insertion point, then OT[0] for the
     * foreground UI that follows. */
    mem_w32(anchor, (mem_r32(anchor) & 0xFF000000u) | mask);
    mem_w32(tail, (header & 0xFF000000u) | anchor);
    uint32_t ramp_address = c->r[28] + 0xEC8u, ramp = mem_r32(ramp_address) + 0x60u;
    mem_w32(ramp_address, (int32_t)ramp > 0x300 ? 0x300u : ramp);
    return 1;
}
