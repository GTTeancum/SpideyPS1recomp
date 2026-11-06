/*
 * The console clock, following the PC runtime (Runtime.PresentFrame, LibEtc,
 * Interrupts): one tick per 60 Hz vblank. Each tick services the CD and delivers
 * the vblank interrupt to the game's handler; every second tick presents the
 * display area. Only VSync and the game's counter wait advance the clock.
 */
#include <hal/video.h>
#include <windows.h>
#include "gpu.h"
#include "hwgpu.h"
#include "runtime.h"

volatile uint32_t rt_vblank_count;
volatile int rt_presenting;
int rt_video_720p, rt_video_wide, rt_frame_wide;
volatile const char *rt_where = "";
static LARGE_INTEGER g_frequency, g_next;
static int g_ticks_until_present;

/* ---- interrupts ------------------------------------------------------------------ */
static int g_in_handler;
static int g_pending[16];

static void dispatch_irq(int irq)
{
    uint32_t env = bios_intr_env();
    if (!env) return;
    uint32_t handler = mem_r32(env + 2u + (uint32_t)irq * 4u);
    if (!handler) return;
    CpuContext *c = rt_cpu();
    CpuContext saved = *c;
    mem_w16(env, 1);
    recomp_dispatch(c, handler);
    mem_w16(env, 0);
    *c = saved;
}

void rt_raise_irq(int irq)
{
    if (irq < 0 || irq >= 16 || !rt_cpu()) return;
    if (g_in_handler) { g_pending[irq] = 1; return; }
    g_in_handler = 1;
    dispatch_irq(irq);
    for (int again = 1; again;) {
        again = 0;
        for (int i = 0; i < 16; i++)
            if (g_pending[i]) { g_pending[i] = 0; dispatch_irq(i); again = 1; }
    }
    g_in_handler = 0;
}

/* ---- presentation ------------------------------------------------------------------ */
static void present(void)
{
    GpuDisplay d;
    gpu_display(&d);
    if (!d.enabled || d.width <= 0 || d.height <= 0 || d.rgb24) return;
#ifndef RT_SOFTWARE_GPU
    static int hw_tried;
    if (!hw_tried) { hw_tried = 1; hw_init(); }
    if (hw_active()) {
        rt_presenting = 1;
        hw_note_display(d.x, d.y, d.width, d.height);
        hw_present(d.x, d.y, rt_frame_wide);
        return;
    }
#endif
    rt_presenting = 1;
    uint32_t *fb = (uint32_t *)XVideoGetFB();
    if (!fb) return;
    for (int y = 0; y < 480; y++) {
        const uint16_t *row = gpu_vram[(d.y + y * d.height / 480) & (VRAM_H - 1)];
        uint32_t *out = fb + y * 640;
        for (int x = 0; x < 640; x++) {
            uint16_t p = row[(d.x + x * d.width / 640) & (VRAM_W - 1)];
            uint32_t r = (p & 0x1Fu) << 3, g = ((p >> 5) & 0x1Fu) << 3, b = ((p >> 10) & 0x1Fu) << 3;
            out[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

/* ---- the clock ------------------------------------------------------------------------ */
void rt_clock_init(void)
{
    QueryPerformanceFrequency(&g_frequency);
    QueryPerformanceCounter(&g_next);
}

/* Wait for the next 60 Hz edge and do what a console vblank does. */
void rt_tick(void)
{
    LONGLONG period = g_frequency.QuadPart / 60;
    g_next.QuadPart += period;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (now.QuadPart - g_next.QuadPart > period * 4) g_next = now;   /* far behind: drop ticks */
    while (now.QuadPart < g_next.QuadPart) {
        LONGLONG left = g_next.QuadPart - now.QuadPart;
        Sleep(left > g_frequency.QuadPart / 500 ? 1 : 0);
        QueryPerformanceCounter(&now);
    }
    RT_WHERE("tick present");
    if (++g_ticks_until_present >= 2) { g_ticks_until_present = 0; present(); }
    RT_WHERE("tick cd");
    cd_tick();
    RT_WHERE("tick pad");
    rt_pad_refresh();
    RT_WHERE("tick irq");
    rt_vblank_count++;
    rt_raise_irq(0);
    RT_WHERE("game");
}

/* VSync(mode): a negative mode polls the counter, servicing an edge when one is due;
 * any other mode waits one edge. Same as the PC's LibEtc.VSync. */
void hook_LibEtc_VSync(CpuContext *c)
{
    rt_note("VSync");
    if ((int32_t)c->r[4] < 0) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (now.QuadPart >= g_next.QuadPart + g_frequency.QuadPart / 60) rt_tick();
        c->r[2] = rt_vblank_count;
        return;
    }
    rt_tick();
    c->r[2] = 0;
}

/*
 * Idle-loop breaker (the PC's Runtime.IdleTick): a game waiting for an interrupt by
 * spinning on a variable its IRQ handler writes never calls VSync, and here
 * interrupts only arrive with a tick. Spinning code paths (BIOS calls, pad reads)
 * call this; it delivers a tick only when a real 60 Hz edge is due, so time neither
 * stalls nor runs ahead. The clock is read once every 256 calls to stay cheap.
 */
void rt_idle(void)
{
    static unsigned calls;
    static int busy;
    if ((++calls & 255u) || busy || !rt_cpu()) return;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (now.QuadPart < g_next.QuadPart + g_frequency.QuadPart / 60) return;
    busy = 1;
    rt_tick();
    busy = 0;
}

/* The game's own wait on the counter its vblank IRQ increments (LibEtc.WaitCounter). */
void rt_wait_counter(CpuContext *c, uint32_t address)
{
    uint32_t target = mem_r32(address) + c->r[4];
    while (mem_r32(address) < target) rt_tick();
    c->r[3] = target;
    c->r[2] = 0;
}
