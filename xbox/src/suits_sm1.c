/*
 * Mod suits (reskins): a port of the PC's SuitMods.cs material binding and
 * ActorMaterialCache (spiderman/patches, RecompOne.Runtime/Assets/Textures). A fix to
 * either belongs in both places.
 *
 * A reskin keeps the stock Spider-Man model and replaces its textures. The loaded
 * model ("spidey" in the actor table at 0x800A0904) lists its materials: a texture
 * page, CLUT and UV rectangle each, and an id (+20) that the suit's textures are keyed
 * by. Each ordering table, the materials whose id the suit replaces are bound; a
 * textured primitive whose texels lie inside a bound material then samples the suit's
 * texture over that rectangle instead.
 *
 * The suit is chosen by suit=<id> in D:\openspidey.ini. Its textures are DXT1 DDS
 * files made from the PC suit's PNGs by xbox/tools/prep_suits.py, capped in size for
 * the Xbox's memory, in D:\mods\suits\<id> with a suit.txt listing them.
 *
 * The suit also names the bundled model it reskins and the stock powers it uses, and
 * may bring its own model (modelfile) with an animation rig (retarget_sm1.c); both are
 * served by assets_sm1.c when the game loads spidey.psx. Materials bind only while
 * that suit's model is the one loaded. Not yet on the Xbox: choosing a suit in the
 * game's costume menu.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "runtime.h"
#include "settings.h"

#define ACTORS 0x800A0904u
#define MAX_TEXTURES 32
#define MAX_BINDINGS 64

typedef struct { uint32_t id; void *dxt1; int width, height; } SuitTexture;
static SuitTexture g_textures[MAX_TEXTURES];
static int g_texture_count;

typedef struct { int page_x, page_y, depth, clut_x, clut_y, u0, v0, w, h; const SuitTexture *texture; } Binding;
static Binding g_bindings[MAX_BINDINGS];
static int g_binding_count;
/* The texture pages (32 x 3 depths) the bindings use: a world triangle on any other
 * page is answered without going through the list. */
static uint32_t g_binding_pages[3];
static int page_bit(int page_x, int page_y) { return (page_x / 64) + (page_y / 256) * 16; }

static int ram(uint32_t p, uint32_t size) { return p >= 0x80000000u && p <= 0x80800000u - size; }

/* ---- loading ------------------------------------------------------------------------ */
static void *load_dds(const char *path, int *width, int *height)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t header[128];
    void *pixels = 0;
    if (fread(header, 1, sizeof header, f) == sizeof header && memcmp(header, "DDS ", 4) == 0 &&
        memcmp(header + 84, "DXT1", 4) == 0) {
        uint32_t h, w;
        memcpy(&h, header + 12, 4);
        memcpy(&w, header + 16, 4);
        uint32_t bytes = w * h / 2;
        if (w >= 4 && h >= 4 && w <= 1024 && h <= 1024 && !(w & (w - 1)) && !(h & (h - 1))) {
            pixels = MmAllocateContiguousMemoryEx(bytes, 0, 0x03ffafff, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
            if (pixels && fread(pixels, 1, bytes, f) != bytes) { MmFreeContiguousMemory(pixels); pixels = 0; }
            /* Write-combined: drain the CPU's writes before the GPU is pointed at them. */
            __builtin_ia32_sfence();
            *width = (int)w; *height = (int)h;
        }
    }
    fclose(f);
    return pixels;
}

static int g_active, g_profile;
static char g_model[24] = "spiderman", g_model_file[160], g_rig_file[160];

int suit_active(void) { return g_active; }
const char *suit_model(void) { return g_model; }
int suit_profile(void) { return g_profile; }
const char *suit_model_file(void) { return g_model_file[0] ? g_model_file : 0; }
const char *suit_rig_file(void) { return g_rig_file[0] ? g_rig_file : 0; }

static void load_suit(const char *id)
{
    char path[160], line[160];
    snprintf(path, sizeof path, "D:\\mods\\suits\\%s\\suit.txt", id);
    FILE *f = fopen(path, "r");
    if (!f) { rt_note("suit not found"); return; }
    while (g_texture_count < MAX_TEXTURES && fgets(line, sizeof line, f)) {
        unsigned key;
        char file[64];
        int profile;
        /* "modelfile" before "model": sscanf's "model %s" also matches "modelfile x". */
        if (sscanf(line, "modelfile %63s", file) == 1) { snprintf(g_model_file, sizeof g_model_file, "D:\\mods\\suits\\%s\\%s", id, file); continue; }
        if (!strncmp(line, "model ", 6) && sscanf(line + 6, "%23s", g_model) == 1) continue;
        if (sscanf(line, "profile %d", &profile) == 1) { if (profile >= 0 && profile < 10) g_profile = profile; continue; }
        if (sscanf(line, "rig %63s", file) == 1) { snprintf(g_rig_file, sizeof g_rig_file, "D:\\mods\\suits\\%s\\%s", id, file); continue; }
        if (sscanf(line, "texture %x %63s", &key, file) != 2) continue;
        char dds[200];
        snprintf(dds, sizeof dds, "D:\\mods\\suits\\%s\\%s", id, file);
        SuitTexture *t = &g_textures[g_texture_count];
        t->dxt1 = load_dds(dds, &t->width, &t->height);
        if (!t->dxt1) { rt_note("suit texture failed to load"); continue; }
        t->id = key;
        g_texture_count++;
    }
    fclose(f);
}

static const SuitTexture *texture_for(uint32_t id)
{
    for (int i = 0; i < g_texture_count; i++) if (g_textures[i].id == id) return &g_textures[i];
    return 0;
}

/* ---- binding (ActorMaterialCache.Build) --------------------------------------------- */
static void bind_materials(void)
{
    g_binding_count = 0;
    g_binding_pages[0] = g_binding_pages[1] = g_binding_pages[2] = 0;
    uint32_t entry = 0;
    for (uint32_t i = 0; i < 40; i++) {
        uint32_t p = ACTORS + i * 64u;
        if (mem_r32(p) == 0x64697073u && mem_r16(p + 4) == 0x7965u && mem_r8(p + 6) == 0) { entry = p; break; }
    }
    if (!entry) return;
    uint32_t model = mem_r32(entry + 0x14);
    if (!ram(model, 16)) return;
    uint32_t objects = mem_r32(model + 8);
    if (objects > 1024 || !ram(model, 16 + objects * 36)) return;
    uint32_t meshes = mem_r32(model + 12 + objects * 36);
    uint32_t metadata = mem_r32(entry + 12);
    if (meshes > 4096 || !ram(metadata, meshes * 4 + 4)) return;
    uint32_t start = metadata + meshes * 4, count = mem_r32(start);
    if (count > 4096 || !ram(start, 4 + count * 4)) return;
    for (uint32_t i = 0; i < count && g_binding_count < MAX_BINDINGS; i++) {
        uint32_t p = mem_r32(start + 4 + i * 4);
        if (!ram(p, 32)) continue;
        const SuitTexture *t = texture_for(mem_r32(p + 20));
        if (!t) continue;
        int u = (int)mem_r8(p), v = (int)mem_r8(p + 1);
        int tpage = (int)mem_r16(p + 6), clut = (int)mem_r16(p + 2), depth = (tpage >> 7) & 3;
        Binding *b = &g_bindings[g_binding_count++];
        b->page_x = (tpage & 0xF) * 64; b->page_y = ((tpage >> 4) & 1) * 256;
        b->depth = depth > 2 ? 2 : depth;
        b->clut_x = (clut & 0x3F) * 16; b->clut_y = (clut >> 6) & 0x1FF;
        b->u0 = u; b->v0 = v;
        b->w = (((int)mem_r8(p + 4) - u) & 255) + 1;
        b->h = (((int)mem_r8(p + 9) - v) & 255) + 1;
        b->texture = t;
        g_binding_pages[b->depth] |= 1u << page_bit(b->page_x, b->page_y);
    }
    if (g_binding_count) rt_note("suit materials bound");
}

/* ---- lookup (SuitMods.ResolveUncached's match) -------------------------------------- */
static int lookup(const RtTile *tile, RtReplacement *out)
{
    if (tile->depth < 0 || tile->depth > 2 || !(g_binding_pages[tile->depth] >> page_bit(tile->page_x, tile->page_y) & 1))
        return 0;
    if (!assets_sm1_suit_loaded()) return 0;
    for (int i = 0; i < g_binding_count; i++) {
        const Binding *b = &g_bindings[i];
        if (tile->page_x != b->page_x || tile->page_y != b->page_y || tile->depth != b->depth ||
            tile->clut_x != b->clut_x || tile->clut_y != b->clut_y ||
            tile->u0 < b->u0 || tile->v0 < b->v0 || tile->u0 >= b->u0 + b->w || tile->v0 >= b->v0 + b->h ||
            tile->u0 + tile->w > b->u0 + b->w + 1 || tile->v0 + tile->h > b->v0 + b->h + 1)
            continue;
        out->dxt1 = b->texture->dxt1;
        out->width = b->texture->width; out->height = b->texture->height;
        out->u0 = b->u0; out->v0 = b->v0; out->w = b->w; out->h = b->h;
        return 1;
    }
    return 0;
}

void suits_sm1_install(void)
{
    const char *id = setting("suit");
    if (!id || strchr(id, '\\') || strchr(id, '/') || strstr(id, "..")) return;
    load_suit(id);
    if (!g_texture_count && !g_model_file[0]) return;
    g_active = 1;
    rt_drawotag_hook = bind_materials;
    rt_texture_hook = lookup;
    rt_note("suit loaded");
}
