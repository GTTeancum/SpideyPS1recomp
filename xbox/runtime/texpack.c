#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "gpu.h"
#include "hwgpu.h"
#include "texpack.h"

/* ---- uploads (VramTracker) ----------------------------------------------------------- */
/* The last few uploads overlapping each 16x16 block: a texture is whatever the game
 * uploaded as one image, so that is the region a pack key covers. */
#define BLOCK_W 16
#define BLOCK_H 16
#define COLS (VRAM_W / BLOCK_W)
#define ROWS (VRAM_H / BLOCK_H)
#define PER_BLOCK 4
typedef struct { int16_t x, y, w, h; uint32_t stamp; } Upload;
static Upload g_uploads[ROWS * COLS * PER_BLOCK];
static uint8_t g_gpu_dirty[ROWS * COLS];
static uint32_t g_upload_clock;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void bounds(int x, int y, int w, int h, int *c0, int *r0, int *c1, int *r1)
{
    *c0 = clampi(x / BLOCK_W, 0, COLS - 1); *r0 = clampi(y / BLOCK_H, 0, ROWS - 1);
    *c1 = clampi((x + (w > 1 ? w - 1 : 0)) / BLOCK_W, 0, COLS - 1);
    *r1 = clampi((y + (h > 1 ? h - 1 : 0)) / BLOCK_H, 0, ROWS - 1);
}

void texpack_note_upload(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0 || w > VRAM_W || h > VRAM_H) return;
    Upload up = { (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, ++g_upload_clock };
    int c0, r0, c1, r1;
    bounds(x, y, w, h, &c0, &r0, &c1, &r1);
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) {
            Upload *slot = &g_uploads[(r * COLS + c) * PER_BLOCK];
            for (int i = PER_BLOCK - 1; i > 0; i--) slot[i] = slot[i - 1];
            slot[0] = up;
            g_gpu_dirty[r * COLS + c] = 0;
        }
}

void texpack_note_gpu_write(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    int c0, r0, c1, r1;
    bounds(x, y, w, h, &c0, &r0, &c1, &r1);
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) g_gpu_dirty[r * COLS + c] = 1;
}

static int gpu_dirty(int x, int y, int w, int h)
{
    int c0, r0, c1, r1;
    bounds(x, y, w, h, &c0, &r0, &c1, &r1);
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) if (g_gpu_dirty[r * COLS + c]) return 1;
    return 0;
}

/* The smallest recorded upload wholly containing the rectangle; ties to the newest. */
static const Upload *find_upload(int x, int y, int w, int h)
{
    const Upload *slot = &g_uploads[(clampi(y / BLOCK_H, 0, ROWS - 1) * COLS + clampi(x / BLOCK_W, 0, COLS - 1)) * PER_BLOCK];
    const Upload *best = 0;
    for (int i = 0; i < PER_BLOCK; i++) {
        const Upload *u = &slot[i];
        if (!u->stamp || x < u->x || y < u->y || x + w > u->x + u->w || y + h > u->y + u->h) continue;
        int area = u->w * u->h;
        if (!best || area < best->w * best->h || (area == best->w * best->h && u->stamp > best->stamp)) best = u;
    }
    return best;
}

/* ---- fingerprint (TextureTile.Hash) -------------------------------------------------- */
typedef struct { int page_x, page_y, u0, v0, w, h, bpp, clut_x, clut_y, clut_count; } Region;

static void fnv(uint64_t *h, uint8_t b) { *h ^= b; *h *= 1099511628211ull; }

static int vram_x(const Region *r) { return r->page_x + (r->bpp == 4 ? r->u0 >> 2 : r->bpp == 8 ? r->u0 >> 1 : r->u0); }
static int vram_w(const Region *r)
{
    if (r->bpp == 4) return ((r->u0 + r->w - 1) >> 2) - (r->u0 >> 2) + 1;
    if (r->bpp == 8) return ((r->u0 + r->w - 1) >> 1) - (r->u0 >> 1) + 1;
    return r->w;
}

static int fingerprint(const Region *r, uint64_t *index_hash, uint64_t *clut_hash)
{
    if (r->w <= 0 || r->h <= 0 || r->w * r->h > 256 * 256) return 0;
    uint64_t used[4] = { 0, 0, 0, 0 };
    uint64_t h = 1469598103934665603ull;
    fnv(&h, (uint8_t)r->bpp);
    fnv(&h, (uint8_t)(r->w & 0xFF)); fnv(&h, (uint8_t)(r->w >> 8));
    fnv(&h, (uint8_t)(r->h & 0xFF)); fnv(&h, (uint8_t)(r->h >> 8));
    int x0 = vram_x(r), words = vram_w(r), y0 = r->page_y + r->v0;
    for (int y = 0; y < r->h; y++) {
        const uint16_t *row = gpu_vram[(y0 + y) & (VRAM_H - 1)];
        for (int x = 0; x < words; x++) {
            uint16_t v = row[(x0 + x) & (VRAM_W - 1)];
            fnv(&h, (uint8_t)(v & 0xFF)); fnv(&h, (uint8_t)(v >> 8));
            if (r->bpp == 4) {
                used[0] |= 1ull << (v & 0xF); used[0] |= 1ull << ((v >> 4) & 0xF);
                used[0] |= 1ull << ((v >> 8) & 0xF); used[0] |= 1ull << ((v >> 12) & 0xF);
            } else if (r->bpp == 8) {
                int a = v & 0xFF, b = (v >> 8) & 0xFF;
                used[a >> 6] |= 1ull << (a & 63);
                used[b >> 6] |= 1ull << (b & 63);
            }
        }
    }
    *index_hash = h;
    *clut_hash = 0;
    if (!r->clut_count) return 1;
    uint64_t c = 1469598103934665603ull;
    const uint16_t *clut = gpu_vram[r->clut_y & (VRAM_H - 1)];
    for (int i = 0; i < r->clut_count; i++) {
        if (!(used[i >> 6] & (1ull << (i & 63)))) continue;
        uint16_t e = clut[(r->clut_x + i) & (VRAM_W - 1)];
        fnv(&c, (uint8_t)(i & 0xFF)); fnv(&c, (uint8_t)(e & 0xFF)); fnv(&c, (uint8_t)(e >> 8));
    }
    *clut_hash = c;
    return 1;
}

/* ---- the pack ------------------------------------------------------------------------ */
typedef struct { uint64_t index_hash, clut_hash; void *dxt1; int width, height, failed; } PackTexture;
#define PACK_MAX 2048
#define PACK_SLOTS 4096
static PackTexture g_pack[PACK_MAX];
static int16_t g_pack_slot[PACK_SLOTS];      /* index + 1 into g_pack, by fingerprint */
static int g_pack_count;
static char g_pack_dir[96];
volatile uint32_t rt_pack_hits, rt_pack_loads;

static unsigned slot_of(uint64_t a, uint64_t b) { return (unsigned)((a ^ (b * 0x9E3779B97F4A7C15ull)) >> 40) & (PACK_SLOTS - 1); }

int texpack_load(const char *dir)
{
    char path[160], line[96];
    snprintf(g_pack_dir, sizeof g_pack_dir, "%s", dir);
    snprintf(path, sizeof path, "%s\\index.txt", dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    while (g_pack_count < PACK_MAX && fgets(line, sizeof line, f)) {
        unsigned long long a, b;
        if (sscanf(line, "%16llx_%16llx", &a, &b) != 2) continue;
        PackTexture *t = &g_pack[g_pack_count];
        t->index_hash = a; t->clut_hash = b;
        unsigned s = slot_of(a, b);
        while (g_pack_slot[s]) s = (s + 1) & (PACK_SLOTS - 1);
        g_pack_slot[s] = (int16_t)(++g_pack_count);
    }
    fclose(f);
    return g_pack_count;
}

static PackTexture *find(uint64_t a, uint64_t b)
{
    if (!g_pack_count) return 0;
    for (unsigned s = slot_of(a, b); g_pack_slot[s]; s = (s + 1) & (PACK_SLOTS - 1)) {
        PackTexture *t = &g_pack[g_pack_slot[s] - 1];
        if (t->index_hash == a && t->clut_hash == b) return t;
    }
    return 0;
}

/* The DDS, read on first use. */
static int ready(PackTexture *t)
{
    if (t->dxt1) return 1;
    if (t->failed) return 0;
    char path[160];
    snprintf(path, sizeof path, "%s\\%016llx_%016llx.dds", g_pack_dir,
             (unsigned long long)t->index_hash, (unsigned long long)t->clut_hash);
    t->failed = 1;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t header[128];
    if (fread(header, 1, sizeof header, f) == sizeof header && memcmp(header, "DDS ", 4) == 0 &&
        memcmp(header + 84, "DXT1", 4) == 0) {
        uint32_t h, w;
        memcpy(&h, header + 12, 4);
        memcpy(&w, header + 16, 4);
        uint32_t bytes = w * h / 2;
        if (w >= 4 && h >= 4 && w <= 1024 && h <= 1024 && !(w & (w - 1)) && !(h & (h - 1))) {
            void *p = MmAllocateContiguousMemoryEx(bytes, 0, 0x03ffafff, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
            if (p && fread(p, 1, bytes, f) == bytes) {
                __builtin_ia32_sfence();
                t->dxt1 = p; t->width = (int)w; t->height = (int)h; t->failed = 0;
                rt_pack_loads++;
            } else if (p) MmFreeContiguousMemory(p);
        }
    }
    fclose(f);
    return t->dxt1 != 0;
}

/* ---- lookup (TextureResolver.ResolvePage, upload path) ------------------------------- */
/* Fingerprints are remembered per uploaded region and CLUT until VRAM under either
 * changes. */
#define MEMO 1024
typedef struct { int16_t x, y, w, h; int32_t clut; uint32_t stamp; PackTexture *hit; int valid; } Memo;
static Memo g_memo[MEMO];

int texpack_lookup(const RtTile *tile, RtReplacement *out)
{
    if (!g_pack_count) return 0;
    int bpp = tile->depth == 0 ? 4 : tile->depth == 1 ? 8 : 16;
    int per_word = bpp == 4 ? 4 : bpp == 8 ? 2 : 1;
    Region t = { tile->page_x, tile->page_y, tile->u0, tile->v0, tile->w, tile->h, bpp,
                 tile->clut_x, tile->clut_y, bpp == 4 ? 16 : bpp == 8 ? 256 : 0 };
    int tx = vram_x(&t), tw = vram_w(&t), ty = t.page_y + t.v0;
    const Upload *up = find_upload(tx, ty, tw, t.h);
    if (!up || gpu_dirty(up->x, up->y, up->w, up->h)) return 0;
    Region r = { t.page_x, t.page_y, (up->x - t.page_x) * per_word, up->y - t.page_y, up->w * per_word, up->h,
                 bpp, t.clut_x, t.clut_y, t.clut_count };
    if (r.w <= 0 || r.h <= 0 || r.w * r.h > 256 * 256) return 0;
    int32_t clut = (tile->clut_y << 10) | tile->clut_x | (bpp << 20);
    uint32_t stamp = hw_vram_newest(up->x, up->y, up->w, up->h);
    if (r.clut_count) {
        uint32_t c = hw_vram_newest(r.clut_x, r.clut_y, r.clut_count, 1);
        if (c > stamp) stamp = c;
    }
    unsigned m = ((unsigned)up->x * 31u + (unsigned)up->y * 131u + (unsigned)up->w * 7u + (unsigned)clut * 2654435761u) & (MEMO - 1);
    Memo *e = &g_memo[m];
    if (!e->valid || e->x != up->x || e->y != up->y || e->w != up->w || e->h != up->h || e->clut != clut || e->stamp < stamp) {
        uint64_t a, b;
        e->x = up->x; e->y = up->y; e->w = up->w; e->h = up->h; e->clut = clut;
        e->stamp = hw_vram_generation();
        e->valid = 1;
        e->hit = fingerprint(&r, &a, &b) ? find(a, b) : 0;
    }
    if (!e->hit || !ready(e->hit)) return 0;
    out->dxt1 = e->hit->dxt1;
    out->width = e->hit->width; out->height = e->hit->height;
    out->u0 = r.u0; out->v0 = r.v0; out->w = r.w; out->h = r.h;
    rt_pack_hits++;
    return 1;
}
