/*
 * 16:9 widescreen for Spider-Man: a port of the PC's Wide.cs and WorldFrustum.cs
 * (spiderman/patches). A gameplay fix to either belongs in both places.
 *
 * The field of view is widened at projection: the GTE squeezes projected X toward the
 * centre by 3/4 (gte.c), so the game culls and orders for the wider view and submits
 * the extra scenery itself, and the frame is presented at 16:9. SM1's object bounds
 * tests run before projection, so WorldFrustum widens their horizontal planes to
 * match. The HUD does not come through the GTE; the top-left panel (health, web
 * cartridges) and the chase meter are squeezed by the same fraction about their near
 * edge, so presentation stretches them back to the drawn shape and they sit against the
 * wider screen's edges. Menus, FMV and everything outside the gameplay loop stay 4:3.
 *
 * Widescreen follows the console: on with 720p or a 480 TV set to widescreen.
 */
#include <math.h>
#include "runtime.h"

#define A0 c->r[4]

/* SM1's gameplay loop buffer swap: PutDispEnv's caller's caller in gameplay. */
#define SM1_GAMEPLAY_SWAP 0x8002C2ACu
/* 4:3 of view in a 16:9 frame: projected X x 1000/1333. */
#define FOV_NUM 1000
#define FOV_DEN 1333

static int g_wide;

/* ---- HUD elements ------------------------------------------------------------------- */
/* Boxes of the HUD elements squeezed this frame, consulted the next frame so a detail
 * (the compass needle, submitted before its ring) can move with its element. */
#define ELEMENTS 16
#define MAX_DETAIL 40
typedef struct { int x0, x1, y0, y1; } Box;
static Box g_cur[ELEMENTS], g_prev[ELEMENTS];
static int g_cur_n, g_prev_n;

static void remember(int lo, int hi, int top, int bot)
{
    if (g_cur_n >= ELEMENTS) return;
    Box b = { lo, hi, top, bot };
    g_cur[g_cur_n++] = b;
}

static void roll_elements(void)
{
    for (int i = 0; i < g_cur_n; i++) g_prev[i] = g_cur[i];
    g_prev_n = g_cur_n;
    g_cur_n = 0;
}

/* Health and web cartridges: an element-sized box in the top-left of the frame. */
static int in_hud_corner(int lo, int hi, int top, int bot, int w, int h)
{
    if (hi - lo > w / 4 || bot - top > h / 3) return 0;
    return hi <= w * 2 / 5 && bot <= h * 2 / 5;
}

/* The remembered element this small detail sits in, or -1. */
static int carry_host(int lo, int hi, int top, int bot)
{
    if (hi - lo > MAX_DETAIL || bot - top > MAX_DETAIL) return -1;
    const int slack = 12;
    int cx = (lo + hi) / 2, cy = (top + bot) / 2;
    for (int i = 0; i < g_prev_n; i++)
        if (cx >= g_prev[i].x0 - slack && cx <= g_prev[i].x1 + slack &&
            cy >= g_prev[i].y0 - slack && cy <= g_prev[i].y1 + slack) return i;
    return -1;
}

/* An exact axis-aligned rectangle: the HUD's quads and sprites, not GTE geometry. */
static int screen_aligned(const RtPrim *p)
{
    if (p->count == 2) return 1;
    if (p->count != 4) return 0;
    int xs = 1, ys = 1, ox = p->x[0], oy = p->y[0];
    for (int i = 1; i < 4; i++) {
        if (p->x[i] != p->x[0] && p->x[i] != ox) { if (xs == 2) return 0; ox = p->x[i]; xs = 2; }
        if (p->y[i] != p->y[0] && p->y[i] != oy) { if (ys == 2) return 0; oy = p->y[i]; ys = 2; }
    }
    return xs == 2 && ys == 2;
}

/* The Chase Venom meter, by its HUD atlas identity (it moves during transitions). */
static int chase_meter(const RtPrim *p)
{
    if (!p->textured || p->count != 4) return 0;
    static const struct { int tpage, clut, u0, v0, u1, v1; } parts[] = {
        { 8, 418, 108, 248, 111, 255 },     /* left cap */
        { 12, 418, 52, 168, 55, 175 },      /* right cap */
        { 8, 419, 240, 248, 251, 255 },     /* repeated rail */
        { 136, 3552, 126, 152, 143, 177 },  /* Spider-Man marker */
        { 138, 3616, 32, 208, 61, 237 },    /* Venom marker */
    };
    for (unsigned k = 0; k < sizeof parts / sizeof parts[0]; k++) {
        if ((p->tpage & ~0x60) != parts[k].tpage || p->clut != parts[k].clut) continue;
        for (int i = 0; i < 4; i++)
            if (p->u[i] < parts[k].u0 || p->u[i] > parts[k].u1 || p->v[i] < parts[k].v0 || p->v[i] > parts[k].v1)
                return 0;
        return 1;
    }
    return 0;
}

/* Scale about the near edge (anchor -1 left, 1 right), keeping the element's distance
 * from that edge so it sits where the 4:3 layout puts it. */
static void squeeze(RtPrim *p, int anchor)
{
    int origin = anchor < 0 ? p->left : anchor > 0 ? p->right : (p->left + p->right) / 2;
    for (int i = 0; i < p->count; i++)
        p->x[i] = origin + (int)((int64_t)(p->x[i] - origin) * FOV_NUM / FOV_DEN);
}

static void screen(RtPrim *p)
{
    if (!g_wide || p->depth) return;          /* world geometry is widened by the GTE */
    int lo = p->x[0], hi = p->x[0], top = p->y[0], bot = p->y[0];
    for (int i = 1; i < p->count; i++) {
        if (p->x[i] < lo) lo = p->x[i];
        if (p->x[i] > hi) hi = p->x[i];
        if (p->y[i] < top) top = p->y[i];
        if (p->y[i] > bot) bot = p->y[i];
    }
    /* Buffer-relative: the game draws alternate frames at y 0 and 256. */
    int rlo = lo - p->left, rhi = hi - p->left, rtop = top - p->top, rbot = bot - p->top;
    int w = p->right - p->left + 1, h = p->bottom - p->top + 1;
    int aligned = screen_aligned(p);
    if (aligned && chase_meter(p)) { squeeze(p, 1); return; }
    int panel = aligned && in_hud_corner(rlo, rhi, rtop, rbot, w, h);
    int host = panel ? -1 : carry_host(rlo, rhi, rtop, rbot);
    if (!panel && host < 0) return;
    int anchor = host >= 0 ? ((g_prev[host].x0 + g_prev[host].x1) / 2 <= w / 2 ? -1 : 1)
                           : ((rlo + rhi) / 2 <= w / 2 ? -1 : 1);
    squeeze(p, anchor);
    if (panel) remember(rlo, rhi, rtop, rbot);
}

/* Once a frame: widen for gameplay, keep the console's own 4:3 everywhere else. */
static void follow(void)
{
    g_wide = rt_video_wide && rt_disp_grandparent == SM1_GAMEPLAY_SWAP;
    rt_fov_num = g_wide ? FOV_NUM : 1;
    rt_fov_den = g_wide ? FOV_DEN : 1;
    rt_frame_wide = g_wide;
    roll_elements();
}

/* ---- object bounds tests (WorldFrustum, func_8007B1B4) -------------------------------- */
/* Round half to even, as the PC's Math.Round does. (nxdk's lround is an unimplemented
 * stub that asserts and halts the console.) */
static int16_t round16(double v)
{
    double f = floor(v), d = v - f;
    long n = (long)f;
    if (d > 0.5 || (d == 0.5 && (n & 1))) n++;
    return (int16_t)n;
}

static uint32_t g_saved[5];
static int g_changed;

int hook_WorldFrustum_Enter(CpuContext *c)
{
    (void)c;
    g_changed = rt_fov_num != rt_fov_den;
    if (!g_changed) return 1;
    int16_t m[10];
    for (int i = 0; i < 5; i++) {
        uint32_t word = g_saved[i] = gte_read_control(16 + i);
        m[2 * i] = (int16_t)word;
        m[2 * i + 1] = (int16_t)(word >> 16);
    }
    /* The colour matrix holds the bottom, left and right inward planes. Their sum is
     * the forward component, their difference horizontal: widen only that, then
     * normalise so the native radius and box tests keep their units. */
    double left[3], right[3], ll = 0, rr = 0, ratio = (double)rt_fov_num / rt_fov_den;
    for (int i = 0; i < 3; i++) {
        double forward = (m[3 + i] + m[6 + i]) * 0.5;
        double horizontal = (m[3 + i] - m[6 + i]) * 0.5 * ratio;
        left[i] = forward + horizontal; right[i] = forward - horizontal;
        ll += left[i] * left[i]; rr += right[i] * right[i];
    }
    for (int i = 0; i < 3; i++) {
        m[3 + i] = round16(left[i] * 4096 / sqrt(ll));
        m[6 + i] = round16(right[i] * 4096 / sqrt(rr));
    }
    for (int i = 0; i < 5; i++)
        gte_write_control(16 + i, (uint32_t)(uint16_t)m[2 * i] | ((uint32_t)(uint16_t)m[2 * i + 1] << 16));
    return 1;
}

void hook_WorldFrustum_Exit(CpuContext *c)
{
    (void)c;
    if (!g_changed) return;
    for (int i = 0; i < 5; i++) gte_write_control(16 + i, g_saved[i]);
    g_changed = 0;
}

void wide_sm1_install(void)
{
    rt_frame_hook = follow;
    rt_prim_hook = screen;
}
