/*
 * The PSY-Q graphics library calls the configuration replaces, a port of the PC
 * runtime's LibGpu feeding the software GPU (gpu.c). Drawing is immediate, so
 * DrawSync never waits.
 */
#include "hwgpu.h"
#include "gpu.h"
#include "runtime.h"

#define A0 c->r[4]
#define A1 c->r[5]
#define A2 c->r[6]
#define A3 c->r[7]
#define V0 c->r[2]

static int16_t s16(uint32_t a) { return (int16_t)mem_r16(a); }
static int16_t clamp16(int v, int lo, int hi) { return (int16_t)(v < lo ? lo : v > hi ? hi : v); }

static uint32_t get_cs(int x, int y)
{
    x = clamp16(x, 0, VRAM_W - 1); y = clamp16(y, 0, VRAM_H - 1);
    return 0xE3000000u | (((uint32_t)y & 0x3FF) << 10) | ((uint32_t)x & 0x3FF);
}
static uint32_t get_ce(int x, int y)
{
    x = clamp16(x, 0, VRAM_W - 1); y = clamp16(y, 0, VRAM_H - 1);
    return 0xE4000000u | (((uint32_t)y & 0x3FF) << 10) | ((uint32_t)x & 0x3FF);
}
static uint32_t get_ofs(int x, int y) { return 0xE5000000u | (((uint32_t)y & 0x7FF) << 11) | ((uint32_t)x & 0x7FF); }
static uint32_t get_mode(int dfe, int dtd, uint32_t tpage)
{
    return (dtd ? 0xE1000200u : 0xE1000000u) | (dfe ? 0x400u : 0u) | (tpage & 0x9FF);
}
static uint32_t get_tw(int x, int y, int w, int h)
{
    uint32_t c0 = ((uint32_t)x & 0xFF) >> 3, c1 = ((uint32_t)y & 0xFF) >> 3;
    uint32_t c2 = ((uint32_t)(-w) & 0xFF) >> 3, c3 = ((uint32_t)(-h) & 0xFF) >> 3;
    return 0xE2000000u | (c1 << 15) | (c0 << 10) | (c3 << 5) | c2;
}

volatile uint32_t rt_ot_start, rt_ot_addr, rt_ot_packets;
uint32_t rt_disp_grandparent;
void (*rt_frame_hook)(void);
void (*rt_drawotag_hook)(void);
volatile uint32_t rt_drawotag_count;

void hook_LibGpu_DrawOTag(CpuContext *c)
{
    rt_note("DrawOTag");
    hw_depth_frame();
    rt_drawotag_count++;
    RT_WHERE("DrawOTag");
    if (rt_drawotag_hook) rt_drawotag_hook();
    uint32_t addr = A0 & (RECOMP_RAM_SIZE - 4u);
    rt_ot_start = addr;
    for (int guard = 0; guard < 0x100000; guard++) {
        rt_ot_packets = (uint32_t)guard;
        rt_ot_addr = addr;
        uint32_t header = mem_r32(addr), count = header >> 24;
        for (uint32_t i = 0; i < count; i++) gpu_write_gp0(mem_r32(addr + 4u + i * 4u));
        uint32_t next = header & 0xFFFFFFu;
        if (next == 0xFFFFFFu || (next & 0x800000u)) break;
        addr = next & (RECOMP_RAM_SIZE - 4u);
    }
}

void hook_LibGpu_DrawSync(CpuContext *c) { V0 = 0; }

void hook_LibGpu_PutDrawEnv(CpuContext *c)
{
    uint32_t env = A0;
    int clip_x = s16(env + 0x00), clip_y = s16(env + 0x02), clip_w = s16(env + 0x04), clip_h = s16(env + 0x06);
    int ofs_x = s16(env + 0x08), ofs_y = s16(env + 0x0A);
    int tw_x = s16(env + 0x0C), tw_y = s16(env + 0x0E), tw_w = s16(env + 0x10), tw_h = s16(env + 0x12);
    uint32_t tpage = mem_r16(env + 0x14);
    int dtd = (int)mem_r8(env + 0x16), dfe = (int)mem_r8(env + 0x17), isbg = (int)mem_r8(env + 0x18);
    uint32_t r0 = mem_r8(env + 0x19), g0 = mem_r8(env + 0x1A), b0 = mem_r8(env + 0x1B);
    gpu_write_gp0(get_cs(clip_x, clip_y));
    gpu_write_gp0(get_ce(clip_x + clip_w - 1, clip_y + clip_h - 1));
    gpu_write_gp0(get_ofs(ofs_x, ofs_y));
    gpu_write_gp0(get_mode(dfe, dtd, tpage));
    gpu_write_gp0(get_tw(tw_x, tw_y, tw_w, tw_h));
    gpu_write_gp0(0xE6000000u);
    if (isbg) {
        int w = clip_w < 0 ? 0 : clip_w > VRAM_W - 1 ? VRAM_W - 1 : clip_w;
        int h = clip_h < 0 ? 0 : clip_h > VRAM_H - 1 ? VRAM_H - 1 : clip_h;
        int x = clip_x - ofs_x, y = clip_y - ofs_y;
        gpu_write_gp0(0x60000000u | (b0 << 16) | (g0 << 8) | r0);
        gpu_write_gp0(((uint32_t)(uint16_t)y << 16) | (uint16_t)x);
        gpu_write_gp0(((uint32_t)(uint16_t)h << 16) | (uint16_t)w);
    }
    V0 = A0;
}

void hook_LibGpu_PutDispEnv(CpuContext *c)
{
    rt_note("PutDispEnv");
    rt_disp_grandparent = mem_r32(c->r[29] + 16u);
    if (rt_frame_hook) rt_frame_hook();
    uint32_t env = A0;
    int disp_x = s16(env + 0x00), disp_y = s16(env + 0x02), disp_w = s16(env + 0x04), disp_h = s16(env + 0x06);
    int scr_x = s16(env + 0x08), scr_y = s16(env + 0x0A), scr_w = s16(env + 0x0C), scr_h = s16(env + 0x0E);
    int isinter = (int)mem_r8(env + 0x10), isrgb24 = (int)mem_r8(env + 0x11);
    gpu_write_gp1(0x05000000u | (((uint32_t)disp_y & 0x3FF) << 10) | ((uint32_t)disp_x & 0x3FF));
    int h_start = scr_x * 10 + 0x260, v_start = scr_y + 0x10;
    int h_end = h_start + (scr_w ? scr_w * 10 : 2560), v_end = v_start + (scr_h ? scr_h : 240);
    h_start = h_start < 500 ? 500 : h_start > 3290 ? 3290 : h_start;
    h_end = h_end < h_start + 0x50 ? h_start + 0x50 : h_end > 3290 ? 3290 : h_end;
    v_start = v_start < 0x10 ? 0x10 : v_start > 256 ? 256 : v_start;
    v_end = v_end < v_start + 2 ? v_start + 2 : v_end > 258 ? 258 : v_end;
    gpu_write_gp1(0x06000000u | (((uint32_t)h_end & 0xFFF) << 12) | ((uint32_t)h_start & 0xFFF));
    gpu_write_gp1(0x07000000u | (((uint32_t)v_end & 0x3FF) << 10) | ((uint32_t)v_start & 0x3FF));
    uint32_t mode = 0x08000000u;
    if (isrgb24) mode |= 0x10;
    if (isinter) mode |= 0x20;
    if (disp_w <= 280) { }
    else if (disp_w <= 352) mode |= 1;
    else if (disp_w <= 400) mode |= 0x40;
    else if (disp_w <= 560) mode |= 2;
    else mode |= 3;
    if (disp_h > 256) mode |= 0x24;
    gpu_write_gp1(mode);
    V0 = A0;
}

void hook_LibGpu_LoadImage(CpuContext *c)
{
    rt_note("LoadImage");
    uint32_t rect = A0, src = A1;
    uint32_t x = mem_r16(rect), y = mem_r16(rect + 2), w = mem_r16(rect + 4), h = mem_r16(rect + 6);
    V0 = 0;
    if (!w || !h) return;
    gpu_write_gp0(0xA0000000u);
    gpu_write_gp0((y << 16) | x);
    gpu_write_gp0((h << 16) | w);
    uint32_t words = (w * h + 1u) >> 1;
    for (uint32_t i = 0; i < words; i++) gpu_write_gp0(mem_r32(src + i * 4u));
}

void hook_LibGpu_StoreImage(CpuContext *c)
{
    uint32_t rect = A0, dst = A1;
    uint32_t x = mem_r16(rect), y = mem_r16(rect + 2), w = mem_r16(rect + 4), h = mem_r16(rect + 6);
    V0 = 0;
    if (!w || !h) return;
    gpu_write_gp0(0xC0000000u);
    gpu_write_gp0((y << 16) | x);
    gpu_write_gp0((h << 16) | w);
    uint32_t words = (w * h + 1u) >> 1;
    for (uint32_t i = 0; i < words; i++) mem_w32(dst + i * 4u, gpu_read_data());
}

void hook_LibGpu_MoveImage(CpuContext *c)
{
    uint32_t rect = A0;
    uint32_t sx = mem_r16(rect), sy = mem_r16(rect + 2), w = mem_r16(rect + 4), h = mem_r16(rect + 6);
    V0 = 0;
    if (!w || !h) return;
    gpu_write_gp0(0x80000000u);
    gpu_write_gp0((sy << 16) | sx);
    gpu_write_gp0(((A2 & 0xFFFFu) << 16) | (A1 & 0xFFFFu));   /* destination is a1/a2 */
    gpu_write_gp0((h << 16) | w);
}

void hook_LibGpu_ClearImage(CpuContext *c)
{
    uint32_t rect = A0;
    uint32_t x = mem_r16(rect), y = mem_r16(rect + 2), w = mem_r16(rect + 4), h = mem_r16(rect + 6);
    V0 = 0;
    if (!w || !h) return;
    gpu_write_gp0(0x02000000u | ((A3 & 0xFFu) << 16) | ((A2 & 0xFFu) << 8) | (A1 & 0xFFu));
    gpu_write_gp0((y << 16) | x);
    gpu_write_gp0((h << 16) | w);
}
