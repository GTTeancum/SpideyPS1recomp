/*
 * Mod suits, shared by both games: a port of the PC's SuitMods.cs (catalogue,
 * selection, material binding) and ActorMaterialCache (spiderman/patches,
 * spiderman2/patches, RecompOne.Runtime/Assets/Textures). A fix to either belongs in
 * both places.
 *
 * Catalogue: every D:\mods\suits\<id>\suit.txt (made from the PC suit by
 * xbox/tools/prep_suits.py), listed by name as the PC lists them, after the game's
 * stock costumes in its costume viewer (assets_sm1.c, costume_sm2.c). Choosing one
 * there (suits_select) loads its textures and rig and drops the previous suit's; the
 * choice is kept in the save folder (suit-sm1.txt / suit-sm2.txt, the PC's
 * selected-suit.txt) and restored at start. suit=<id> in D:\openspidey.ini chooses one
 * for a test run instead (the PC's SPIDEY_COSTUME), without keeping it.
 *
 * A reskin keeps the stock Spider-Man model and replaces its textures. The loaded
 * model ("spidey" in the game's actor table: SM1 0x800A0904, SM2 0x800ACED8) lists its
 * materials: a texture page, CLUT and UV rectangle each, and an id (+20) that the
 * suit's textures are keyed by. Each ordering table, the materials whose id the suit
 * replaces are bound; a textured primitive whose texels lie inside a bound material
 * then samples the suit's texture over that rectangle instead. Materials bind only
 * while the suit's model is the one loaded (the game's predicate).
 *
 * The suit also names the bundled model it reskins, its stock powers (SM1's index and
 * by name, which SM2 looks up in its own list) and may bring its own model (modelfile)
 * with an animation rig (src/rig.c); the game's costume code serves them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "card.h"
#include "files.h"
#include "hwgpu.h"
#include "rig.h"
#include "runtime.h"
#include "settings.h"

static uint32_t g_actors;               /* the game's actor table */
static int (*g_suit_loaded)(void);      /* whether the loaded spidey is the suit's */
static char g_tag[8];                   /* "sm1" / "sm2": the kept choice's file */
#define MAX_TEXTURES 32
#define MAX_BINDINGS 64

/* ---- the catalogue ------------------------------------------------------------------ */
typedef struct {
    char id[48], name[48], model[24], profile_name[24], comments[160];
    int profile;
} SuitEntry;
static SuitEntry g_suits[SUITS_MAX];
static int g_suit_count;

static void read_entry(const char *id, SuitEntry *e)
{
    memset(e, 0, sizeof *e);
    snprintf(e->id, sizeof e->id, "%s", id);
    strcpy(e->model, "spiderman");
    strcpy(e->profile_name, "spiderman");
    snprintf(e->name, sizeof e->name, "%s", id);
    char path[160], line[200];
    static char text[4096];
    snprintf(path, sizeof path, "D:\\mods\\suits\\%s\\suit.txt", id);
    if (rt_file_text(path, text, sizeof text) < 0) return;
    for (const char *cursor = text; rt_next_line(&cursor, line, sizeof line);) {
        int profile;
        if (!strncmp(line, "name ", 5)) snprintf(e->name, sizeof e->name, "%s", line + 5);
        else if (!strncmp(line, "comments ", 9)) snprintf(e->comments, sizeof e->comments, "%s", line + 9);
        else if (!strncmp(line, "model ", 6)) sscanf(line + 6, "%23s", e->model);
        else if (!strncmp(line, "profilename ", 12)) sscanf(line + 12, "%23s", e->profile_name);
        else if (sscanf(line, "profile %d", &profile) == 1 && profile >= 0 && profile < 10) e->profile = profile;
    }
}

static int by_name(const void *a, const void *b)
{
    const SuitEntry *x = a, *y = b;
    int c = rt_strcasecmp(x->name, y->name);
    return c ? c : strcmp(x->id, y->id);
}

static void scan(int capacity)
{
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA("D:\\mods\\suits\\*", &found);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || found.cFileName[0] == '.') continue;
        if (strlen(found.cFileName) >= sizeof g_suits[0].id || g_suit_count >= capacity) continue;
        char path[160];
        snprintf(path, sizeof path, "D:\\mods\\suits\\%s\\suit.txt", found.cFileName);
        if (rt_file_size(path) < 0) continue;
        read_entry(found.cFileName, &g_suits[g_suit_count++]);
    } while (FindNextFileA(h, &found));
    FindClose(h);
    qsort(g_suits, (size_t)g_suit_count, sizeof g_suits[0], by_name);
}

int suits_count(void) { return g_suit_count; }
const char *suits_name(int i) { return i >= 0 && i < g_suit_count ? g_suits[i].name : ""; }
const char *suits_comments(int i) { return i >= 0 && i < g_suit_count ? g_suits[i].comments : ""; }
int suits_profile(int i) { return i >= 0 && i < g_suit_count ? g_suits[i].profile : 0; }
const char *suits_profile_name(int i) { return i >= 0 && i < g_suit_count ? g_suits[i].profile_name : "spiderman"; }

/* ---- the selected suit's textures ------------------------------------------------------ */
typedef struct { uint32_t id; void *dxt1; int width, height; } SuitTexture;
static SuitTexture g_textures[MAX_TEXTURES];
static int g_texture_count;

/* Textures of a suit switched away from: the GPU may still read them for up to three
 * frames (hwgpu.c's in_flight), so they are freed once that has passed. */
#define RETIRED_MAX (MAX_TEXTURES * 2)
static struct { void *dxt1; uint32_t bytes, frame; } g_retired[RETIRED_MAX];
static int g_retired_count;

/* all=1 only after hw_finish. */
static void free_retired(int all)
{
    uint32_t now = hw_frame_number();
    for (int i = 0; i < g_retired_count; i++) {
        if (!all && now < g_retired[i].frame + 4u) continue;
        MmFreeContiguousMemory(g_retired[i].dxt1);
        rt_mem_add(MEM_SUITS, -(int32_t)g_retired[i].bytes);
        g_retired[i--] = g_retired[--g_retired_count];
    }
}

static void *load_dds(const char *path, int *width, int *height)
{
    uint8_t header[128];
    void *pixels = 0;
    if (rt_file_read(path, 0, header, sizeof header) == (long)sizeof header && memcmp(header, "DDS ", 4) == 0 &&
        memcmp(header + 84, "DXT1", 4) == 0) {
        uint32_t h, w;
        memcpy(&h, header + 12, 4);
        memcpy(&w, header + 16, 4);
        uint32_t bytes = w * h / 2;
        if (w >= 4 && h >= 4 && w <= 1024 && h <= 1024 && !(w & (w - 1)) && !(h & (h - 1))) {
            pixels = MmAllocateContiguousMemoryEx(bytes, 0, 0x03ffafff, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
            if (pixels && rt_file_read(path, sizeof header, pixels, bytes) != (long)bytes) { MmFreeContiguousMemory(pixels); pixels = 0; }
            if (pixels) rt_mem_add(MEM_SUITS, (int32_t)bytes);
            /* Write-combined: drain the CPU's writes before the GPU is pointed at them. */
            __builtin_ia32_sfence();
            *width = (int)w; *height = (int)h;
        }
    }
    return pixels;
}

static int g_active = -1;               /* catalogue index, or -1: the stock costumes */
static char g_model_file[160], g_rig_file[160];

int suit_active(void) { return g_active >= 0; }
int suits_selected(void) { return g_active; }
const char *suit_model(void) { return g_active >= 0 ? g_suits[g_active].model : "spiderman"; }
int suit_profile(void) { return g_active >= 0 ? g_suits[g_active].profile : 0; }
const char *suit_profile_name(void) { return g_active >= 0 ? g_suits[g_active].profile_name : "spiderman"; }
const char *suit_model_file(void) { return g_active >= 0 && g_model_file[0] ? g_model_file : 0; }
const char *suit_rig_file(void) { return g_active >= 0 && g_rig_file[0] ? g_rig_file : 0; }

static void keep_choice(void)
{
    const char *dir = card_dir();
    if (!dir) return;
    char path[160];
    snprintf(path, sizeof path, "%s\\suit-%s.txt", dir, g_tag);
    const char *id = g_active >= 0 ? g_suits[g_active].id : "";
    rt_file_write(path, 0, id, (uint32_t)strlen(id), 1);
}

static void unbind(void);

int suits_select(int index, int keep)
{
    if (index < 0 || index >= g_suit_count) index = -1;
    if (index == g_active) return 1;
    /* Retire the current suit's textures and rig. */
    for (int i = 0; i < g_texture_count; i++) {
        if (g_retired_count >= RETIRED_MAX) { hw_finish(); free_retired(1); }   /* full: the GPU is done with them first */
        g_retired[g_retired_count].dxt1 = g_textures[i].dxt1;
        g_retired[g_retired_count].bytes = (uint32_t)(g_textures[i].width * g_textures[i].height / 2);
        g_retired[g_retired_count].frame = hw_frame_number();
        g_retired_count++;
    }
    g_texture_count = 0;
    unbind();               /* the bindings pointed at the list just emptied */
    g_model_file[0] = g_rig_file[0] = 0;
    g_active = -1;
    rig_unload();
    if (index >= 0) {
        const char *id = g_suits[index].id;
        char path[160], line[160];
        static char text[4096];
        snprintf(path, sizeof path, "D:\\mods\\suits\\%s\\suit.txt", id);
        if (rt_file_text(path, text, sizeof text) >= 0) {
            for (const char *cursor = text; rt_next_line(&cursor, line, sizeof line);) {
                unsigned key;
                char file[64];
                /* "modelfile" before "model": sscanf's "model %s" also matches "modelfile x". */
                if (sscanf(line, "modelfile %63s", file) == 1) { snprintf(g_model_file, sizeof g_model_file, "D:\\mods\\suits\\%s\\%s", id, file); continue; }
                if (sscanf(line, "rig %63s", file) == 1) { snprintf(g_rig_file, sizeof g_rig_file, "D:\\mods\\suits\\%s\\%s", id, file); continue; }
                if (g_texture_count >= MAX_TEXTURES || sscanf(line, "texture %x %63s", &key, file) != 2) continue;
                char dds[200];
                snprintf(dds, sizeof dds, "D:\\mods\\suits\\%s\\%s", id, file);
                SuitTexture *t = &g_textures[g_texture_count];
                t->dxt1 = load_dds(dds, &t->width, &t->height);
                if (!t->dxt1) { rt_note("suit texture failed to load"); continue; }
                t->id = key;
                g_texture_count++;
            }
        }
        if (g_texture_count || g_model_file[0]) {
            g_active = index;
            if (g_rig_file[0]) rig_install(g_rig_file);
            rt_log("suit %s selected", id);
        } else rt_log("suit %s: nothing to load", id);
    } else rt_log("stock costumes");
    if (keep) keep_choice();
    return index < 0 || g_active == index;
}

static const SuitTexture *texture_for(uint32_t id)
{
    for (int i = 0; i < g_texture_count; i++) if (g_textures[i].id == id) return &g_textures[i];
    return 0;
}

/* ---- binding (ActorMaterialCache.Build) --------------------------------------------- */
typedef struct { int page_x, page_y, depth, clut_x, clut_y, u0, v0, w, h; const SuitTexture *texture; } Binding;
static Binding g_bindings[MAX_BINDINGS];
static int g_binding_count;
/* The texture pages (32 x 3 depths) the bindings use: a world triangle on any other
 * page is answered without going through the list. */
static uint32_t g_binding_pages[3];
static int page_bit(int page_x, int page_y) { return (page_x / 64) + (page_y / 256) * 16; }

static void unbind(void)
{
    g_binding_count = 0;
    g_binding_pages[0] = g_binding_pages[1] = g_binding_pages[2] = 0;
}

static int ram(uint32_t p, uint32_t size) { return p >= 0x80000000u && p <= 0x80800000u - size; }

static void bind_materials(void)
{
    if (g_retired_count) free_retired(0);
    unbind();
    if (!g_texture_count) return;
    uint32_t entry = 0;
    for (uint32_t i = 0; i < 40; i++) {
        uint32_t p = g_actors + i * 64u;
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
    if (!g_suit_loaded || !g_suit_loaded()) return 0;
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

static int find(const char *id)
{
    for (int i = 0; i < g_suit_count; i++) if (!rt_strcasecmp(g_suits[i].id, id)) return i;
    return -1;
}

void suits_install(uint32_t actor_table, int (*suit_loaded)(void), const char *game_tag, int capacity)
{
    g_actors = actor_table;
    g_suit_loaded = suit_loaded;
    snprintf(g_tag, sizeof g_tag, "%s", game_tag);
    scan(capacity < SUITS_MAX ? capacity : SUITS_MAX);
    rt_drawotag_hook = bind_materials;
    rt_texture_hook = lookup;
    /* A test run's suit=<id>, else the kept choice. */
    const char *id = setting("suit");
    if (id) { suits_select(find(id), 0); return; }
    const char *dir = card_dir();
    if (!dir) return;
    char path[160], kept[64] = "";
    snprintf(path, sizeof path, "%s\\suit-%s.txt", dir, g_tag);
    if (rt_file_text(path, kept, sizeof kept) < 0) return;
    kept[strcspn(kept, "\r\n")] = 0;
    if (kept[0]) suits_select(find(kept), 0);
}
