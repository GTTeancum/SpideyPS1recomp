#include <string.h>
#include "gpu.h"

uint16_t gpu_vram[VRAM_H][VRAM_W] __attribute__((aligned(16)));

static int area_left, area_top, area_right = VRAM_W - 1, area_bottom = VRAM_H - 1;
static int offset_x, offset_y;
static int tex_page_x, tex_page_y, tex_depth, blend_mode, dither, tex_disable;
static int win_mask_x, win_mask_y, win_off_x, win_off_y;
static int set_mask, check_mask;

static int disp_x, disp_y;
static int h_range1 = 0x200, h_range2 = 0xC00, v_range1 = 0x10, v_range2 = 0x100;
static int hres, hres368, vres480, pal, disp24, interlace, display_disabled = 1, dma_dir;

static uint32_t fifo[256];
static int fifo_count, need, polyline;

static int load_image, load_x, load_y, load_w, load_h, load_px;
static int read_image, read_x, read_y, read_w, read_h, read_px;
static uint32_t gpu_read;
static int stat_field;

#define LEN_POLYLINE (-1)
#define LEN_IMAGE_LOAD (-2)

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int mini(int a, int b) { return a < b ? a : b; }
static int maxi(int a, int b) { return a > b ? a : b; }
static int sign_extend11(uint32_t v) { return (v & 0x400) ? (int)(v | 0xFFFFF800u) : (int)v; }
static int coord_x(uint32_t w) { int x = (int)(w & 0x7FF); return (x & 0x400) ? x - 0x800 : x; }
static int coord_y(uint32_t w) { int y = (int)((w >> 16) & 0x7FF); return (y & 0x400) ? y - 0x800 : y; }
static uint16_t to15(int r, int g, int b)
{
    return (uint16_t)(((r >> 3) & 0x1F) | (((g >> 3) & 0x1F) << 5) | (((b >> 3) & 0x1F) << 10));
}

/* ---- display --------------------------------------------------------------- */
static int cycles_per_pixel(void)
{
    if (hres368) return 7;
    switch (hres) { case 0: return 10; case 1: return 8; case 2: return 5; default: return 4; }
}

void gpu_display(GpuDisplay *out)
{
    out->x = disp_x;
    out->y = disp_y;
    out->width = clampi(((h_range2 - h_range1) / cycles_per_pixel() + 2) & ~3, 0, VRAM_W);
    int lines = v_range2 - v_range1;
    if (vres480) lines <<= 1;
    out->height = clampi(lines, 0, VRAM_H);
    out->rgb24 = disp24;
    out->enabled = !display_disabled;
}

uint32_t gpu_read_status(void)
{
    uint32_t s = 0;
    s |= (uint32_t)((tex_page_x / 64) & 0xF);
    s |= (uint32_t)(((tex_page_y / 256) & 1) << 4);
    s |= (uint32_t)((blend_mode & 3) << 5);
    s |= (uint32_t)((tex_depth & 3) << 7);
    if (dither) s |= 1u << 9;
    s |= 1u << 10;
    if (set_mask) s |= 1u << 11;
    if (check_mask) s |= 1u << 12;
    s |= 1u << 13;
    if (tex_disable) s |= 1u << 15;
    if (hres368) s |= 1u << 16;
    s |= (uint32_t)((hres & 3) << 17);
    if (vres480) s |= 1u << 19;
    if (pal) s |= 1u << 20;
    if (disp24) s |= 1u << 21;
    if (interlace) s |= 1u << 22;
    if (display_disabled) s |= 1u << 23;
    s |= (1u << 26) | (1u << 27) | (1u << 28);
    s |= (uint32_t)((dma_dir & 3) << 29);
    if (dma_dir == 1) s |= 1u << 25; else if (dma_dir == 2) s |= 1u << 28; else if (dma_dir == 3) s |= 1u << 27;
    stat_field = !stat_field;
    if (stat_field) s |= 1u << 31;
    return s;
}

/* ---- pixels ------------------------------------------------------------------ */
static uint16_t fetch_texel(int u, int v, int clut)
{
    u = (u & ~(win_mask_x * 8)) | ((win_off_x & win_mask_x) * 8);
    v = (v & ~(win_mask_y * 8)) | ((win_off_y & win_mask_y) * 8);
    u &= 0xFF; v &= 0xFF;
    int row = (tex_page_y + v) & (VRAM_H - 1);
    if (tex_depth == 2 || tex_depth == 3) return gpu_vram[row][(tex_page_x + u) & (VRAM_W - 1)];
    int clut_x = (clut & 0x3F) * 16, clut_y = (clut >> 6) & 0x1FF, index;
    if (tex_depth == 0) {
        uint16_t block = gpu_vram[row][(tex_page_x + (u >> 2)) & (VRAM_W - 1)];
        index = (block >> ((u & 3) * 4)) & 0xF;
    } else {
        uint16_t block = gpu_vram[row][(tex_page_x + (u >> 1)) & (VRAM_W - 1)];
        index = (block >> ((u & 1) * 8)) & 0xFF;
    }
    return gpu_vram[clut_y & (VRAM_H - 1)][(clut_x + index) & (VRAM_W - 1)];
}

static void plot(int x, int y, int r, int g, int b, int semi, int mask_bit)
{
    if (x < area_left || x > area_right || y < area_top || y > area_bottom) return;
    if (x < 0 || x >= VRAM_W || y < 0 || y >= VRAM_H) return;
    uint16_t bg = gpu_vram[y][x];
    if (check_mask && (bg & 0x8000)) return;
    int fr = mini(31, r >> 3), fg = mini(31, g >> 3), fb = mini(31, b >> 3);
    if (semi) {
        int br = bg & 0x1F, bgn = (bg >> 5) & 0x1F, bbl = (bg >> 10) & 0x1F;
        switch (blend_mode) {
        case 0: fr = (br + fr) >> 1; fg = (bgn + fg) >> 1; fb = (bbl + fb) >> 1; break;
        case 1: fr = mini(31, br + fr); fg = mini(31, bgn + fg); fb = mini(31, bbl + fb); break;
        case 2: fr = maxi(0, br - fr); fg = maxi(0, bgn - fg); fb = maxi(0, bbl - fb); break;
        default: fr = mini(31, br + (fr >> 2)); fg = mini(31, bgn + (fg >> 2)); fb = mini(31, bbl + (fb >> 2)); break;
        }
    }
    uint16_t out = (uint16_t)(fr | (fg << 5) | (fb << 10));
    if (set_mask || mask_bit) out |= 0x8000;
    gpu_vram[y][x] = out;
}

static void shade_texel(int x, int y, uint16_t texel, int r, int g, int b, int raw, int semi)
{
    if (texel == 0) return;
    int stp = (texel & 0x8000) != 0;
    int tr = (texel & 0x1F) << 3, tg = ((texel >> 5) & 0x1F) << 3, tb = ((texel >> 10) & 0x1F) << 3;
    if (!raw) { tr = tr * r >> 7; tg = tg * g >> 7; tb = tb * b >> 7; }
    plot(x, y, tr, tg, tb, semi && stp, stp);
}

/* ---- primitives ---------------------------------------------------------------- */
typedef struct { int x, y, r, g, b, u, v; } Vert;

static int top_left(const Vert *p0, const Vert *p1)
{
    int dy = p1->y - p0->y, dx = p1->x - p0->x;
    return dy < 0 || (dy == 0 && dx > 0);
}

static void raster_triangle(Vert a, Vert b, Vert c, int tex, int gouraud, int semi, int raw, int clut)
{
    int span_x = maxi(a.x, maxi(b.x, c.x)) - mini(a.x, mini(b.x, c.x));
    int span_y = maxi(a.y, maxi(b.y, c.y)) - mini(a.y, mini(b.y, c.y));
    if (span_x > 1023 || span_y > 511) return;
    int64_t area = (int64_t)(b.x - a.x) * (c.y - a.y) - (int64_t)(b.y - a.y) * (c.x - a.x);
    if (area == 0) return;
    if (area < 0) { Vert t = b; b = c; c = t; area = -area; }

    int min_x = maxi(area_left, mini(a.x, mini(b.x, c.x)));
    int max_x = mini(area_right, maxi(a.x, maxi(b.x, c.x)));
    int min_y = maxi(area_top, mini(a.y, mini(b.y, c.y)));
    int max_y = mini(area_bottom, maxi(a.y, maxi(b.y, c.y)));
    if (min_x > max_x || min_y > max_y) return;

    int bias0 = top_left(&b, &c) ? 0 : -1, bias1 = top_left(&c, &a) ? 0 : -1, bias2 = top_left(&a, &b) ? 0 : -1;
    int sx0 = b.y - c.y, sy0 = c.x - b.x;
    int sx1 = c.y - a.y, sy1 = a.x - c.x;
    int sx2 = a.y - b.y, sy2 = b.x - a.x;
    int64_t w0_row = (int64_t)(c.x - b.x) * (min_y - b.y) - (int64_t)(c.y - b.y) * (min_x - b.x);
    int64_t w1_row = (int64_t)(a.x - c.x) * (min_y - c.y) - (int64_t)(a.y - c.y) * (min_x - c.x);
    int64_t w2_row = (int64_t)(b.x - a.x) * (min_y - a.y) - (int64_t)(b.y - a.y) * (min_x - a.x);

    for (int y = min_y; y <= max_y; y++, w0_row += sy0, w1_row += sy1, w2_row += sy2) {
        int64_t w0 = w0_row, w1 = w1_row, w2 = w2_row;
        for (int x = min_x; x <= max_x; x++, w0 += sx0, w1 += sx1, w2 += sx2) {
            if (w0 + bias0 < 0 || w1 + bias1 < 0 || w2 + bias2 < 0) continue;
            int r, g, bl;
            if (gouraud) {
                r = (int)((w0 * a.r + w1 * b.r + w2 * c.r) / area);
                g = (int)((w0 * a.g + w1 * b.g + w2 * c.g) / area);
                bl = (int)((w0 * a.b + w1 * b.b + w2 * c.b) / area);
            } else { r = a.r; g = a.g; bl = a.b; }
            if (tex) {
                int u = (int)((w0 * a.u + w1 * b.u + w2 * c.u) / area);
                int v = (int)((w0 * a.v + w1 * b.v + w2 * c.v) / area);
                shade_texel(x, y, fetch_texel(u, v, clut), r, g, bl, raw, semi);
            } else plot(x, y, r, g, bl, semi, 0);
        }
    }
}

static void set_texpage(uint32_t tp)
{
    tex_page_x = (int)(tp & 0xF) * 64;
    tex_page_y = (int)((tp >> 4) & 1) * 256;
    blend_mode = (int)((tp >> 5) & 3);
    tex_depth = (int)((tp >> 7) & 3);
    tex_disable = (tp & (1u << 11)) != 0;
}

static void draw_polygon(void)
{
    uint32_t cmd = fifo[0];
    int gouraud = (cmd >> 28) & 1, quad = (cmd >> 27) & 1, tex = (cmd >> 26) & 1;
    int semi = (cmd >> 25) & 1, raw = (cmd >> 24) & 1, n = quad ? 4 : 3;
    Vert v[4];
    int idx = 1, clut = 0;
    int cr = (int)(cmd & 0xFF), cg = (int)((cmd >> 8) & 0xFF), cb = (int)((cmd >> 16) & 0xFF);
    for (int i = 0; i < n; i++) {
        if (gouraud && i > 0) {
            uint32_t cw = fifo[idx++];
            cr = (int)(cw & 0xFF); cg = (int)((cw >> 8) & 0xFF); cb = (int)((cw >> 16) & 0xFF);
        }
        v[i].r = cr; v[i].g = cg; v[i].b = cb;
        uint32_t vw = fifo[idx++];
        v[i].x = offset_x + coord_x(vw);
        v[i].y = offset_y + coord_y(vw);
        v[i].u = v[i].v = 0;
        if (tex) {
            uint32_t uvw = fifo[idx++];
            v[i].u = (int)(uvw & 0xFF);
            v[i].v = (int)((uvw >> 8) & 0xFF);
            if (i == 0) clut = (int)((uvw >> 16) & 0xFFFF);
            else if (i == 1) set_texpage((uvw >> 16) & 0xFFFF);
        }
    }
    raster_triangle(v[0], v[1], v[2], tex, gouraud, semi, raw, clut);
    if (quad) raster_triangle(v[1], v[2], v[3], tex, gouraud, semi, raw, clut);
}

static void draw_rectangle(void)
{
    uint32_t cmd = fifo[0];
    int sz = (int)((cmd >> 27) & 3), tex = (cmd >> 26) & 1, semi = (cmd >> 25) & 1, raw = (cmd >> 24) & 1;
    int cr = (int)(cmd & 0xFF), cg = (int)((cmd >> 8) & 0xFF), cb = (int)((cmd >> 16) & 0xFF);
    int idx = 1;
    uint32_t vw = fifo[idx++];
    int x = offset_x + coord_x(vw), y = offset_y + coord_y(vw);
    int u0 = 0, v0 = 0, clut = 0;
    if (tex) {
        uint32_t uvw = fifo[idx++];
        u0 = (int)(uvw & 0xFF); v0 = (int)((uvw >> 8) & 0xFF); clut = (int)((uvw >> 16) & 0xFFFF);
    }
    int w, h;
    if (sz == 0) { uint32_t wh = fifo[idx]; w = (int)(wh & 0xFFFF); h = (int)((wh >> 16) & 0xFFFF); }
    else w = h = sz == 1 ? 1 : sz == 2 ? 8 : 16;
    int x0 = maxi(x, area_left), x1 = mini(x + w - 1, area_right);
    int y0 = maxi(y, area_top), y1 = mini(y + h - 1, area_bottom);
    for (int py = y0; py <= y1; py++)
        for (int px = x0; px <= x1; px++) {
            if (tex) shade_texel(px, py, fetch_texel((u0 + px - x) & 0xFF, (v0 + py - y) & 0xFF, clut), cr, cg, cb, raw, semi);
            else plot(px, py, cr, cg, cb, semi, 0);
        }
}

static void line_segment(int x0, int y0, int r0, int g0, int b0, int x1, int y1, int r1, int g1, int b1, int semi)
{
    x0 += offset_x; y0 += offset_y; x1 += offset_x; y1 += offset_y;
    int dx = x1 - x0, dy = y1 - y0;
    int steps = maxi(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
    if (steps == 0) { plot(x0, y0, r0, g0, b0, semi, 0); return; }
    if (steps > 2048) return;
    for (int i = 0; i <= steps; i++) {
        int x = x0 + (dx * i + (dx >= 0 ? steps / 2 : -steps / 2)) / steps;
        int y = y0 + (dy * i + (dy >= 0 ? steps / 2 : -steps / 2)) / steps;
        plot(x, y, r0 + (r1 - r0) * i / steps, g0 + (g1 - g0) * i / steps, b0 + (b1 - b0) * i / steps, semi, 0);
    }
}

static void rgb(uint32_t w, int *r, int *g, int *b) { *r = (int)(w & 0xFF); *g = (int)((w >> 8) & 0xFF); *b = (int)((w >> 16) & 0xFF); }

static void draw_line(void)
{
    uint32_t cmd = fifo[0];
    int gouraud = (cmd >> 28) & 1, semi = (cmd >> 25) & 1, idx = 1;
    int r0, g0, b0, r1, g1, b1;
    rgb(cmd, &r0, &g0, &b0); r1 = r0; g1 = g0; b1 = b0;
    uint32_t v0 = fifo[idx++];
    if (gouraud) rgb(fifo[idx++], &r1, &g1, &b1);
    uint32_t v1 = fifo[idx++];
    line_segment(coord_x(v0), coord_y(v0), r0, g0, b0, coord_x(v1), coord_y(v1), r1, g1, b1, semi);
}

static void execute_polyline(void)
{
    uint32_t cmd = fifo[0];
    int gouraud = (cmd >> 28) & 1, semi = (cmd >> 25) & 1, idx = 1, first = 1, have = 0;
    int r, g, b, px = 0, py = 0, pr = 0, pg = 0, pb = 0;
    rgb(cmd, &r, &g, &b);
    while (idx < fifo_count) {
        if (gouraud && !first) rgb(fifo[idx++], &r, &g, &b);
        if (idx >= fifo_count) break;
        uint32_t vw = fifo[idx++];
        int x = coord_x(vw), y = coord_y(vw);
        if (have) line_segment(px, py, pr, pg, pb, x, y, r, g, b, semi);
        px = x; py = y; pr = r; pg = g; pb = b; have = 1; first = 0;
    }
}

static void fill_rect(void)
{
    uint16_t color = to15((int)(fifo[0] & 0xFF), (int)((fifo[0] >> 8) & 0xFF), (int)((fifo[0] >> 16) & 0xFF));
    int x = (int)(fifo[1] & 0x3F0), y = (int)((fifo[1] >> 16) & 0x1FF);
    int w = (int)(((fifo[2] & 0x3FF) + 0xF) & ~0xF), h = (int)((fifo[2] >> 16) & 0x1FF);
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            gpu_vram[(y + dy) & (VRAM_H - 1)][(x + dx) & (VRAM_W - 1)] = color;
}

static void copy_vram(void)
{
    int sx = (int)(fifo[1] & 0x3FF), sy = (int)((fifo[1] >> 16) & 0x1FF);
    int dx = (int)(fifo[2] & 0x3FF), dy = (int)((fifo[2] >> 16) & 0x1FF);
    int w = (int)(fifo[3] & 0x3FF); if (!w) w = 0x400;
    int h = (int)((fifo[3] >> 16) & 0x1FF); if (!h) h = 0x200;
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++) {
            uint16_t px = gpu_vram[(sy + row) & (VRAM_H - 1)][(sx + col) & (VRAM_W - 1)];
            uint16_t *d = &gpu_vram[(dy + row) & (VRAM_H - 1)][(dx + col) & (VRAM_W - 1)];
            if (check_mask && (*d & 0x8000)) continue;
            *d = set_mask ? (uint16_t)(px | 0x8000) : px;
        }
}

static void begin_image_load(void)
{
    load_x = (int)(fifo[1] & 0x3FF);
    load_y = (int)((fifo[1] >> 16) & 0x1FF);
    load_w = (int)(fifo[2] & 0xFFFF); load_w = load_w == 0 ? 0x400 : (load_w & 0x3FF) ? (load_w & 0x3FF) : 0x400;
    load_h = (int)((fifo[2] >> 16) & 0xFFFF); load_h = load_h == 0 ? 0x200 : (load_h & 0x1FF) ? (load_h & 0x1FF) : 0x200;
    load_px = 0;
    load_image = 1;
    fifo_count = 0;
}

static void store_halfword(uint16_t value)
{
    if (!load_image) return;
    uint16_t stored = set_mask ? (uint16_t)(value | 0x8000) : value;
    int x = (load_x + load_px % load_w) & (VRAM_W - 1), y = (load_y + load_px / load_w) & (VRAM_H - 1);
    if (!(check_mask && (gpu_vram[y][x] & 0x8000))) gpu_vram[y][x] = stored;
    if (++load_px >= load_w * load_h) load_image = 0;
}

static void begin_image_read(void)
{
    read_x = (int)(fifo[1] & 0x3FF);
    read_y = (int)((fifo[1] >> 16) & 0x1FF);
    read_w = (int)(fifo[2] & 0x3FF); if (!read_w) read_w = 0x400;
    read_h = (int)((fifo[2] >> 16) & 0x1FF); if (!read_h) read_h = 0x200;
    read_px = 0;
    read_image = 1;
}

static uint16_t read_halfword(void)
{
    if (!read_image) return 0;
    int x = (read_x + read_px % read_w) & (VRAM_W - 1), y = (read_y + read_px / read_w) & (VRAM_H - 1);
    uint16_t v = gpu_vram[y][x];
    if (++read_px >= read_w * read_h) read_image = 0;
    return v;
}

uint32_t gpu_read_data(void)
{
    if (!read_image) return gpu_read;
    uint16_t lo = read_halfword(), hi = read_halfword();
    return (uint32_t)lo | ((uint32_t)hi << 16);
}

/* ---- command queue ----------------------------------------------------------------- */
static int command_length(uint32_t word)
{
    uint32_t op = word >> 24;
    if (op == 0x02) return 3;
    if (op >= 0x20 && op <= 0x3F) {
        int n = (word & (1u << 27)) ? 4 : 3, shaded = (word >> 28) & 1, tex = (word >> 26) & 1;
        return 1 + n + (shaded ? n - 1 : 0) + (tex ? n : 0);
    }
    if (op >= 0x40 && op <= 0x5F) {
        if (word & (1u << 27)) return LEN_POLYLINE;
        return 1 + 2 + ((word >> 28) & 1);
    }
    if (op >= 0x60 && op <= 0x7F) {
        int sz = (int)((word >> 27) & 3), tex = (word >> 26) & 1;
        return 1 + 1 + tex + (sz == 0);
    }
    if (op >= 0x80 && op <= 0x9F) return 4;
    if (op >= 0xA0 && op <= 0xBF) return LEN_IMAGE_LOAD;
    if (op >= 0xC0 && op <= 0xDF) return 3;
    return 1;
}

static void execute(void)
{
    uint32_t word = fifo[0], op = word >> 24;
    if (op == 0x02) fill_rect();
    else if (op >= 0x20 && op <= 0x3F) draw_polygon();
    else if (op >= 0x40 && op <= 0x5F) draw_line();
    else if (op >= 0x60 && op <= 0x7F) draw_rectangle();
    else if (op >= 0x80 && op <= 0x9F) copy_vram();
    else if (op >= 0xA0 && op <= 0xBF) begin_image_load();
    else if (op >= 0xC0 && op <= 0xDF) begin_image_read();
    else switch (op) {
    case 0xE1:
        tex_page_x = (int)(word & 0xF) * 64; tex_page_y = (int)((word >> 4) & 1) * 256;
        blend_mode = (int)((word >> 5) & 3); tex_depth = (int)((word >> 7) & 3);
        dither = (word >> 9) & 1; tex_disable = (word >> 11) & 1;
        break;
    case 0xE2:
        win_mask_x = (int)(word & 0x1F); win_mask_y = (int)((word >> 5) & 0x1F);
        win_off_x = (int)((word >> 10) & 0x1F); win_off_y = (int)((word >> 15) & 0x1F);
        break;
    case 0xE3: area_left = (int)(word & 0x3FF); area_top = (int)((word >> 10) & 0x3FF); break;
    case 0xE4: area_right = (int)(word & 0x3FF); area_bottom = (int)((word >> 10) & 0x3FF); break;
    case 0xE5: offset_x = sign_extend11(word & 0x7FF); offset_y = sign_extend11((word >> 11) & 0x7FF); break;
    case 0xE6: set_mask = word & 1; check_mask = (word >> 1) & 1; break;
    }
}

void gpu_write_gp0(uint32_t word)
{
    if (load_image) { store_halfword((uint16_t)word); store_halfword((uint16_t)(word >> 16)); return; }
    if (polyline) {
        if ((word & 0xF000F000u) == 0x50005000u) { polyline = 0; execute_polyline(); fifo_count = 0; }
        else if (fifo_count < 256) fifo[fifo_count++] = word;
        return;
    }
    fifo[fifo_count++] = word;
    if (fifo_count == 1) {
        need = command_length(word);
        if (need == LEN_POLYLINE) { polyline = 1; return; }
        if (need == LEN_IMAGE_LOAD) need = 3;
    }
    if (fifo_count >= need) { execute(); if (!load_image) fifo_count = 0; }
}

static void reset(void)
{
    fifo_count = 0;
    polyline = load_image = read_image = 0;
    display_disabled = 1;
    dma_dir = 0;
    tex_page_x = tex_page_y = tex_depth = blend_mode = 0;
    dither = tex_disable = 0;
    win_mask_x = win_mask_y = win_off_x = win_off_y = 0;
    area_left = area_top = 0; area_right = VRAM_W - 1; area_bottom = VRAM_H - 1;
    offset_x = offset_y = 0;
    set_mask = check_mask = 0;
    disp_x = disp_y = 0;
}

void gpu_write_gp1(uint32_t word)
{
    uint32_t op = (word >> 24) & 0xFF, p = word & 0xFFFFFF;
    switch (op) {
    case 0x00: reset(); break;
    case 0x01: fifo_count = 0; polyline = 0; load_image = 0; break;
    case 0x03: display_disabled = p & 1; break;
    case 0x04: dma_dir = (int)(p & 3); break;
    case 0x05: disp_x = (int)(p & 0x3FF); disp_y = (int)((p >> 10) & 0x1FF); break;
    case 0x06: h_range1 = (int)(p & 0xFFF); h_range2 = (int)((p >> 12) & 0xFFF); break;
    case 0x07: v_range1 = (int)(p & 0x3FF); v_range2 = (int)((p >> 10) & 0x3FF); break;
    case 0x08:
        hres = (int)(p & 3); hres368 = (p >> 6) & 1; vres480 = (p >> 2) & 1;
        pal = (p >> 3) & 1; disp24 = (p >> 4) & 1; interlace = (p >> 5) & 1;
        break;
    case 0x10:
        switch (p & 0xFF) {
        case 0x03: gpu_read = (uint32_t)(area_left | (area_top << 10)); break;
        case 0x04: gpu_read = (uint32_t)(area_right | (area_bottom << 10)); break;
        case 0x05: gpu_read = (uint32_t)((offset_x & 0x7FF) | ((offset_y & 0x7FF) << 11)); break;
        default: gpu_read = 0; break;
        }
        break;
    }
}
