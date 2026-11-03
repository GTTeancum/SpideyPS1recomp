/*
 * Overlay activation and run-time calls, following the PC dispatcher
 * (RecompOne.Runtime.Dispatch.Dispatcher): an overlay's functions become callable
 * when it is loaded; loading one whose region wholly covers an active overlay's
 * drops that one. Later loads win when two active overlays share an address.
 */
#include <string.h>
#include "runtime.h"

#define ACTIVE_MAX 32

static const RecompGame *g_game;
static const RecompOverlay *g_active[ACTIVE_MAX];
static int g_active_count;

const RecompGame *rt_game(void) { return g_game; }

void rt_dispatch_init(const RecompGame *game)
{
    g_game = game;
    g_active_count = 0;
}

static int has_region(const RecompOverlay *o) { return o->base != 0 && o->size != 0; }

int rt_overlay_load(const char *name)
{
    const RecompOverlay *overlay = 0;
    for (int i = 0; i < g_game->overlay_count; i++)
        if (strcmp(g_game->overlays[i]->name, name) == 0) overlay = g_game->overlays[i];
    if (!overlay) return 0;

    for (int i = 0; i < g_active_count; i++)
        if (g_active[i] == overlay) {
            /* Already active: move it to the end so its functions win. */
            memmove(&g_active[i], &g_active[i + 1], (size_t)(g_active_count - i - 1) * sizeof g_active[0]);
            g_active[g_active_count - 1] = overlay;
            return 1;
        }

    if (has_region(overlay)) {
        uint32_t start = overlay->base & 0x1FFFFFFFu, end = start + overlay->size;
        int kept = 0;
        for (int i = 0; i < g_active_count; i++) {
            const RecompOverlay *o = g_active[i];
            uint32_t s = o->base & 0x1FFFFFFFu, e = s + o->size;
            if (has_region(o) && s >= start && e <= end) {
                rt_log("overlay %s overwritten by %s", o->name, overlay->name);
                continue;
            }
            g_active[kept++] = o;
        }
        g_active_count = kept;
    }
    if (g_active_count == ACTIVE_MAX) rt_fatal(0, "too many active overlays loading %s", name);
    g_active[g_active_count++] = overlay;
    rt_log("loaded overlay: %s", name);
    return 1;
}

static RecompFn find(const RecompOverlay *o, uint32_t address)
{
    int lo = 0, hi = o->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t a = o->functions[mid].address;
        if (a == address) return o->functions[mid].fn;
        if (a < address) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

volatile uint32_t rt_dispatch_count;

void recomp_dispatch(CpuContext *c, uint32_t address)
{
    rt_dispatch_count++;
    if (bios_dispatch(c, address)) return;
    for (int i = g_active_count - 1; i >= 0; i--) {
        RecompFn fn = find(g_active[i], address);
        if (fn) { fn(c); return; }
    }
    rt_fatal(c, "unmapped call to %08lX", (unsigned long)address);
}
