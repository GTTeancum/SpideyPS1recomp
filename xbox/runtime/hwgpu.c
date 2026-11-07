#include <stdio.h>
#include <hal/video.h>
#include <pbkit/pbkit.h>
#include <pbkit/nv_regs.h>
#include <xboxkrnl/xboxkrnl.h>
#include <string.h>
#include "gpu.h"
#include "hwgpu.h"
#include "texpack.h"

/* pbkit's pb_push1/pb_push2 are out-of-line calls; state changes are frequent enough
 * that the calls showed in profiles. Same encoding: method header, then the words. */
static inline uint32_t *push1_inline(uint32_t *p, uint32_t cmd, uint32_t a) { p[0] = (1u << 18) | cmd; p[1] = a; return p + 2; }
static inline uint32_t *push2_inline(uint32_t *p, uint32_t cmd, uint32_t a, uint32_t b) { p[0] = (2u << 18) | cmd; p[1] = a; p[2] = b; return p + 3; }
#define pb_push1 push1_inline
#define pb_push2 push2_inline
#include "prof.h"
#include "runtime.h"

/* Display buffers and the screen are the console's output mode (640x480 or 1280x720). */
static int g_out_w = 640, g_out_h = 480;
#define OUT_W g_out_w
#define OUT_H g_out_h
#define PUSHBUFFER_SIZE (4u * 1024u * 1024u)
#define TEXTURE_BASE (1 | (2 << 4) | NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE)
#define MASK(mask, value) (((value) << (__builtin_ffs(mask) - 1)) & (mask))
#define LINEAR_ARGB (TEXTURE_BASE | (NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8 << 8) | (1 << 16))

/* Texel classes in texture alpha, for the alpha test (see ps1ps.ps.cg). */
#define ALPHA_OPAQUE 0x80u
#define ALPHA_SEMI 0xFFu

static int g_active;

/* ---- display buffers -------------------------------------------------------------- */
typedef struct { int x, y, w, h, valid; } Region;
static Region g_regions[HW_REGIONS];
static int g_region_next;

int hw_active(void) { return g_active; }

/* For the status text: the display buffers known and how they were replaced. */
volatile uint32_t rt_hw_region_info[HW_REGIONS], rt_hw_region_swaps;

void hw_note_display(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0) return;
    for (int i = 0; i < HW_REGIONS; i++)
        if (g_regions[i].valid && g_regions[i].x == x && g_regions[i].y == y) {
            if (g_regions[i].w == width && g_regions[i].h == height) return;
            g_regions[i].w = width; g_regions[i].h = height;
            hw_blit(i, x, y, width, height);
            return;
        }
    /* A buffer seen for the first time: what was drawn into it so far is in VRAM. */
    int i = g_region_next;
    g_region_next = (g_region_next + 1) % HW_REGIONS;
    Region *r = &g_regions[i];
    r->x = x; r->y = y; r->w = width; r->h = height; r->valid = 1;
    rt_hw_region_swaps++;
    rt_hw_region_info[i] = (uint32_t)x | (uint32_t)y << 10 | (uint32_t)width << 20;
    hw_blit(i, x, y, width, height);
}

int hw_region_for(int x, int y)
{
    if (!g_active) return -1;
    for (int i = 0; i < HW_REGIONS; i++) {
        const Region *r = &g_regions[i];
        if (r->valid && x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h) return i;
    }
    return -1;
}

/* ---- command buffer ------------------------------------------------------------------- */
static uint32_t *g_open, *g_base;
static unsigned g_open_prims;
/* Where the open block started. A block is limited in size, not only in primitives:
 * the push-buffer wrap check (below) runs when a block opens, so one block must not
 * grow past the slack it leaves. A shadow draw alone can add hundreds of KB; an
 * unbounded block ran off the push buffer's end and the GPU halted on the garbage
 * ("Dma push buffer engine encountered invalid data"). */
static uint32_t *g_open_start;
#define BLOCK_BYTES_MAX (16u * 1024u)
static int block_full(const uint32_t *p) { return (unsigned)(p - g_open_start) * 4u > BLOCK_BYTES_MAX; }
/* Kept free at the buffer's end, and between the CPU's writes and the GPU's reads. */
#define WRAP_SLACK (256u * 1024u)
/* The GPU's read position in the push buffer (physical), as pbkit reads it. */
#define DMA_GET (*(volatile uint32_t *)(VIDEO_BASE + 0x00800000u + 0x44u))   /* NV_USER, channel 0 */
static int g_target = -1;

/* Consecutive triangles with the same state share one BEGIN/END (submit). */
static int g_batch;
static void end_batch(void)
{
    if (g_batch && g_open) g_open = pb_push1(g_open, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
    g_batch = 0;
}

static void close_block(void)
{
    end_batch();
    if (!g_open) return;
    pb_end(g_open);
    g_open = 0;
    g_open_prims = 0;
}

static uint32_t *open_block(void)
{
    end_batch();
    if (g_open && g_open_prims < 128 && !block_full(g_open)) { g_open_prims++; return g_open; }
    close_block();
    uint32_t *p = pb_begin();
    if (!g_base || p < g_base) g_base = p;
    if ((unsigned)(p - g_base) * 4u > PUSHBUFFER_SIZE - WRAP_SLACK) {
        /* Wrap without pbkit's pb_reset, which waits for the GPU to empty the whole
         * buffer: write the jump back to the head and move the put pointer there. The
         * GPU runs on through the old commands, takes the jump and stops at put. */
        *p = 1u + ((uint32_t)g_base & 0x0FFFFFFFu);
        pb_end(g_base);
        g_target = -1;
        p = pb_begin();
    }
    /* Lapping the GPU: commands it has not read yet lie ahead of p, and must not be
     * overwritten. Its read position strictly inside the slack ahead means wait (at
     * p itself it is idle, waiting for these commands). */
    for (;;) {
        uint32_t *get = (uint32_t *)(DMA_GET | 0x80000000u);
        if (get <= p || get >= p + WRAP_SLACK / 4u) break;
        rt_hw_wait = 6;
    }
    rt_hw_wait = 0;
    g_open = p;
    g_open_start = p;
    g_open_prims = 1;
    return p;
}

static void target(int r)
{
    if (g_target == r) return;
    close_block();
    if (r < 0) pb_target_back_buffer(); else pb_target_extra_buffer(r);
    g_target = r;
}

/* ---- textures --------------------------------------------------------------------------- */
/*
 * 4- and 8-bit textures use the chip's own palette lookup: a texture page's colour
 * numbers become one swizzled I8 image, converted once when that VRAM changes, and
 * each CLUT becomes a 256-entry palette. Changing CLUT costs nothing, so a page drawn
 * with many palettes (one thug in several colours) is converted once, not once per
 * palette. 15-bit pages are converted to A8R8G8B8.
 */
#define INDEX_PAGES 64
#define DIRECT_PAGES 16
#define PALETTES 512
#define BLOCK 64                         /* VRAM invalidation granularity, in pixels */
static uint32_t g_block_gen[VRAM_H / BLOCK][VRAM_W / BLOCK];
static uint32_t g_gen = 1;
static uint32_t g_frame;
static int in_flight(uint32_t used) { return used + 3u > g_frame; }
static uint32_t *g_white, *g_blit;
volatile uint32_t rt_hw_texture_decodes, rt_hw_palette_builds, rt_hw_frames, rt_hw_wait, rt_hw_perspective, rt_hw_affine;

/* What a draw samples. scale maps PlayStation texel coordinates to the chip's:
 * 1 for linear images, 1/size for swizzled ones (normalised coordinates). */
typedef struct {
    const void *pixels;
    uint32_t format, palette;            /* palette: NV097_SET_TEXTURE_PALETTE value, 0 none */
    unsigned pitch, w, h;
    float uoff, voff, uscale, vscale;    /* chip coordinate = (PlayStation texel - off) * scale */
    uint32_t filter;                     /* NV097_SET_TEXTURE_FILTER */
    uint32_t address;                    /* NV097_SET_TEXTURE_ADDRESS */
} Tex;

/* Per axis: 1 wrap, 5 clamp to edge. */
#define ADDRESS(wrap_u, wrap_v) (((wrap_u) ? 1u : 5u) | (((wrap_v) ? 1u : 5u) << 8) | (5u << 16))
static uint32_t g_bound_address = ADDRESS(0, 0);

#define FILTER_POINT 0x01012000u
#define FILTER_LINEAR 0x02022000u
int (*rt_texture_hook)(const RtTile *tile, RtReplacement *out);
static uint32_t g_bound_filter = FILTER_POINT;

/* Wait for the GPU to drain; `site` shows in the status text while waiting. */
/* Used by a frame the GPU may not have finished: the current or the two before
 * (pb_finished holds the CPU back once three frames are queued). */
static int in_flight(uint32_t used);

static void drain(uint32_t site)
{
    rt_hw_wait = site;
    while (pb_busy()) {}
    rt_hw_wait = 0;
}

void hw_vram_written(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    g_gen++;
    for (int by = y / BLOCK; by <= (y + h - 1) / BLOCK; by++)
        for (int bx = x / BLOCK; bx <= (x + w - 1) / BLOCK; bx++)
            g_block_gen[by & (VRAM_H / BLOCK - 1)][bx & (VRAM_W / BLOCK - 1)] = g_gen;
}

uint32_t hw_vram_generation(void) { return g_gen; }
uint32_t hw_vram_newest(int x, int y, int w, int h);

static uint32_t newest(int x, int y, int w, int h)
{
    uint32_t n = 0;
    for (int by = y / BLOCK; by <= (y + h - 1) / BLOCK; by++)
        for (int bx = x / BLOCK; bx <= (x + w - 1) / BLOCK; bx++) {
            uint32_t g = g_block_gen[by & (VRAM_H / BLOCK - 1)][bx & (VRAM_W / BLOCK - 1)];
            if (g > n) n = g;
        }
    return n;
}

uint32_t hw_vram_newest(int x, int y, int w, int h) { return w > 0 && h > 0 ? newest(x, y, w, h) : 0; }

static uint32_t argb15(uint16_t p, uint32_t alpha)
{
    uint32_t r = p & 0x1Fu, g = (p >> 5) & 0x1Fu, b = (p >> 10) & 0x1Fu;
    r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2);
    return (alpha << 24) | (r << 16) | (g << 8) | b;
}

static uint32_t texel_argb(uint16_t t)
{
    if (t == 0) return 0;
    return argb15(t, (t & 0x8000) ? ALPHA_SEMI : ALPHA_OPAQUE);
}

/* Swizzled offset of (u, v) in a 256x256 image: the bits of u and v interleaved. */
static uint16_t g_swizzle_u[256], g_swizzle_v[256];
static void swizzle_init(void)
{
    for (unsigned i = 0; i < 256; i++) {
        unsigned su = 0, sv = 0;
        for (unsigned bit = 0; bit < 8; bit++)
            if (i & (1u << bit)) { su |= 1u << (2 * bit); sv |= 1u << (2 * bit + 1); }
        g_swizzle_u[i] = (uint16_t)su; g_swizzle_v[i] = (uint16_t)sv;
    }
}

static void *contiguous(unsigned bytes)
{
    void *p = MmAllocateContiguousMemoryEx(bytes, 0, 0x03ffafff, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (!p) rt_note("hw texture memory exhausted");
    return p;
}

/* -- pages: colour numbers (I8) or 15-bit colour (A8R8G8B8) --
 *
 * A texture window (GP0 E2) repeats a power-of-two tile of the page: the texel used is
 * (u mod T) + base on each windowed axis. Such draws get a texture of just that tile,
 * sampled with wrapping, so one tile serves every window that selects it and a level
 * full of tiled surfaces does not convert (and evict) a 256x256 page per window.
 * A window that is not a power-of-two tile keeps the general path: the whole page
 * converted with the window applied. */
#define TILE_PAGES 256
typedef struct {
    int page_x, page_y, depth, win_mask_x, win_mask_y, win_off_x, win_off_y;
    int base_u, base_v, tw, th;          /* the texels this texture holds */
    uint32_t stamp, used, bytes;
    void *pixels;
    int valid;
} Page;
static Page g_index_pages[INDEX_PAGES], g_direct_pages[DIRECT_PAGES], g_tile_pages[TILE_PAGES];

/* Key of the texture a draw samples: whole page, power-of-two tile, or baked window. */
typedef struct { int page_x, page_y, depth, win_mask_x, win_mask_y, win_off_x, win_off_y, base_u, base_v, tw, th; } PageKey;

/* One axis of a window as a tile: 0 when it is not a power-of-two tile. */
static int window_tile(int mask, int off, int *base, int *size)
{
    if (!mask) { *base = 0; *size = 256; return 1; }
    int t = 256 - mask * 8;
    if (t < 8 || (t & (t - 1))) return 0;
    *base = (off & mask) * 8;
    *size = t;
    return 1;
}

static void page_key(const HwTexture *t, PageKey *k)
{
    k->page_x = t->page_x; k->page_y = t->page_y; k->depth = t->depth;
    if ((t->win_mask_x || t->win_mask_y) &&
        window_tile(t->win_mask_x, t->win_off_x, &k->base_u, &k->tw) &&
        window_tile(t->win_mask_y, t->win_off_y, &k->base_v, &k->th)) {
        k->win_mask_x = k->win_mask_y = k->win_off_x = k->win_off_y = 0;
        return;
    }
    k->win_mask_x = t->win_mask_x; k->win_mask_y = t->win_mask_y; k->win_off_x = t->win_off_x; k->win_off_y = t->win_off_y;
    k->base_u = k->base_v = 0; k->tw = k->th = 256;
}

static int same_page(const Page *e, const PageKey *k)
{
    return e->valid && e->page_x == k->page_x && e->page_y == k->page_y && e->depth == k->depth &&
           e->win_mask_x == k->win_mask_x && e->win_mask_y == k->win_mask_y &&
           e->win_off_x == k->win_off_x && e->win_off_y == k->win_off_y &&
           e->base_u == k->base_u && e->base_v == k->base_v && e->tw == k->tw && e->th == k->th;
}

/* Swizzled offset in a w x h image (powers of two): the bits of u and v interleaved
 * while both have them, then the longer axis's remaining bits. */
static void swizzle_tables(int w, int h, uint32_t *su, uint32_t *sv)
{
    for (int i = 0; i < 256; i++) su[i] = sv[i] = 0;
    for (int axis = 0; axis < 2; axis++)
        for (int i = 0; i < (axis ? h : w); i++) {
            uint32_t out = 0, dest = 1;
            for (int bit = 1; bit < w || bit < h; bit <<= 1) {
                if (bit < w) { if (!axis && (i & bit)) out |= dest; dest <<= 1; }
                if (bit < h) { if (axis && (i & bit)) out |= dest; dest <<= 1; }
            }
            (axis ? sv : su)[i] = out;
        }
}

static void convert_page_timed(Page *e)
{
    static uint32_t su[256], sv[256];
    if (e->tw == 256 && e->th == 256)
        for (int i = 0; i < 256; i++) { su[i] = g_swizzle_u[i]; sv[i] = g_swizzle_v[i]; }
    else swizzle_tables(e->tw, e->th, su, sv);
    for (int v = 0; v < e->th; v++) {
        int tv = (e->base_v + v) & 0xFF;
        tv = (tv & ~(e->win_mask_y * 8)) | ((e->win_off_y & e->win_mask_y) * 8);
        const uint16_t *row = gpu_vram[(e->page_y + (tv & 0xFF)) & (VRAM_H - 1)];
        for (int u = 0; u < e->tw; u++) {
            int tu = (e->base_u + u) & 0xFF;
            tu = ((tu & ~(e->win_mask_x * 8)) | ((e->win_off_x & e->win_mask_x) * 8)) & 0xFF;
            unsigned at = sv[v] | su[u];
            if (e->depth == 2) ((uint32_t *)e->pixels)[at] = texel_argb(row[(e->page_x + tu) & (VRAM_W - 1)]);
            else if (e->depth == 0)
                ((uint8_t *)e->pixels)[at] = (uint8_t)((row[(e->page_x + (tu >> 2)) & (VRAM_W - 1)] >> ((tu & 3) * 4)) & 0xF);
            else
                ((uint8_t *)e->pixels)[at] = (uint8_t)(row[(e->page_x + (tu >> 1)) & (VRAM_W - 1)] >> ((tu & 1) * 8));
        }
    }
    e->stamp = g_gen;
    rt_hw_texture_decodes++;
}

static void convert_page(Page *e)
{
    PROF_BEGIN(t);
    convert_page_timed(e);
    PROF_END(PROF_PAGE, t);
}


/* A slot to fill: a free one, else the least recently used. */
static Page *refill(Page *slots, int count)
{
    Page *best = 0;
    for (int i = 0; i < count; i++) {
        if (!slots[i].valid) return &slots[i];
        if (!best || slots[i].used < best->used) best = &slots[i];
    }
    return best;
}

static Page *page(const HwTexture *t)
{
    PageKey k;
    page_key(t, &k);
    int depth = k.depth, tiled = k.tw != 256 || k.th != 256;
    Page *slots = tiled ? g_tile_pages : depth == 2 ? g_direct_pages : g_index_pages;
    int count = tiled ? TILE_PAGES : depth == 2 ? DIRECT_PAGES : INDEX_PAGES;
    static Page *last[2][3];
    int texels_per_word = depth == 0 ? 4 : depth == 1 ? 2 : 1;
    uint32_t fresh = newest(k.page_x + k.base_u / texels_per_word, k.page_y + k.base_v,
                            (k.tw + texels_per_word - 1) / texels_per_word, k.th);
    Page *hit = last[tiled][depth];
    if (!hit || !same_page(hit, &k)) {
        hit = 0;
        for (int i = 0; i < count; i++) if (same_page(&slots[i], &k)) { hit = &slots[i]; break; }
    }
    if (hit && hit->stamp >= fresh) { hit->used = g_frame; return last[tiled][depth] = hit; }
    /* Stale or missing. Rewrite a stale page in place unless this frame drew from it;
     * a slot this frame drew from may still be in the GPU's queue, so that costs a drain. */
    Page *e = hit && !in_flight(hit->used) ? hit : refill(slots, count);
    if (hit && e != hit) hit->valid = 0;
    if (e->valid && in_flight(e->used)) { close_block(); drain(1); }
    uint32_t bytes = (uint32_t)(k.tw * k.th * (depth == 2 ? 4 : 1));
    if (e->pixels && e->bytes != bytes) { MmFreeContiguousMemory(e->pixels); e->pixels = 0; }
    if (!e->pixels) {
        if (!(e->pixels = contiguous(bytes))) { e->valid = 0; return 0; }
        e->bytes = bytes;
    }
    e->page_x = k.page_x; e->page_y = k.page_y; e->depth = depth;
    e->win_mask_x = k.win_mask_x; e->win_mask_y = k.win_mask_y; e->win_off_x = k.win_off_x; e->win_off_y = k.win_off_y;
    e->base_u = k.base_u; e->base_v = k.base_v; e->tw = k.tw; e->th = k.th;
    RT_WHERE("hw convert page");
    convert_page(e);
    e->valid = 1;
    e->used = g_frame;
    return last[tiled][depth] = e;
}

/* -- palettes: one per (CLUT, depth), found through a direct table -- */
typedef struct { uint32_t stamp, used; uint32_t *colors; uint16_t clut; uint8_t depth, valid; } Palette;
static Palette g_palettes[PALETTES];
static uint16_t g_palette_of[2][0x10000];    /* slot + 1, 0 none */
static int g_palette_next;

static Palette *palette(int clut, int depth)
{
    int cx = (clut & 0x3F) * 16, cy = (clut >> 6) & 0x1FF, n = depth == 0 ? 16 : 256;
    uint32_t fresh = newest(cx, cy, n, 1);
    uint16_t *index = &g_palette_of[depth][clut & 0xFFFF];
    Palette *hit = *index ? &g_palettes[*index - 1] : 0;
    if (hit && hit->stamp >= fresh) { hit->used = g_frame; return hit; }
    Palette *e = hit && !in_flight(hit->used) ? hit : 0;
    if (!e) {
        /* Round robin, skipping palettes this frame still needs. */
        for (int tries = 0; tries < PALETTES && !e; tries++) {
            Palette *c = &g_palettes[g_palette_next];
            g_palette_next = (g_palette_next + 1) % PALETTES;
            if (!c->valid || !in_flight(c->used)) e = c;
        }
        if (!e) { close_block(); drain(5); e = &g_palettes[g_palette_next]; }
        if (e->valid) g_palette_of[e->depth][e->clut] = 0;
        if (hit) hit->valid = 0;
    }
    if (!e->colors && !(e->colors = contiguous(256u * 4u))) return 0;
    const uint16_t *row = gpu_vram[cy & (VRAM_H - 1)];
    for (int i = 0; i < 256; i++) e->colors[i] = i < n ? texel_argb(row[(cx + i) & (VRAM_W - 1)]) : 0;
    e->clut = (uint16_t)clut; e->depth = (uint8_t)depth; e->valid = 1;
    e->stamp = g_gen; e->used = g_frame;
    *index = (uint16_t)(e - g_palettes + 1);
    rt_hw_palette_builds++;
    RT_WHERE("hw palette built");
    return e;
}

static uint32_t log2u(unsigned v) { uint32_t n = 0; while ((1u << n) < v) n++; return n; }
#define SWIZZLED(color, w, h) (TEXTURE_BASE | ((color) << 8) | (1 << 16) | (log2u(w) << 20) | (log2u(h) << 24))

/* The texture a primitive samples; 0 when it cannot be had. */
static int texture_timed(const HwTexture *key, Tex *out)
{
    Page *pg = page(key);
    if (!pg) return 0;
    out->pixels = pg->pixels;
    out->pitch = 0; out->w = (unsigned)pg->tw; out->h = (unsigned)pg->th;
    /* A tile wraps: chip coordinate u / T repeats every T texels, as the window does. */
    out->uoff = out->voff = 0;
    out->uscale = 1.0f / (float)pg->tw; out->vscale = 1.0f / (float)pg->th;
    out->filter = FILTER_POINT;
    out->address = ADDRESS(pg->tw != 256, pg->th != 256);
    if (key->depth == 2) {
        out->format = SWIZZLED(NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8, pg->tw, pg->th);
        out->palette = 0;
        return 1;
    }
    Palette *pal = palette(key->clut, key->depth);
    if (!pal) return 0;
    out->format = SWIZZLED(NV097_SET_TEXTURE_FORMAT_COLOR_SZ_I8_A8R8G8B8, pg->tw, pg->th);
    out->palette = ((uint32_t)pal->colors & 0x03FFFFC0u) | NV097_SET_TEXTURE_PALETTE_CONTEXT_DMA |
                   (NV097_SET_TEXTURE_PALETTE_LENGTH_256 << 2);
    return 1;
}

static int texture(const HwTexture *key, Tex *out)
{
    PROF_BEGIN(t);
    int r = texture_timed(key, out);
    PROF_END(PROF_TEXTURE, t);
    return r;
}


/* A replacement texture for these texels, if the game's rules have one. Replacement
 * art samples bilinear, and its cutouts (DXT1 alpha) are drawn without the
 * PlayStation's per-texel semi-transparency, which the art does not carry. */
volatile uint32_t rt_hw_replaced;
static int replacement_timed(const HwTexture *key, int u0, int v0, int w, int h, Tex *out)
{
    /* Under a texture window the texels sampled are the window's tile, as the PC's
     * TextureResolver has it (twOr, window extent): match on that tile, and repeat
     * the art with it. */
    int base_u = 0, size_u = 256, base_v = 0, size_v = 256;
    int win_u = key->win_mask_x && window_tile(key->win_mask_x, key->win_off_x, &base_u, &size_u);
    int win_v = key->win_mask_y && window_tile(key->win_mask_y, key->win_off_y, &base_v, &size_v);
    if (win_u) { u0 = base_u; w = size_u; }
    if (win_v) { v0 = base_v; h = size_v; }
    RtTile tile = { key->page_x, key->page_y, key->depth, (key->clut & 0x3F) * 16, (key->clut >> 6) & 0x1FF, u0, v0, w, h };
    RtReplacement rep;
    /* The game's rules (a suit) first, then texture packs (Dreamcast characters). */
    if (!(rt_texture_hook && rt_texture_hook(&tile, &rep)) && !texpack_lookup(&tile, &rep)) return 0;
    unsigned logw = 0, logh = 0;
    while ((1 << logw) < rep.width) logw++;
    while ((1 << logh) < rep.height) logh++;
    out->pixels = rep.dxt1;
    out->format = TEXTURE_BASE | (NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5 << 8) | (1 << 16) | (logw << 20) | (logh << 24);
    out->palette = 0;
    out->pitch = 0; out->w = (unsigned)rep.width; out->h = (unsigned)rep.height;
    /* A windowed axis: the primitive's coordinate wraps every tile, so the art wraps
     * too when it covers exactly the tile (chip coordinate (u - (u0 - base)) / w). */
    int wrap_u = win_u && rep.w == size_u, wrap_v = win_v && rep.h == size_v;
    out->uoff = (float)(wrap_u ? rep.u0 - base_u : rep.u0); out->voff = (float)(wrap_v ? rep.v0 - base_v : rep.v0);
    out->uscale = 1.0f / (float)rep.w; out->vscale = 1.0f / (float)rep.h;
    out->filter = FILTER_LINEAR;
    out->address = ADDRESS(wrap_u, wrap_v);
    rt_hw_replaced++;
    return 1;
}

static int replacement(const HwTexture *key, int u0, int v0, int w, int h, Tex *out)
{
    PROF_BEGIN(t);
    int r = replacement_timed(key, u0, v0, w, h, out);
    PROF_END(PROF_REPLACE, t);
    return r;
}


static Tex linear(const void *pixels, unsigned pitch, unsigned w, unsigned h)
{
    Tex t = { pixels, LINEAR_ARGB, 0, pitch, w, h, 0, 0, 1.0f, 1.0f, FILTER_POINT, ADDRESS(0, 0) };
    return t;
}

/* ---- render state ------------------------------------------------------------------------ */
static const void *g_bound;
static uint32_t g_bound_palette;
static int g_blend = -2, g_alpha = -1;

/* Stencil: used by the shadow only (bit 0x01). */
#define STENCIL_SHADOW 4         /* darken each pixel once */
#define STENCIL_SHADOW_CLEAR 5   /* clear the shadow's bit again, colour writes off */
static int g_stencil = -1, g_stencil_force = -1;

static uint32_t *stencil(uint32_t *p, int state)
{
    if (g_stencil == state) return p;
    g_stencil = state;
    if (state < 0) return pb_push1(p, NV097_SET_STENCIL_TEST_ENABLE, 0);
    int shadow = state >= STENCIL_SHADOW;
    p = pb_push1(p, NV097_SET_STENCIL_TEST_ENABLE, 1);
    p = pb_push1(p, NV097_SET_STENCIL_MASK, shadow ? 0x01 : 0x80);
    p = pb_push1(p, NV097_SET_STENCIL_FUNC_MASK, shadow ? 0x01 : 0x80);
    p = pb_push1(p, NV097_SET_STENCIL_OP_FAIL, NV097_SET_STENCIL_OP_V_KEEP);
    p = pb_push1(p, NV097_SET_STENCIL_OP_ZFAIL, NV097_SET_STENCIL_OP_V_KEEP);
    if (state == STENCIL_SHADOW) {
        p = pb_push1(p, NV097_SET_STENCIL_FUNC, NV097_SET_ALPHA_FUNC_V_EQUAL);
        p = pb_push1(p, NV097_SET_STENCIL_FUNC_REF, 0);
        p = pb_push1(p, NV097_SET_STENCIL_OP_ZPASS, NV097_SET_STENCIL_OP_V_INCRSAT);
    } else if (state == STENCIL_SHADOW_CLEAR) {
        p = pb_push1(p, NV097_SET_STENCIL_FUNC, NV097_SET_ALPHA_FUNC_V_ALWAYS);
        p = pb_push1(p, NV097_SET_STENCIL_FUNC_REF, 0);
        p = pb_push1(p, NV097_SET_STENCIL_OP_ZPASS, NV097_SET_STENCIL_OP_V_ZERO);
    } else {
        /* check: pass where the mask bit is clear; written pixels get set's value. */
        p = pb_push1(p, NV097_SET_STENCIL_FUNC, (state & 2) ? NV097_SET_ALPHA_FUNC_V_NOTEQUAL : NV097_SET_ALPHA_FUNC_V_ALWAYS);
        p = pb_push1(p, NV097_SET_STENCIL_FUNC_REF, 0x80);
        p = pb_push1(p, NV097_SET_STENCIL_OP_ZPASS, (state & 1) ? NV097_SET_STENCIL_OP_V_REPLACE : NV097_SET_STENCIL_OP_V_ZERO);
    }
    return p;
}

static uint32_t *bind(uint32_t *p, const Tex *t)
{
    if (g_bound != t->pixels) {
        g_bound = t->pixels;
        p = pb_push2(p, NV097_SET_TEXTURE_OFFSET, (uint32_t)t->pixels & 0x03ffffff, t->format);
        if (t->pitch) {
            p = pb_push1(p, NV097_SET_TEXTURE_CONTROL1, t->pitch << 16);
            p = pb_push1(p, NV097_SET_TEXTURE_IMAGE_RECT, (t->w << 16) | t->h);
        }
    }
    if (g_bound_address != t->address) {
        g_bound_address = t->address;
        p = pb_push1(p, NV097_SET_TEXTURE_ADDRESS, t->address);
    }
    if (g_bound_filter != t->filter) {
        g_bound_filter = t->filter;
        p = pb_push1(p, NV097_SET_TEXTURE_FILTER, t->filter);
    }
    if (t->palette && g_bound_palette != t->palette) {
        g_bound_palette = t->palette;
        p = pb_push1(p, NV097_SET_TEXTURE_PALETTE, t->palette);
    }
    return p;
}

/* blend: -1 off, 0..3 the PlayStation modes. */
static uint32_t *blend(uint32_t *p, int mode)
{
    if (g_blend == mode) return p;
    g_blend = mode;
    p = pb_push1(p, NV097_SET_BLEND_ENABLE, mode >= 0);
    if (mode < 0) return p;
    uint32_t src = NV097_SET_BLEND_FUNC_SFACTOR_V_ONE, dst = NV097_SET_BLEND_FUNC_DFACTOR_V_ONE;
    uint32_t eq = NV097_SET_BLEND_EQUATION_V_FUNC_ADD, color = 0;
    switch (mode) {
    case 0: src = NV097_SET_BLEND_FUNC_SFACTOR_V_CONSTANT_COLOR; dst = NV097_SET_BLEND_FUNC_DFACTOR_V_CONSTANT_COLOR; color = 0x80808080u; break;
    case 2: eq = NV097_SET_BLEND_EQUATION_V_FUNC_REVERSE_SUBTRACT; break;
    case 3: src = NV097_SET_BLEND_FUNC_SFACTOR_V_CONSTANT_COLOR; color = 0x40404040u; break;
    }
    p = pb_push1(p, NV097_SET_BLEND_FUNC_SFACTOR, src);
    p = pb_push1(p, NV097_SET_BLEND_FUNC_DFACTOR, dst);
    p = pb_push1(p, NV097_SET_BLEND_EQUATION, eq);
    p = pb_push1(p, NV097_SET_BLEND_COLOR, color);
    return p;
}

/* alpha: -1 off, otherwise the texel class to keep (0 = any non-transparent). */
static uint32_t *alpha(uint32_t *p, int keep)
{
    if (g_alpha == keep) return p;
    g_alpha = keep;
    p = pb_push1(p, NV097_SET_ALPHA_TEST_ENABLE, keep >= 0);
    if (keep < 0) return p;
    p = pb_push1(p, NV097_SET_ALPHA_FUNC, keep == 0 ? NV097_SET_ALPHA_FUNC_V_GREATER : NV097_SET_ALPHA_FUNC_V_EQUAL);
    p = pb_push1(p, NV097_SET_ALPHA_REF, (uint32_t)keep);
    return p;
}

/* ---- projected depths ------------------------------------------------------------------ */
#define DEPTH_SLOTS 16384
#define DEPTH_PROBES 8
typedef struct { uint32_t key; uint16_t z, gen; } DepthSlot;
static DepthSlot g_depth[DEPTH_SLOTS];
static uint16_t g_depth_gen = 2;

static uint32_t depth_key(int sx, int sy) { return 0x80000000u | ((uint32_t)(sx & 0x7FF) << 11) | (uint32_t)(sy & 0x7FF); }
static unsigned depth_hash(uint32_t key) { return (key * 2654435761u) >> 18; }
static int depth_live(const DepthSlot *s) { return (uint16_t)(g_depth_gen - s->gen) <= 1; }

/* sz 0 records "no depth here" this frame, so an older point at the same pixel is
 * not taken for this one. */
void hw_note_depth(int sx, int sy, uint16_t sz)
{
    uint32_t key = depth_key(sx, sy);
    unsigned h = depth_hash(key);
    DepthSlot *free_slot = 0;
    for (int i = 0; i < DEPTH_PROBES; i++) {
        DepthSlot *s = &g_depth[(h + i) & (DEPTH_SLOTS - 1)];
        if (s->key == key || !depth_live(s)) {
            if (s->key == key || !free_slot) { free_slot = s; if (s->key == key) break; }
        }
    }
    if (!free_slot) free_slot = &g_depth[h];
    free_slot->key = key; free_slot->z = sz; free_slot->gen = g_depth_gen;
}

static float depth_exact(int sx, int sy)
{
    uint32_t key = depth_key(sx, sy);
    unsigned h = depth_hash(key);
    for (int i = 0; i < DEPTH_PROBES; i++) {
        const DepthSlot *s = &g_depth[(h + i) & (DEPTH_SLOTS - 1)];
        if (s->key == key && depth_live(s)) return (float)s->z;
    }
    return 0.0f;
}

static float hw_depth_at_timed(int sx, int sy) { return depth_exact(sx, sy); }

void hw_depth_frame(void) { g_depth_gen++; }

/* ---- primitives -------------------------------------------------------------------------- */
static float sx(int r, float x) { return (x - (float)g_regions[r].x) * (float)OUT_W / (float)g_regions[r].w; }
static float sy(int r, float y) { return (y - (float)g_regions[r].y) * (float)OUT_H / (float)g_regions[r].h; }

/* w > 0 makes the chip interpolate colour and texture coordinates in perspective. */

/* Written inline (pbkit's pb_push4f is an out-of-line call per attribute, a large
 * share of frame time with thousands of vertices): a method header, then four floats. */
#define METHOD4(cmd) ((4u << 18) | (cmd))
static inline uint32_t *push4f(uint32_t *p, uint32_t cmd, float a, float b, float c, float d)
{
    float *f = (float *)(p + 1);
    p[0] = METHOD4(cmd);
    f[0] = a; f[1] = b; f[2] = c; f[3] = d;
    return p + 5;
}

static inline uint32_t *vertex(uint32_t *p, float x, float y, float r, float g, float b, float u, float v, float w)
{
    p = push4f(p, NV097_SET_VERTEX_DATA4F_M + 3 * 16, r, g, b, 1.0f);
    p = push4f(p, NV097_SET_VERTEX_DATA4F_M + 9 * 16, u, v, 0.0f, 1.0f);
    p = push4f(p, NV097_SET_VERTEX_DATA4F_M, x, y, 0.0f, w);
    return p;
}

float hw_depth_at(int sx, int sy)
{
    PROF_BEGIN(t);
    float r = hw_depth_at_timed(sx, sy);
    PROF_END(PROF_DEPTH, t);
    return r;
}


/* Draw `count` vertices once per required pass: opaque texels without blending,
 * then semi-transparent texels with the primitive's blend mode. */
static void submit(int r, uint32_t op, const float (*xy)[2], const float (*rgb)[3], const float (*uv)[2], const float *w, int count,
                   const Tex *tex, int textured, int semi, int mode)
{
    PROF_BEGIN(sub);
    target(r);
    int passes = textured && semi ? 2 : 1;
    for (int pass = 0; pass < passes; pass++) {
        int keep, how;
        if (!textured) { keep = -1; how = semi ? mode : -1; }
        else if (!semi) { keep = 0; how = -1; }
        else if (pass == 0) { keep = ALPHA_OPAQUE; how = -1; }
        else { keep = ALPHA_SEMI; how = mode; }
        uint32_t *p;
        int mask = g_stencil_force;
        int same = g_bound == tex->pixels && (!tex->palette || g_bound_palette == tex->palette) &&
                   g_bound_filter == tex->filter && g_bound_address == tex->address && g_alpha == keep && g_blend == how &&
                   g_stencil == mask;
        if (op == NV097_SET_BEGIN_END_OP_TRIANGLES && g_batch && g_open && g_open_prims < 128 && !block_full(g_open) && same) {
            p = g_open;
            g_open_prims++;
        } else {
            p = open_block();
            p = bind(p, tex);
            p = alpha(p, keep);
            p = blend(p, how);
            p = stencil(p, mask);
            p = pb_push1(p, NV097_SET_BEGIN_END, op);
        }
        for (int i = 0; i < count; i++)
            p = vertex(p, xy[i][0], xy[i][1], rgb[i][0], rgb[i][1], rgb[i][2],
                       (uv[i][0] - tex->uoff) * tex->uscale, (uv[i][1] - tex->voff) * tex->vscale, w ? w[i] : 1.0f);
        if (op == NV097_SET_BEGIN_END_OP_TRIANGLES) g_batch = 1;
        else p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
        g_open = p;
    }
    PROF_END(PROF_SUBMIT, sub);
}

/* Vertex colour as the shader expects it: modulating colour /255 (128 -> x2 = 1),
 * raw textures 0.5, untextured colour /510 so x2 gives the colour itself. */
static void shade(float out[3], uint8_t r, uint8_t g, uint8_t b, int textured, int raw)
{
    float k = !textured ? 1.0f / 510.0f : 1.0f / 255.0f;
    if (textured && raw) { out[0] = out[1] = out[2] = 0.5f; return; }
    out[0] = r * k; out[1] = g * k; out[2] = b * k;
}

/*
 * PlayStation geometry may reach far off screen (the GTE gives +-1023, the backdrop and
 * ground use it), and at 720p those corners exceed the range the NV2A rasterises: it
 * dropped whole triangles, leaving black wedges where the sky and ground should be. A
 * triangle with a corner outside a guard band is clipped to it here first. Attributes
 * are interpolated as the chip does, in perspective (by 1/w), so the cut is invisible.
 */
#define GUARD 512.0f
static int g_area[4] = { 0, 0, VRAM_W - 1, VRAM_H - 1 };
void hw_area(int left, int top, int right, int bottom) { g_area[0] = left; g_area[1] = top; g_area[2] = right; g_area[3] = bottom; }
static float g_edge[4];   /* clip rectangle, output pixels: left, right, top, bottom */

static void clip_edges(int r)
{
    g_edge[0] = sx(r, (float)g_area[0]); g_edge[1] = sx(r, (float)(g_area[2] + 1));
    g_edge[2] = sy(r, (float)g_area[1]); g_edge[3] = sy(r, (float)(g_area[3] + 1));
    if (g_edge[0] < -GUARD) g_edge[0] = -GUARD;
    if (g_edge[1] > (float)OUT_W + GUARD) g_edge[1] = (float)OUT_W + GUARD;
    if (g_edge[2] < -GUARD) g_edge[2] = -GUARD;
    if (g_edge[3] > (float)OUT_H + GUARD) g_edge[3] = (float)OUT_H + GUARD;
}
typedef struct { float x, y, r, g, b, u, v, q; } ClipVertex;   /* q = 1/w; colour, uv premultiplied by q */

static int inside(const ClipVertex *c, int edge)
{
    switch (edge) {
    case 0: return c->x >= g_edge[0];
    case 1: return c->x <= g_edge[1];
    case 2: return c->y >= g_edge[2];
    default: return c->y <= g_edge[3];
    }
}

static ClipVertex crossing(const ClipVertex *a, const ClipVertex *b, int edge)
{
    float t;
    switch (edge) {
    case 0: t = (g_edge[0] - a->x) / (b->x - a->x); break;
    case 1: t = (g_edge[1] - a->x) / (b->x - a->x); break;
    case 2: t = (g_edge[2] - a->y) / (b->y - a->y); break;
    default: t = (g_edge[3] - a->y) / (b->y - a->y); break;
    }
    ClipVertex c;
    c.x = a->x + (b->x - a->x) * t; c.y = a->y + (b->y - a->y) * t;
    c.r = a->r + (b->r - a->r) * t; c.g = a->g + (b->g - a->g) * t; c.b = a->b + (b->b - a->b) * t;
    c.u = a->u + (b->u - a->u) * t; c.v = a->v + (b->v - a->v) * t; c.q = a->q + (b->q - a->q) * t;
    return c;
}

static void submit_tri(int r, const float (*xy)[2], const float (*rgb)[3], const float (*uv)[2], const float *w,
                       const Tex *tex, int textured, int semi, int mode)
{
    clip_edges(r);
    int out = 0;
    for (int i = 0; i < 3; i++)
        if (xy[i][0] < g_edge[0] || xy[i][0] > g_edge[1] || xy[i][1] < g_edge[2] || xy[i][1] > g_edge[3]) out = 1;
    if (!out) { submit(r, NV097_SET_BEGIN_END_OP_TRIANGLES, xy, rgb, uv, w, 3, tex, textured, semi, mode); return; }
    ClipVertex poly[2][12];
    int n = 3, cur = 0;
    for (int i = 0; i < 3; i++) {
        float q = 1.0f / (w ? w[i] : 1.0f);
        ClipVertex c = { xy[i][0], xy[i][1], rgb[i][0] * q, rgb[i][1] * q, rgb[i][2] * q, uv[i][0] * q, uv[i][1] * q, q };
        poly[0][i] = c;
    }
    for (int edge = 0; edge < 4 && n; edge++) {
        const ClipVertex *in = poly[cur];
        ClipVertex *o = poly[cur ^ 1];
        int m = 0;
        for (int i = 0; i < n && m < 10; i++) {
            const ClipVertex *a = &in[i], *b = &in[(i + 1) % n];
            int ia = inside(a, edge), ib = inside(b, edge);
            if (ia) o[m++] = *a;
            if (ia != ib) o[m++] = crossing(a, b, edge);
        }
        n = m;
        cur ^= 1;
    }
    for (int i = 1; i + 1 < n; i++) {
        const ClipVertex *c[3] = { &poly[cur][0], &poly[cur][i], &poly[cur][i + 1] };
        float txy[3][2], trgb[3][3], tuv[3][2], tw[3];
        for (int k = 0; k < 3; k++) {
            float wk = 1.0f / c[k]->q;
            txy[k][0] = c[k]->x; txy[k][1] = c[k]->y;
            trgb[k][0] = c[k]->r * wk; trgb[k][1] = c[k]->g * wk; trgb[k][2] = c[k]->b * wk;
            tuv[k][0] = c[k]->u * wk; tuv[k][1] = c[k]->v * wk;
            tw[k] = wk;
        }
        submit(r, NV097_SET_BEGIN_END_OP_TRIANGLES, (const float (*)[2])txy, (const float (*)[3])trgb, (const float (*)[2])tuv,
               w ? tw : 0, 3, tex, textured, semi, mode);
    }
}

void hw_triangle(int r, const HwVertex v[3], const HwTexture *tex, int semi, int mode, int raw, int gouraud)
{
    float xy[3][2], rgb[3][3], uv[3][2], w[3];
    /* Perspective only when every corner came from the GTE; a mix stays affine. */
#ifdef RT_AFFINE
    int depth = 0;
#else
    int depth = v[0].w > 0 && v[1].w > 0 && v[2].w > 0;
    /* With vertex tags each depth is the corner's own; the guard below was for depths
     * looked up by screen position (precise_vertices=0). */
    if (depth && !rt_precise_vertices) {
        /* A corner with a depth from the wrong point (two points can share a pixel) shows
         * as a fan of streaks. A small triangle cannot span a large depth range, so one
         * that seems to is drawn affine instead. */
        float zmin = v[0].w, zmax = v[0].w, xmin = v[0].x, xmax = v[0].x, ymin = v[0].y, ymax = v[0].y;
        for (int i = 1; i < 3; i++) {
            if (v[i].w < zmin) zmin = v[i].w;
            if (v[i].w > zmax) zmax = v[i].w;
            if (v[i].x < xmin) xmin = v[i].x;
            if (v[i].x > xmax) xmax = v[i].x;
            if (v[i].y < ymin) ymin = v[i].y;
            if (v[i].y > ymax) ymax = v[i].y;
        }
        if (zmax > zmin * 2.0f && xmax - xmin < 96 && ymax - ymin < 96) { depth = 0; }
    }
#endif
    if (tex) { if (depth) rt_hw_perspective++; else rt_hw_affine++; }
    for (int i = 0; i < 3; i++) {
        w[i] = depth ? v[i].w : 1.0f;
        const HwVertex *s = gouraud ? &v[i] : &v[0];
        xy[i][0] = sx(r, v[i].x); xy[i][1] = sy(r, v[i].y);
        shade(rgb[i], s->r, s->g, s->b, tex != 0, raw);
        uv[i][0] = v[i].u; uv[i][1] = v[i].v;
    }
    if (depth && !semi && rt_precise_vertices) {
        /* Hairline cracks: where the game's pieces meet only to within a fraction of a
         * pixel (a split face beside an unsplit one, a vertex on a neighbour's edge),
         * pixel centres can fall between them and show what was there before: a speck
         * or short seam that moves with the camera. The PC fills such pixels after
         * drawing (GlShaders WideCompleteFs); without a spare buffer for that here,
         * each opaque world triangle is drawn 0.6 pixels larger about its centre so
         * neighbours overlap. Texture coordinates stay with the corners (sub-pixel
         * stretch); semi-transparent ones are left alone (an overlap would show). */
        float cx = (xy[0][0] + xy[1][0] + xy[2][0]) / 3.0f, cy = (xy[0][1] + xy[1][1] + xy[2][1]) / 3.0f;
        for (int i = 0; i < 3; i++) {
            float dx = xy[i][0] - cx, dy = xy[i][1] - cy, len = __builtin_sqrtf(dx * dx + dy * dy);
            if (len > 0.01f) { xy[i][0] += dx * (0.6f / len); xy[i][1] += dy * (0.6f / len); }
        }
    }
    if (rt_geometry_debug == 1 && tex && depth) {
        /* Gap test (geometry_debug=1): each material one flat colour, so a crack shows
         * as a thin line of another colour inside it, countable exactly. */
        uint32_t hsh = ((uint32_t)tex->clut * 2654435761u) ^ ((uint32_t)(tex->page_x | tex->page_y << 10) * 40503u);
        float cr = (float)((hsh >> 8) & 0xFF) / 510.0f, cg = (float)((hsh >> 16) & 0xFF) / 510.0f, cb = (float)((hsh >> 24) & 0xFF) / 510.0f;
        for (int i = 0; i < 3; i++) { rgb[i][0] = cr; rgb[i][1] = cg; rgb[i][2] = cb; }
        Tex t = linear(g_white, 64, 16, 16);
        submit_tri(r, xy, rgb, uv, w, &t, 0, 0, mode);
        return;
    }
    if (tex) {
        /* Which texel a pixel takes, as the PC's renderer (GlShaders: floor where the
         * texture runs forward across the screen, ceil where it runs backward) and the
         * console do: with floor alone, mirrored art (window columns, brick) showed the
         * neighbouring texel in a thin line along polygon edges. The direction is
         * constant across a triangle, affine or perspective. */
        float ax = xy[1][0] - xy[0][0], ay = xy[1][1] - xy[0][1], bx = xy[2][0] - xy[0][0], by = xy[2][1] - xy[0][1];
        float area = ax * by - bx * ay;
        if (area > 1e-6f || area < -1e-6f) {
            float dudx = ((uv[1][0] - uv[0][0]) * by - (uv[2][0] - uv[0][0]) * ay) / area;
            float dvdy = (ax * (uv[2][1] - uv[0][1]) - bx * (uv[1][1] - uv[0][1])) / area;
            float du = dudx < 0 ? 0.999f : 0.0001f, dv = dvdy < 0 ? 0.999f : 0.0001f;
            for (int i = 0; i < 3; i++) { uv[i][0] += du; uv[i][1] += dv; }
        }
        Tex t;
        int u_lo = v[0].u, u_hi = v[0].u, v_lo = v[0].v, v_hi = v[0].v;
        for (int i = 1; i < 3; i++) {
            if (v[i].u < u_lo) u_lo = v[i].u;
            if (v[i].u > u_hi) u_hi = v[i].u;
            if (v[i].v < v_lo) v_lo = v[i].v;
            if (v[i].v > v_hi) v_hi = v[i].v;
        }
        if (replacement(tex, u_lo, v_lo, u_hi - u_lo + 1, v_hi - v_lo + 1, &t)) {
            submit_tri(r, xy, rgb, uv, w, &t, 1, 0, mode);
            return;
        }
        if (!texture(tex, &t)) return;
        submit_tri(r, xy, rgb, uv, w, &t, 1, semi, mode);
    } else {
        Tex t = linear(g_white, 64, 16, 16);
        submit_tri(r, xy, rgb, uv, w, &t, 0, semi, mode);
    }
}

void hw_rect(int r, int x, int y, int w, int h, uint8_t u0, uint8_t v0, int tw, int th, uint8_t cr, uint8_t cg, uint8_t cb,
             const HwTexture *tex, int semi, int mode, int raw)
{
    float x0 = sx(r, (float)x), x1 = sx(r, (float)(x + w)), y0 = sy(r, (float)y), y1 = sy(r, (float)(y + h));
    /* Trimmed to the drawing area, texture coordinates with it. */
    clip_edges(r);
    float fu0 = u0, fv0 = v0, fu1 = (float)(u0 + tw), fv1 = (float)(v0 + th);
    if (x0 < g_edge[0]) { fu0 += (fu1 - fu0) * (g_edge[0] - x0) / (x1 - x0); x0 = g_edge[0]; }
    if (x1 > g_edge[1]) { fu1 -= (fu1 - fu0) * (x1 - g_edge[1]) / (x1 - x0); x1 = g_edge[1]; }
    if (y0 < g_edge[2]) { fv0 += (fv1 - fv0) * (g_edge[2] - y0) / (y1 - y0); y0 = g_edge[2]; }
    if (y1 > g_edge[3]) { fv1 -= (fv1 - fv0) * (y1 - g_edge[3]) / (y1 - y0); y1 = g_edge[3]; }
    if (x1 <= x0 || y1 <= y0) return;
    float xy[4][2] = { { x0, y0 }, { x1, y0 }, { x0, y1 }, { x1, y1 } };
    float c[3];
    shade(c, cr, cg, cb, tex != 0, raw);
    float rgb[4][3] = { { c[0], c[1], c[2] }, { c[0], c[1], c[2] }, { c[0], c[1], c[2] }, { c[0], c[1], c[2] } };
    float uv[4][2] = { { fu0, fv0 }, { fu1, fv0 }, { fu0, fv1 }, { fu1, fv1 } };
    if (tex) {
        Tex t;
        if (replacement(tex, u0, v0, tw, th, &t)) {
            submit(r, NV097_SET_BEGIN_END_OP_TRIANGLE_STRIP, xy, rgb, uv, 0, 4, &t, 1, 0, mode);
            return;
        }
        if (!texture(tex, &t)) return;
        submit(r, NV097_SET_BEGIN_END_OP_TRIANGLE_STRIP, xy, rgb, uv, 0, 4, &t, 1, semi, mode);
    } else {
        Tex t = linear(g_white, 64, 16, 16);
        submit(r, NV097_SET_BEGIN_END_OP_TRIANGLE_STRIP, xy, rgb, uv, 0, 4, &t, 0, semi, mode);
    }
}

void hw_line(int r, const HwVertex v[2], int semi, int mode)
{
    float xy[2][2], rgb[2][3], uv[2][2] = { { 0, 0 }, { 0, 0 } };
    for (int i = 0; i < 2; i++) {
        xy[i][0] = sx(r, v[i].x); xy[i][1] = sy(r, v[i].y);
        shade(rgb[i], v[i].r, v[i].g, v[i].b, 0, 0);
    }
    Tex t = linear(g_white, 64, 16, 16);
    submit(r, NV097_SET_BEGIN_END_OP_LINES, xy, rgb, uv, 0, 2, &t, 0, semi, mode);
}

void hw_fill(int r, int x, int y, int w, int h, uint16_t color15)
{
    target(r);
    close_block();
    int x0 = (int)sx(r, (float)x), y0 = (int)sy(r, (float)y);
    int x1 = (int)sx(r, (float)(x + w)), y1 = (int)sy(r, (float)(y + h));
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > OUT_W) x1 = OUT_W; if (y1 > OUT_H) y1 = OUT_H;
    if (x1 > x0 && y1 > y0) pb_fill(x0, y0, x1 - x0, y1 - y0, argb15(color15, 0xFF));
}

void hw_blit(int r, int x, int y, int w, int h)
{
    const Region *g = &g_regions[r];
    int x0 = x > g->x ? x : g->x, y0 = y > g->y ? y : g->y;
    int x1 = x + w < g->x + g->w ? x + w : g->x + g->w, y1 = y + h < g->y + g->h ? y + h : g->y + g->h;
    if (x1 <= x0 || y1 <= y0) return;
    RT_WHERE("hw blit");
    close_block();
    drain(2);
    int bw = x1 - x0, bh = y1 - y0;
    for (int row = 0; row < bh; row++) {
        const uint16_t *src = &gpu_vram[(y0 + row) & (VRAM_H - 1)][0];
        uint32_t *out = g_blit + row * VRAM_W;
        for (int col = 0; col < bw; col++) out[col] = argb15(src[(x0 + col) & (VRAM_W - 1)], 0xFF);
    }
    g_bound = 0;
    float X0 = sx(r, (float)x0), X1 = sx(r, (float)x1), Y0 = sy(r, (float)y0), Y1 = sy(r, (float)y1);
    float xy[4][2] = { { X0, Y0 }, { X1, Y0 }, { X0, Y1 }, { X1, Y1 } };
    float rgb[4][3] = { { .5f, .5f, .5f }, { .5f, .5f, .5f }, { .5f, .5f, .5f }, { .5f, .5f, .5f } };
    float uv[4][2] = { { 0, 0 }, { (float)bw, 0 }, { 0, (float)bh }, { (float)bw, (float)bh } };
    target(r);
    uint32_t *p = open_block();
    Tex t = linear(g_blit, VRAM_W * 4, (unsigned)bw, (unsigned)bh);
    p = bind(p, &t);
    p = stencil(p, -1);
    p = alpha(p, -1);
    p = blend(p, -1);
    p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_TRIANGLE_STRIP);
    for (int i = 0; i < 4; i++) p = vertex(p, xy[i][0], xy[i][1], rgb[i][0], rgb[i][1], rgb[i][2], uv[i][0], uv[i][1], 1.0f);
    p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
    g_open = p;
    /* The next blit rewrites g_blit: let this one finish first. */
    close_block();
    drain(3);
    g_bound = 0;
}

void hw_shadow(int r, const float (*xy)[2], int count)
{
    if (count <= 0) return;
    target(r);
    /* Black, averaged with what is there (blend mode 0): half brightness, each pixel
     * once (stencil bit 0x01); then, colour writes off, clear the bit again so the
     * PlayStation mask bit (0x80) is untouched. Only the shadow uses bit 0x01 and the
     * clear tests nothing, so one rectangle over the triangles clears it as redrawing
     * every triangle did, for a fraction of the vertices. */
    static float sxy[96][2], rgb[96][3], uv[96][2];
    for (int i = 0; i < 96; i++) { rgb[i][0] = rgb[i][1] = rgb[i][2] = 0; uv[i][0] = uv[i][1] = 0; }
    Tex t = linear(g_white, 64, 16, 16);
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    g_stencil_force = STENCIL_SHADOW;
    for (int start = 0; start < count; start += 32) {
        int n = count - start < 32 ? count - start : 32;
        for (int i = 0; i < n * 3; i++) {
            float x = sx(r, xy[start * 3 + i][0]), y = sy(r, xy[start * 3 + i][1]);
            sxy[i][0] = x; sxy[i][1] = y;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
        submit(r, NV097_SET_BEGIN_END_OP_TRIANGLES, (const float (*)[2])sxy, (const float (*)[3])rgb, (const float (*)[2])uv,
               0, n * 3, &t, 0, 1, 0);
    }
    g_stencil_force = STENCIL_SHADOW_CLEAR;
    { uint32_t *p = open_block(); g_open = pb_push1(p, NV097_SET_COLOR_MASK, 0); }
    x0 -= 2; y0 -= 2; x1 += 2; y1 += 2;
    const float quad[6][2] = { { x0, y0 }, { x1, y0 }, { x0, y1 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };
    submit(r, NV097_SET_BEGIN_END_OP_TRIANGLES, quad, (const float (*)[3])rgb, (const float (*)[2])uv, 0, 6, &t, 0, 1, 0);
    g_stencil_force = -1;
    uint32_t *p = open_block();
    g_open = pb_push1(p, NV097_SET_COLOR_MASK, 0x01010101);
}

/* The last presented display buffer, for test tools reading console memory. */
volatile uint32_t rt_hw_shown, rt_hw_shown_w, rt_hw_shown_h;

static void hw_present_timed(int x, int y, int wide)
{
    int r = -1;
    for (int i = 0; i < HW_REGIONS; i++)
        if (g_regions[i].valid && g_regions[i].x == x && g_regions[i].y == y) r = i;
    if (r < 0) return;
    RT_WHERE("hw present");
    rt_hw_shown = (uint32_t)pb_extra_buffer(r); rt_hw_shown_w = (uint32_t)OUT_W; rt_hw_shown_h = (uint32_t)OUT_H;
    target(-1);
    /* A 4:3 picture on a 16:9 screen takes the middle three quarters. */
    float left = 0, right = (float)OUT_W;
    if (rt_video_wide && !wide) {
        left = (float)(OUT_W / 8);
        right = (float)(OUT_W - OUT_W / 8);
        pb_fill(0, 0, OUT_W / 8, OUT_H, 0xFF000000u);
        pb_fill(OUT_W - OUT_W / 8, 0, OUT_W / 8, OUT_H, 0xFF000000u);
    }
    uint32_t *p = open_block();
    Tex t = linear(pb_extra_buffer(r), (unsigned)OUT_W * 4u, OUT_W, OUT_H);
    p = bind(p, &t);
    p = stencil(p, -1);
    p = alpha(p, -1);
    p = blend(p, -1);
    p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_TRIANGLE_STRIP);
    for (int i = 0; i < 4; i++) {
        float X = (i & 1) ? right : left, U = (i & 1) ? OUT_W : 0, Y = (i & 2) ? OUT_H : 0;
        p = vertex(p, X, Y, .5f, .5f, .5f, U, Y, 1.0f);
    }
    p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
    g_open = p;
    close_block();
    /* No wait for the GPU here: pbkit queues the flip (triple buffered) and the next
     * frame's commands follow in order. The push buffer wraps in open_block, and
     * texture memory the GPU may still read is only rewritten after a drain (in_flight). */
#ifdef RT_SYNC_PRESENT
    drain(4);
#endif
    rt_hw_wait = 5;
    while (pb_finished()) {}
    rt_hw_wait = 0;
#ifdef RT_SYNC_PRESENT
    pb_reset();
    g_base = 0;
#endif
    g_target = -1;
    g_bound = 0;
    g_bound_palette = 0;
    g_bound_filter = 0;
    g_stencil = -2;
    g_bound_address = 0;
    g_blend = -2;
    g_alpha = -1;
    g_frame++;
    rt_hw_frames++;
    RT_WHERE("hw presented");
}

void hw_present(int x, int y, int wide)
{
    PROF_BEGIN(t);
    hw_present_timed(x, y, wide);
    PROF_END(PROF_PRESENT, t);
}

/* ---- movie pictures ---------------------------------------------------------------------- */
/* Four buffers in turn: a picture is shown once per hw_picture_show, and pbkit queues
 * at most three, so the buffer handed out next is never one the GPU may still read. */
#define PICTURES 4
static uint32_t *g_pictures[PICTURES];
static unsigned g_picture_pitch, g_picture_w, g_picture_h, g_picture_bytes;
static int g_picture_next = -1;

int hw_picture_begin(int w, int h)
{
    if (!hw_ready() || w <= 0 || h <= 0) return 0;
    hw_picture_end();
    g_picture_pitch = ((unsigned)w * 4u + 63u) & ~63u;   /* linear textures: 64-byte rows */
    g_picture_w = (unsigned)w; g_picture_h = (unsigned)h;
    g_picture_bytes = g_picture_pitch * (unsigned)h;
    for (int i = 0; i < PICTURES; i++) {
        g_pictures[i] = contiguous(g_picture_bytes);
        if (!g_pictures[i]) { hw_picture_end(); return 0; }
        memset(g_pictures[i], 0, g_picture_bytes);
    }
    g_picture_next = -1;
    return 1;
}

uint32_t *hw_picture_next(unsigned *pitch)
{
    if (!g_pictures[0]) return 0;
    g_picture_next = (g_picture_next + 1) % PICTURES;
    *pitch = g_picture_pitch;
    return g_pictures[g_picture_next];
}

void hw_picture_show(float aspect)
{
    if (!g_pictures[0] || g_picture_next < 0) return;
    RT_WHERE("hw picture");
    target(-1);
    pb_fill(0, 0, OUT_W, OUT_H, 0xFF000000u);
    /* Fit the picture's display aspect to the screen's, centred. The screen's own
     * aspect is 16:9 or 4:3 whatever its pixel count (480 lines on a widescreen TV). */
    float screen = rt_video_wide ? 16.0f / 9.0f : 4.0f / 3.0f;
    float w = (float)OUT_W, h = (float)OUT_H;
    if (aspect > 0.0f) {
        if (aspect > screen) h = (float)OUT_H * screen / aspect;
        else w = (float)OUT_W * aspect / screen;
    }
    float left = ((float)OUT_W - w) * 0.5f, top = ((float)OUT_H - h) * 0.5f;
    uint32_t *p = open_block();
    Tex t = linear(g_pictures[g_picture_next], g_picture_pitch, g_picture_w, g_picture_h);
    t.filter = FILTER_LINEAR;
    p = bind(p, &t);
    p = stencil(p, -1);
    p = alpha(p, -1);
    p = blend(p, -1);
    p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_TRIANGLE_STRIP);
    for (int i = 0; i < 4; i++) {
        float X = (i & 1) ? left + w : left, Y = (i & 2) ? top + h : top;
        float U = (i & 1) ? (float)g_picture_w : 0.0f, V = (i & 2) ? (float)g_picture_h : 0.0f;
        p = vertex(p, X, Y, .5f, .5f, .5f, U, V, 1.0f);
    }
    p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
    g_open = p;
    close_block();
    while (pb_finished()) {}
    g_target = -1;
    g_bound = 0;
    g_bound_palette = 0;
    g_bound_filter = 0;
    g_stencil = -2;
    g_bound_address = 0;
    g_blend = -2;
    g_alpha = -1;
    g_frame++;
    rt_hw_frames++;
    RT_WHERE("hw picture shown");
}

void hw_picture_end(void)
{
    if (!g_pictures[0] && !g_pictures[PICTURES - 1]) return;
    close_block();
    drain(7);
    for (int i = 0; i < PICTURES; i++) {
        if (g_pictures[i]) MmFreeContiguousMemory(g_pictures[i]);
        g_pictures[i] = 0;
    }
    g_picture_next = -1;
}


/* ---- set-up -------------------------------------------------------------------------------- */
int hw_init(void)
{
    uint32_t program[] = {
#include "ps1vs.inl"
    };
    VIDEO_MODE mode = XVideoGetMode();
    if (mode.width > 0 && mode.height > 0) { g_out_w = mode.width; g_out_h = mode.height; }
    pb_extra_buffers(HW_REGIONS);
    pb_size(PUSHBUFFER_SIZE);
    if (pb_init()) return 0;
    uint32_t *p = pb_begin();
    p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_START, 0);
    p = pb_push1(p, NV097_SET_TRANSFORM_EXECUTION_MODE,
        MASK(NV097_SET_TRANSFORM_EXECUTION_MODE_MODE, NV097_SET_TRANSFORM_EXECUTION_MODE_MODE_PROGRAM) |
        MASK(NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE, NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE_PRIV));
    p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_CXT_WRITE_EN, 0);
    p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_LOAD, 0);
    pb_end(p);
    for (unsigned i = 0; i < sizeof program / sizeof program[0]; i += 4) {
        p = pb_begin();
        pb_push(p++, NV097_SET_TRANSFORM_PROGRAM, 4);
        memcpy(p, program + i, 16);
        p += 4;
        pb_end(p);
    }
    p = pb_begin();
#include "ps1ps.inl"
    p = pb_push1(p, NV097_SET_CONTROL0, NV097_SET_CONTROL0_TEXTURE_PERSPECTIVE_ENABLE | NV097_SET_CONTROL0_STENCIL_WRITE_ENABLE);
    p = pb_push1(p, NV097_SET_CULL_FACE_ENABLE, 0);
    p = pb_push1(p, NV097_SET_DEPTH_TEST_ENABLE, 0);
    p = pb_push1(p, NV097_SET_DEPTH_MASK, 0);
    p = pb_push1(p, NV097_SET_TEXTURE_ADDRESS, 0x00050505);
    p = pb_push1(p, NV097_SET_TEXTURE_CONTROL0, 0x4003ffc0);
    p = pb_push1(p, NV097_SET_TEXTURE_FILTER, 0x01012000);
    pb_end(p);

    swizzle_init();
    g_white = MmAllocateContiguousMemoryEx(64u * 16u, 0, 0x03ffafff, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    g_blit = MmAllocateContiguousMemoryEx(VRAM_W * VRAM_H * 4u, 0, 0x03ffafff, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (!g_white || !g_blit) return 0;
    for (int i = 0; i < 16 * 16; i++) g_white[i] = (ALPHA_OPAQUE << 24) | 0xFFFFFFu;
    for (int i = 0; i < HW_REGIONS; i++) {
        pb_target_extra_buffer(i);
        pb_fill(0, 0, OUT_W, OUT_H, 0xFF000000u);
    }
    pb_target_back_buffer();
    pb_show_front_screen();
    g_active = 1;
    return 1;
}

int hw_ready(void)
{
    static int tried;
    if (!tried) { tried = 1; hw_init(); }
    return g_active;
}
