/*
 * Spider-Man's character models on the Xbox: the Dreamcast actors and mod suits'
 * own models served in place of the game's, and the fixes those larger models need.
 * Ports of the PC's (spiderman/patches, RecompOne.Runtime/Assets); a fix to either
 * belongs in both places:
 *
 *   LooseWadOverrides   a WAD entry with a prepared actor in D:\assets\builtin is read
 *                       from that file instead, into an expanded-RAM arena
 *   Costume             which actor answers for spidey.psx (the selected costume's
 *                       Dreamcast actor, or the suit's), and the costume's powers
 *   FramePackets        bounded 512 KiB primitive pools for the larger actors
 *   Sm1AnimationBounds  animation-part count repair for older converted actors
 *   ModelGuard          a model missing after a direct level start, answered
 *   DcModelCompatibility  the PS1 head morph and Bag-Man's nested shell
 *   ArmorTextures       Spider-Armor's texture swap limited to matching pages
 *
 * Not ported: the costume viewer's 20-entry list (Costume.PrepareViewer and its
 * generated-code transform); the retail viewer's ten costumes load their Dreamcast
 * actors. Actors are validated by the Windows-side preparation, not at load.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "runtime.h"
#include "settings.h"
#include "SpiderMan.h"
#include "texpack.h"

#define V0 c->r[2]
#define A0 c->r[4]
#define A1 c->r[5]
#define A2 c->r[6]
#define S0 c->r[16]
#define S2 c->r[18]
#define S4 c->r[20]
#define GP c->r[28]
#define SP c->r[29]
#define RA c->r[31]

#define MODEL_TABLE 0x800A0904u
#define MODEL_STRIDE 64u

/* ---- the expanded-RAM arena (LooseWadOverrides) -------------------------------------- */
#define ARENA_LO 0x80300000u
#define ARENA_HI 0x80780000u
#define BLOCKS 128
typedef struct { uint32_t address, size; } Block;
static Block g_free[BLOCKS], g_used[BLOCKS];
static int g_free_count, g_used_count;
volatile uint32_t rt_arena_peak;   /* highest byte above ARENA_LO ever handed out (status) */

static void arena_reset(void)
{
    g_free[0].address = ARENA_LO; g_free[0].size = ARENA_HI - ARENA_LO;
    g_free_count = 1; g_used_count = 0;
}

static uint32_t arena_alloc(uint32_t requested)
{
    uint32_t size = (requested + 15u) & ~15u;
    for (int i = 0; i < g_free_count; i++) {
        if (g_free[i].size < size || g_used_count >= BLOCKS) continue;
        uint32_t address = g_free[i].address;
        g_free[i].address += size; g_free[i].size -= size;
        if (!g_free[i].size) g_free[i] = g_free[--g_free_count];
        g_used[g_used_count].address = address; g_used[g_used_count].size = size; g_used_count++;
        if (address + size - ARENA_LO > rt_arena_peak) rt_arena_peak = address + size - ARENA_LO;
        return address;
    }
    rt_note("asset arena exhausted");
    return 0;
}

int assets_sm1_allocation(uint32_t address, uint32_t *size)
{
    for (int i = 0; i < g_used_count; i++)
        if (g_used[i].address == address) { if (size) *size = g_used[i].size; return 1; }
    return 0;
}

static int arena_free(uint32_t address)
{
    int u = -1;
    for (int i = 0; i < g_used_count; i++) if (g_used[i].address == address) { u = i; break; }
    if (u < 0) return 0;
    uint32_t start = address, size = g_used[u].size;
    g_used[u] = g_used[--g_used_count];
    for (int i = 0; i < g_free_count; i++)
        if (g_free[i].address + g_free[i].size == start) { start = g_free[i].address; size += g_free[i].size; g_free[i--] = g_free[--g_free_count]; }
        else if (g_free[i].address == start + size) { size += g_free[i].size; g_free[i--] = g_free[--g_free_count]; }
    if (g_free_count < BLOCKS) { g_free[g_free_count].address = start; g_free[g_free_count].size = size; g_free_count++; }
    return 1;
}

/* ---- the actors on the disc ---------------------------------------------------------- */
#define ACTORS 128
static char g_actor_names[ACTORS][16];
static int g_actor_count;

static void load_actor_index(void)
{
    FILE *f = fopen("D:\\assets\\builtin\\index.txt", "r");
    if (!f) return;
    char line[64];
    while (g_actor_count < ACTORS && fgets(line, sizeof line, f)) {
        int n = (int)strcspn(line, "\r\n");
        if (n <= 0 || n >= 16) continue;
        line[n] = 0;
        strcpy(g_actor_names[g_actor_count++], line);
    }
    fclose(f);
}

static int have_actor(const char *name)
{
    for (int i = 0; i < g_actor_count; i++) if (rt_strcasecmp(g_actor_names[i], name) == 0) return 1;
    return 0;
}

/* ---- costume (Costume.cs) ------------------------------------------------------------ */
#define SELECTED 0x800A5704u
static const char *const DREAMCAST_MODELS[10] = {
    "spidey.psx", "sp2099.psx", "spsymbi.psx", "spuniv.psx", "spunlim.psx",
    "spbagman.psx", "spscar.psx", "spreilly.psx", "spquick.psx", "sppark.psx",
};
/* Retail costume-configuration words (func_80047DF8), retail costumes 0..9. */
static const uint32_t ABILITY_CONFIG[10] = {
    0x00202040, 0x00402020, 0x00200000, 0x00404040, 0x00402020,
    0x00403030, 0x00202040, 0x00402020, 0x00200000, 0x00200000,
};
static int g_loaded_costume = -1;     /* SUIT_COSTUME: the suit; else 0..9 */
#define SUIT_COSTUME 100

static int selected_costume(void)
{
    if (suit_active()) return SUIT_COSTUME;
    int retail = (int)mem_r8(SELECTED);
    return retail < 10 ? retail : 0;
}

int assets_sm1_suit_loaded(void) { return g_loaded_costume == SUIT_COSTUME; }

static const char *bundled_model(const char *model)
{
    if (!strcmp(model, "scarlet-spider")) return "spscar.psx";
    if (!strcmp(model, "symbiote")) return "spsymbi.psx";
    if (!strcmp(model, "quick-change")) return "spquick.psx";
    if (!strcmp(model, "peter-parker")) return "sppark.psx";
    if (!strcmp(model, "sm2-spiderman")) return "sp2default.psx";
    return "spidey.psx";
}

static int powers(int costume) { return costume == SUIT_COSTUME ? suit_profile() : costume; }

/* Pre func_80047DF8: retail keeps "costume + 1" at gp+0xA8C for the costume powers. */
int hook_Costume_SetRetailCostume(CpuContext *c)
{
    mem_w32(GP + 0xA8Cu, (uint32_t)powers(selected_costume()) + 1u);
    return 1;
}

/* Post func_80047DF8: the player's costume configuration word. */
void hook_Costume_ApplyAbilityProfile(CpuContext *c)
{
    uint32_t player = V0;
    if ((player & 0xFF000000u) != 0x80000000u) return;
    int p = powers(selected_costume());
    mem_w32(player + 0x584u, ABILITY_CONFIG[p < 10 ? p : 0]);
}

/* ---- the costume viewer's reload (Costume.RunRetailTextureOverlay, pre func_80046B40) -- */
#define TEXTURE_REGISTRY 0x80780000u   /* patch_generated_sm1.py */
#define VIEWER_MODEL_NAME 0x807F0000u

static int model_slot(const char *name)
{
    for (int slot = 0; slot < 40; slot++) {
        uint32_t entry = MODEL_TABLE + (uint32_t)slot * MODEL_STRIDE;
        int i = 0;
        for (; name[i]; i++) if ((mem_r8(entry + (uint32_t)i) & 0xDF) != ((uint8_t)name[i] & 0xDF)) break;
        if (!name[i] && mem_r8(entry + (uint32_t)i) == 0) return slot;
    }
    return -1;
}

/* Drop the registry records of the player model being replaced (TextureRegistry). */
static void remove_model_textures(uint32_t count_address, uint32_t entry, uint32_t model)
{
    uint32_t objects = mem_r32(model + 8u);
    if (objects > 1024u) return;
    uint32_t meshes = mem_r32(model + 12u + objects * 36u);
    if (meshes > 4096u) return;
    uint32_t texture_count_address = mem_r32(entry + 0x0Cu) + meshes * 4u;
    uint32_t textures = mem_r32(texture_count_address);
    if (textures > 4096u) return;
    uint32_t count = mem_r32(count_address), write = 0;
    for (uint32_t read = 0; read < count; read++) {
        uint32_t source = TEXTURE_REGISTRY + read * 8u, texture = mem_r32(source);
        int ours = 0;
        for (uint32_t i = 0; i < textures && !ours; i++) ours = mem_r32(texture_count_address + 4u + i * 4u) == texture;
        if (ours) continue;
        if (write != read) {
            uint32_t to = TEXTURE_REGISTRY + write * 8u;
            mem_w32(to, texture);
            mem_w16(to + 4u, (uint16_t)mem_r16(source + 4u));
            mem_w16(to + 6u, (uint16_t)mem_r16(source + 6u));
        }
        write++;
    }
    mem_w32(TEXTURE_REGISTRY + write * 8u, 0);
    mem_w32(count_address, write);
}

/* The retail viewer lays a costume's textures over the one spidey model; each costume
 * here is its own actor, so reload "spidey" in the same model-cache slot instead. */
int hook_Costume_RunRetailTextureOverlay(CpuContext *c)
{
    int selected = selected_costume();
    int slot = model_slot("spidey");
    if (slot < 0 || g_loaded_costume == selected) return 0;
    uint32_t entry = MODEL_TABLE + (uint32_t)slot * MODEL_STRIDE;
    remove_model_textures(GP + 0xA98u, entry, mem_r32(entry + 0x14u));
    uint32_t saved_ra = RA;
    A0 = (uint32_t)slot; A1 = 1; RA = 0x807C0400u;
    SpiderMan_func_800695D0(c);
    const char *spidey = "spidey";
    for (uint32_t i = 0; i < 7; i++) mem_w8(VIEWER_MODEL_NAME + i, (uint8_t)spidey[i]);
    A0 = VIEWER_MODEL_NAME; A1 = 0; RA = 0x807C0404u;
    SpiderMan_LoadPsx(c);
    RA = saved_ra;
    /* The main LoadLevel marks spidey persistent at +0x0B; the unload cleared it. */
    mem_w8(entry + 0x0Bu, 1);
    rt_note("costume actor reloaded");
    return 0;
}

/* ---- the lookup (OverlayPatches.CdWadFind -> AssetOverrides.Find) -------------------- */
static char g_pending_path[160];
static uint32_t g_pending_bytes, g_pending_rounded;
static int g_pending;

void assets_sm1_find(CpuContext *c, const char *name)
{
    (void)c;
    g_pending = 0;
    char source[24];
    if (strlen(name) >= sizeof source || strchr(name, '\\') || strchr(name, '/')) return;
    strcpy(source, name);
    if (rt_strcasecmp(name, "spidey.psx") == 0) {
        int costume = selected_costume();
        g_loaded_costume = costume;
        if (costume == SUIT_COSTUME && suit_model_file()) {
            snprintf(g_pending_path, sizeof g_pending_path, "%s", suit_model_file());
            goto open;
        }
        strcpy(source, costume == SUIT_COSTUME ? bundled_model(suit_model()) : DREAMCAST_MODELS[costume]);
    }
    if (!have_actor(source)) return;
    snprintf(g_pending_path, sizeof g_pending_path, "D:\\assets\\builtin\\%s", source);
open:;
    FILE *f = fopen(g_pending_path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    fclose(f);
    if (bytes <= 0 || bytes > 2 * 1024 * 1024) return;
    g_pending_bytes = (uint32_t)bytes;
    g_pending_rounded = (g_pending_bytes + 0x7FFu) & ~0x7FFu;
    g_pending = 1;
}

/* Post CdWadFind: the entry's size is the override's. */
void assets_sm1_find_exit(CpuContext *c) { if (g_pending) V0 = g_pending_rounded; }

/* The allocation right after an overridden lookup comes from the arena. */
int assets_sm1_heap_alloc(CpuContext *c)
{
    /* FramePackets: only the two primitive pools in func_80061230. */
    if (A0 == 0x17000u && (RA == 0x80061280u || RA == 0x80061298u)) {
        uint32_t pool = arena_alloc(0x80000u);
        if (pool) { V0 = pool; return 1; }
    }
    if (!g_pending || A0 != g_pending_rounded) return 0;
    uint32_t address = arena_alloc(A0);
    if (!address) return 0;
    V0 = address;
    return 1;
}

int assets_sm1_heap_free(CpuContext *c) { return arena_free(A0); }
int assets_sm1_owns(uint32_t address) { return assets_sm1_allocation(address, 0); }

/* Pre CdWadRead: read the override into the buffer instead of the disc's entry. */
int hook_AssetOverrides_CdWadRead(CpuContext *c)
{
    if (!g_pending) return 1;
    g_pending = 0;
    uint32_t to = A0;
    /* This can run mid-level (a trigger loading an actor): one kernel read straight into
     * PlayStation RAM, not the C library's 1 KB reads and a byte at a time. */
    if (mem_is_ram(to) && mem_is_ram(to + g_pending_rounded - 1u) &&
        (to & (RECOMP_RAM_SIZE - 1u)) + g_pending_rounded <= RECOMP_RAM_SIZE) {
        uint8_t *at = mem_ram(to);
        memset(at, 0, g_pending_rounded);
        HANDLE f = CreateFileA(g_pending_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD got = 0;
            ReadFile(f, at, g_pending_bytes, &got, 0);
            CloseHandle(f);
        }
    } else {
        for (uint32_t i = 0; i < g_pending_rounded; i += 4) mem_w32(to + i, 0);
        FILE *f = fopen(g_pending_path, "rb");
        if (f) {
            uint8_t buffer[2048];
            uint32_t done = 0;
            size_t n;
            while (done < g_pending_bytes && (n = fread(buffer, 1, sizeof buffer, f)) > 0) {
                for (size_t i = 0; i < n && done < g_pending_bytes; i++) mem_w8(to + done++, buffer[i]);
            }
            fclose(f);
        }
    }
    V0 = 0xFFFFFFFFu;
    rt_note("actor served from the disc's assets");
    return 0;
}

/* ---- FramePackets.SetLimit (post func_8002BB9C) -------------------------------------- */
void hook_FramePackets_SetLimit(CpuContext *c)
{
    uint32_t frame = mem_r32(0x800B54A8u), pool = mem_r32(frame + 0x74u);
    if (!assets_sm1_owns(pool)) return;
    /* Retail's 256-byte slack and per-primitive capacity checks stay. */
    mem_w32(GP + 0x7F4u, (pool & 0x7FFFFFFFu) + 0x80000u - 0x100u);
}

/* ---- Sm1AnimationBounds.AfterModelLoad (post func_80068BB0) -------------------------- */
static int find_raw_bank(uint32_t model, uint32_t allocation, uint32_t bank, uint32_t *length)
{
    uint32_t offset = mem_r32(model + 4u);
    if (offset < 12u || offset > allocation - 4u) return 0;
    while (offset <= allocation - 4u) {
        uint32_t tag = mem_r32(model + offset);
        if (tag == 0xFFFFFFFFu || allocation - offset < 8u) return 0;
        uint32_t chunk = mem_r32(model + offset + 4u);
        if (chunk > allocation - offset - 8u) return 0;
        if (model + offset + 8u == bank) { if (tag != 0x2Au) return 0; *length = chunk; return 1; }
        offset += 8u + chunk;
    }
    return 0;
}

static int matches_part_count(uint32_t bank, uint32_t length, uint32_t parts)
{
    if (length < 12u) return 0;
    uint32_t clips = mem_r32(bank);
    if (!clips || clips > (length - 4u) / 8u) return 0;
    uint32_t table_end = 4u + clips * 8u;
    if (mem_r32(bank + 4u) != table_end) return 0;
    for (uint32_t clip = 0; clip < clips; clip++) {
        uint32_t record = bank + 4u + clip * 8u;
        uint32_t start = mem_r32(record), end = clip + 1u < clips ? mem_r32(record + 8u) : length;
        uint32_t frames = mem_r16(record + 4u), stride = mem_r16(record + 6u) + 1u;
        if (start < table_end || end < start || end > length || !frames) return 0;
        uint64_t samples = (frames + stride - 1u) / stride;
        if ((uint64_t)(end - start) != samples * parts * 24u) return 0;
    }
    return 1;
}

void hook_Sm1AnimationBounds_AfterModelLoad(CpuContext *c)
{
    uint32_t entry = V0;
    if (entry < MODEL_TABLE || entry >= MODEL_TABLE + 40u * MODEL_STRIDE || (entry - MODEL_TABLE) % MODEL_STRIDE) return;
    uint32_t model = mem_r32(entry + 20u), allocation;
    if (!assets_sm1_allocation(model, &allocation) || allocation < 12u) return;
    uint32_t parts = mem_r32(model + 8u), cached = mem_r16(entry + 52u);
    const uint32_t max_parts = (0x800A5BB4u - 0x800A58E4u) / 24u;
    if (cached <= parts || !parts || parts > max_parts || mem_r16(model) != 4u || mem_r16(model + 2u) != 2u) return;
    uint32_t bank = mem_r32(entry + 24u), length;
    if (!find_raw_bank(model, allocation, bank, &length) || !matches_part_count(bank, length, parts)) return;
    mem_w16(entry + 52u, (uint16_t)parts);
    rt_note("animation part count corrected");
}

/* ---- ModelGuard (ModelFind) ---------------------------------------------------------- */
static char g_wanted[16];
static uint32_t g_wanted_ptr;

static void read_name(uint32_t address, char *out, int max)
{
    int i = 0;
    for (; i < max - 1; i++) { char ch = (char)mem_r8(address + (uint32_t)i); if (!ch) break; out[i] = ch; }
    out[i] = 0;
}

static int model_index(const char *want)
{
    char name[16];
    for (int i = 0; i < 40; i++) {
        read_name(MODEL_TABLE + (uint32_t)i * MODEL_STRIDE, name, sizeof name);
        if (rt_strcasecmp(name, want) == 0) return i;
    }
    return -1;
}

int hook_ModelGuard_FindEnter(CpuContext *c)
{
    g_wanted_ptr = 0;
    if (!setting("level")) return 1;
    g_wanted_ptr = A0;
    read_name(A0, g_wanted, sizeof g_wanted);
    return 1;
}

void hook_ModelGuard_FindExit(CpuContext *c)
{
    if (!g_wanted_ptr || V0 != 0xFFFFFFFFu || !g_wanted[0]) return;
    int slot = -1;
    char file[24];
    snprintf(file, sizeof file, "%s.psx", g_wanted);
    if (have_actor(file)) {
        /* Load the exact actor through the game's own loader. */
        CpuContext saved = *c;
        A0 = g_wanted_ptr; A1 = 0;
        SpiderMan_LoadPsx(c);
        *c = saved;
        slot = model_index(g_wanted);
    }
    if (slot < 0) {
        /* The counterpart (venom2 <-> venom), else the longest shared name prefix among
         * resident character models (level data names carry an underscore). */
        char alt[16];
        int n = (int)strlen(g_wanted);
        strcpy(alt, g_wanted);
        if (n > 1 && alt[n - 1] >= '0' && alt[n - 1] <= '9') { while (n > 0 && alt[n - 1] >= '0' && alt[n - 1] <= '9') alt[--n] = 0; }
        else if (n < 8) { alt[n] = '2'; alt[n + 1] = 0; }
        if (alt[0]) slot = model_index(alt);
        if (slot < 0 && !strchr(g_wanted, '_')) {
            int best = 0;
            char name[16];
            for (int i = 0; i < 40; i++) {
                read_name(MODEL_TABLE + (uint32_t)i * MODEL_STRIDE, name, sizeof name);
                if (!name[0] || strchr(name, '_')) continue;
                int k = 0;
                while (name[k] && g_wanted[k] && (name[k] | 0x20) == (g_wanted[k] | 0x20)) k++;
                if (k >= 5 && k > best) { best = k; slot = i; }
            }
        }
    }
    if (slot < 0) { rt_note("model missing after a direct level start"); return; }
    V0 = (uint32_t)slot;
    rt_note("model rescued after a direct level start");
}

/* ---- DcModelCompatibility ------------------------------------------------------------ */
/* Post func_800472C0: the menu's PS1 head morph writes 38-vertex targets into mesh 7;
 * a Dreamcast head has other topology, so restore the game's own backup of it. */
void hook_DcModelCompatibility_RestoreHeadAfterPs1Morph(CpuContext *c)
{
    uint32_t slot = mem_r8(0x800B53CDu), entry = MODEL_TABLE + (slot << 6);
    uint32_t pointers = mem_r32(entry + 0x10u);
    if (pointers < 4u || mem_r32(pointers - 4u) <= 7u) return;
    uint32_t head = mem_r32(pointers + 7u * 4u);
    uint32_t vertices = mem_r16(head + 2u);
    if (vertices == 38u) return;
    uint32_t backup = mem_r32(GP + 0xAB4u);
    if (!backup) return;
    for (uint32_t i = 0; i < vertices; i++) {
        uint32_t from = backup + i * 8u, to = head + 0x1Cu + i * 8u;
        mem_w16(to, (uint16_t)mem_r16(from));
        mem_w16(to + 2u, (uint16_t)mem_r16(from + 2u));
        mem_w16(to + 4u, (uint16_t)mem_r16(from + 4u));
    }
}

/* DrawPrimSet: Bag-Man's Dreamcast nested shell (179 head faces, 86 inner ones marked
 * 0x6000 by the converter) gets SM1's per-face ordering-table offset. */
#define BAGMAN_DEPTH 0x1F800344u
static int g_bagman_active;
static uint16_t g_bagman_saved;

int hook_DcModelCompatibility_ApplyBagmanNestedShellDepth(CpuContext *c)
{
    g_bagman_active = 0;
    if (A2 != 179u) return 1;
    uint32_t face = A0;
    int inner = 0, outer = 0;
    for (int i = 0; i < 179; i++) {
        uint16_t flags = (uint16_t)mem_r16(face), length = (uint16_t)mem_r16(face + 2u);
        if (length < 20 || length > 64) return 1;
        if ((flags & 0x6000) == 0x6000) inner++; else outer++;
        face += length;
    }
    if (inner != 86 || outer != 93) return 1;
    g_bagman_saved = (uint16_t)mem_r16(BAGMAN_DEPTH);
    mem_w16(BAGMAN_DEPTH, 128);
    g_bagman_active = 1;
    return 1;
}

void hook_DcModelCompatibility_RestoreBagmanNestedShellDepth(CpuContext *c)
{
    (void)c;
    if (!g_bagman_active) return;
    mem_w16(BAGMAN_DEPTH, g_bagman_saved);
    g_bagman_active = 0;
}

/* ---- ArmorTextures.SkipUnmapped (instruction branch at 80046FD4) --------------------- */
static int armor_record(CpuContext *c, uint32_t armor_hash, int *four_bit, int *bytes)
{
    uint32_t hashes = mem_r32(SP + 0x28u), section = mem_r32(SP + 0x2Cu);
    int count = (int)mem_r32(section);
    if (count <= 0 || count > 64) return 0;
    uint32_t record = section + (uint32_t)count * 4u + 4u;
    for (uint32_t i = 0; i < (uint32_t)count; i++) {
        if (mem_r32(hashes + mem_r32(record + 0xCu) * 4u + 4u) == armor_hash) {
            uint32_t size = mem_r32(record + 0x10u);
            int texels = (int)(size & 0xFFFF) * (int)(size >> 16);
            *four_bit = mem_r32(record + 0x4u) == 0x10u;
            *bytes = *four_bit ? texels / 2 : texels;
            return 1;
        }
        record += mem_r32(section + 8u + i * 4u) - mem_r32(section + 4u + i * 4u);
    }
    return 0;
}

/* Returns 1 to skip this texture (resume at 800471B0): no armor version, a different
 * page size or depth, or a suit (which keeps its own look). */
int hook_ArmorTextures_SkipUnmapped(CpuContext *c)
{
    uint32_t descriptor = S0, hash = mem_r32(descriptor + 0x14u), armor_hash = 0;
    for (uint32_t i = 0; i < 13 && !armor_hash; i++)
        if (mem_r32(0x80099658u + i * 8u) == hash) armor_hash = mem_r32(0x80099658u + i * 8u + 4u);
    int four_bit = ((mem_r16(descriptor + 0x6u) >> 7) & 3u) == 0;
    int width = (int16_t)mem_r16(S2);
    int words = four_bit ? (width + 3) >> 2 : (width + 1) >> 1;
    int bytes = words * 2 * (int16_t)mem_r16(S4);
    int armor_four_bit, armor_bytes;
    if (suit_active() || !armor_hash || !armor_record(c, armor_hash, &armor_four_bit, &armor_bytes) ||
        armor_four_bit != four_bit || armor_bytes != bytes)
        return 1;
    return 0;
}

void assets_sm1_install(void)
{
    arena_reset();
    load_actor_index();
    if (texpack_load("D:\\assets\\packs\\dreamcast-sm1-actors")) rt_note("Dreamcast texture pack loaded");
}
