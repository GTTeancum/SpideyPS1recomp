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
 * The costume viewer (Costume.PrepareViewer) lists twenty stock costumes and the mod
 * suits (with tools/patch_generated_sm1.py); each costume is its own Dreamcast actor.
 * Actors are validated by the Windows-side preparation, not at load.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "runtime.h"
#include "guest_layout.h"
#include "settings.h"
#include "SpiderMan.h"
#include "texpack.h"
#include "wad_overrides.h"

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

/* The expanded-RAM arena and the prepared actors: src/wad_overrides.c (shared). */

/* ---- costume (Costume.cs) ------------------------------------------------------------ */
/* The costume viewer lists twenty stock costumes -- retail's ten and ten of SM2's
 * Dreamcast actors -- then the mod suits (VIEWER_ROWS in all). The choice is a word at
 * 0x800A5704 in the block the game saves; retail reads only its low byte, which several
 * shell tables index and so stays a 0..9 proxy. The full 0..19 choice is kept in the
 * word's padding (+1, with the signature "S2" at +2); a mod suit's identity is kept on
 * the Xbox (suits.c), never as a catalogue index in a save. */
#define SELECTED 0x800A5704u
#define EXTENDED (SELECTED + 1u)
#define EXTENDED_MARKER (SELECTED + 2u)
#define EXTENDED_MAGIC 0x3253u
#define UNLOCKS 0x800A5708u
#define LEGACY_MARKER 0x80000000u     /* the first 20-costume version's saves */
#define LEGACY_MASK 0x1F000000u
#define ORIGINAL 10
#define STOCK 20
static const char *const DREAMCAST_MODELS[STOCK] = {
    "spidey.psx", "sp2099.psx", "spsymbi.psx", "spuniv.psx", "spunlim.psx",
    "spbagman.psx", "spscar.psx", "spreilly.psx", "spquick.psx", "sppark.psx",
    "sp2phoenix.psx", "sp2prodigy.psx", "sp2dusk.psx", "sp2insulated.psx",
    "sp2rossred.psx", "sp2rosswhite.psx", "sp2venomx.psx", "sp2negative.psx",
    "sp2battle.psx", "sp2default.psx",
};
static const char *const VIEWER_NAMES[STOCK] = {
    "spider-man", "Spider-man 2099", "symbiote spider-man", "Captain Universe",
    "Spidey unlimited", "Amazing bag man", "Scarlet Spidey", "Ben Reilly",
    "Quick Change Spidey", "Peter Parker", "Spider-Phoenix", "Prodigy", "Dusk",
    "Insulated Suit", "Alex Ross - Red", "Alex Ross - White",
    "Venom 2 - Earth X", "Negative Zone", "Battle Damaged", "Spider-Man - SM2",
};
static const char *const IMPORTED_POWER_TEXT[STOCK - ORIGINAL][3] = {
    { "INVULNERABLE", "ENHANCED STRENGTH", "UNLIMITED WEBBING" }, { "NO SPIDEY BELT" },
    { "STEALTH MODE" }, { "ENHANCED STRENGTH" }, { "STANDARD POWERS" }, { "STANDARD POWERS" },
    { "UNLIMITED WEBBING" }, { "NO SPIDEY BELT" }, { "STANDARD POWERS" }, { "STANDARD POWERS" },
};
static const char *const IMPORTED_UNLOCK_TEXT[STOCK - ORIGINAL] = {
    "CAPTAIN UNIVERSE", "AMAZING BAG MAN", "SPIDEY UNLIMITED", "SPIDER-MAN 2099",
    "SCARLET SPIDEY", "BEN REILLY", "SYMBIOTE SPIDER-MAN", "QUICK CHANGE SPIDEY",
    "PETER PARKER", "START",
};
/* SuitManifest.PowerText: the GAME POWERS lines of SM1's ten profiles (charbio.dat). */
static const char *const POWER_TEXT[ORIGINAL][4] = {
    { "SUPER STRENGTH", "SUPER AGILITY", "STICK TO WALLS", "SPIDER SENSE" },
    { "ENHANCED STRENGTH" }, { "UNLIMITED WEBBING" },
    { "INVULNERABLE", "ENHANCED STRENGTH", "UNLIMITED WEBBING" },
    { "STEALTH MODE" }, { "NO SPIDEY BELT" }, { "NONE" }, { "NONE" },
    { "NO SPIDEY BELT" }, { "NO SPIDEY BELT" },
};
/* Where each imported costume's powers were copied from (never Peter Parker, 9). */
static const uint8_t ABILITY_DONORS[STOCK] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 3, 5, 4, 1, 6, 7, 2, 8, 0, 0 };
/* Retail costume-configuration words (func_80047DF8), and the imported costumes'. */
static const uint32_t ABILITY_CONFIG[STOCK] = {
    0x00202040, 0x00402020, 0x00200000, 0x00404040, 0x00402020,
    0x00403030, 0x00202040, 0x00402020, 0x00200000, 0x00200000,
    0x00404040, 0x00403030, 0x00402020, 0x00402020, 0x00202040,
    0x00402020, 0x00200000, 0x00200000, 0x00202040, 0x00202040,
};
/* An original costume's unlock also unlocks its imported partner. */
static const int8_t UNLOCK_PARTNER[ORIGINAL] = { -1, 13, 16, 10, 12, 11, 14, 15, 17, 18 };

static int g_loaded_costume = -1;     /* the costume whose actor spidey.psx answered with */

/* Costume.ReadSelected: 0..19 a stock costume, 20.. a mod suit. */
static int selected_costume(void)
{
    if (suit_active()) return STOCK + suits_selected();
    uint32_t extended = mem_r8(EXTENDED);
    if (mem_r16(EXTENDED_MARKER) == EXTENDED_MAGIC && extended < STOCK) return (int)extended;
    uint32_t bits = mem_r32(UNLOCKS), retail = mem_r8(SELECTED);
    uint32_t legacy = (bits & LEGACY_MASK) >> 24;
    if ((bits & LEGACY_MARKER) && legacy < STOCK && retail == (legacy < ORIGINAL ? legacy : 0)) return (int)legacy;
    return retail < ORIGINAL ? (int)retail : 0;
}

/* Costume.WriteSelected. */
static void write_selected(int selected)
{
    if (selected >= STOCK) {
        if (suits_select(selected - STOCK, 1)) {
            /* A suit reskins the Dreamcast default: its proxy stays 0 (its powers are
             * applied by ApplyAbilityProfile), and 0 is the fallback if it is removed. */
            mem_w8(EXTENDED, 0);
            mem_w16(EXTENDED_MARKER, EXTENDED_MAGIC);
            mem_w8(SELECTED, 0);
            return;
        }
        selected = 0;
    }
    suits_select(-1, 1);
    if (selected < 0 || selected >= STOCK) selected = 0;
    mem_w8(EXTENDED, (uint8_t)selected);
    mem_w16(EXTENDED_MARKER, EXTENDED_MAGIC);
    mem_w8(SELECTED, (uint8_t)(selected < ORIGINAL ? selected : 0));
}

/* Costume.ApplyPersistentState: once a frame (a save loaded overwrites the block), the
 * paired unlocks and the selection re-applied. */
static void persistent_state(void)
{
    uint32_t bits = mem_r32(UNLOCKS), retail = mem_r8(SELECTED);
    if (retail >= ORIGINAL) retail = 0;
    int selected = (int)retail;
    uint32_t extended = mem_r8(EXTENDED);
    if (mem_r16(EXTENDED_MARKER) == EXTENDED_MAGIC && extended < STOCK) selected = (int)extended;
    else {
        uint32_t legacy = (bits & LEGACY_MASK) >> 24;
        if ((bits & LEGACY_MARKER) && legacy < STOCK && retail == (legacy < ORIGINAL ? legacy : 0)) {
            selected = (int)legacy;
            bits &= ~(LEGACY_MARKER | LEGACY_MASK);
        }
    }
    bits |= 1u | (1u << 19);
    for (int original = 1; original < ORIGINAL; original++)
        if (bits & (1u << original)) bits |= 1u << UNLOCK_PARTNER[original];
    mem_w32(UNLOCKS, bits);
    write_selected(suit_active() ? STOCK + suits_selected() : selected);
}

static void (*g_next_frame_hook)(void);
static void frame(void)
{
    persistent_state();
    if (g_next_frame_hook) g_next_frame_hook();
}

/* The loaded spidey is the chosen suit's own (its materials bind). */
int assets_sm1_suit_loaded(void) { return suit_active() && g_loaded_costume == STOCK + suits_selected(); }

/* ---- the viewer (Costume.PrepareViewer; tools/patch_generated_sm1.py) ---------------- */
#define VIEWER_TABLE GUEST_VIEWER_TABLE
#define VIEWER_NAMES_AT (VIEWER_TABLE + 0x8000u)
#define VIEWER_TEXT (VIEWER_TABLE + 0x10000u)
#define VIEWER_END GUEST_VIEWER_END
#define RETAIL_TABLE 0x802693A8u        /* shell's ten 12-byte rows */

uint32_t costume_sm1_viewer_count(void)
{
    int n = STOCK + suits_count();
    return (uint32_t)(n < VIEWER_ROWS ? n : VIEWER_ROWS);
}

int costume_sm1_is_unlocked(uint32_t index)
{
    return index < costume_sm1_viewer_count() && (index >= STOCK || (mem_r32(UNLOCKS) & (1u << index)) != 0);
}

uint32_t costume_sm1_read_selected(void) { return (uint32_t)selected_costume(); }

void costume_sm1_write_selected(int selected)
{
    if ((uint32_t)selected < costume_sm1_viewer_count()) write_selected(selected);
}

/* One NUL-terminated line, or 0 when it would pass `end` (nothing written). */
static uint32_t viewer_line(uint32_t at, const char *text, uint32_t end)
{
    uint32_t n = (uint32_t)strlen(text);
    if (!at || at + n + 1 > end) return 0;
    for (uint32_t i = 0; i < n; i++) mem_w8(at + i, (uint8_t)text[i]);
    mem_w8(at + n, 0);
    return at + n + 1;
}

/* A colour token: 2, then charbio.dat's heading (105,105,0) or body (68,68,100) colour. */
static uint32_t viewer_colour(uint32_t at, int heading, uint32_t end)
{
    if (!at || at + 4 > end) return 0;
    mem_w8(at, 2);
    mem_w8(at + 1, heading ? 105 : 68);
    mem_w8(at + 2, heading ? 105 : 68);
    mem_w8(at + 3, heading ? 0 : 100);
    return at + 4;
}

static void upper(char *out, size_t size, const char *in)
{
    size_t k = 0;
    for (; in[k] && k < size - 1; k++) out[k] = (char)((in[k] >= 'a' && in[k] <= 'z') ? in[k] - 32 : in[k]);
    out[k] = 0;
}

/* SuitManifest.WrapComments: upper case, 18-character lines, long words split. */
static uint32_t viewer_comments(uint32_t at, const char *text, uint32_t end)
{
    char word[256], line[24] = "";
    const char *p = text;
    while (at && *p) {
        while (*p == ' ') p++;
        if (!*p) break;
        size_t n = 0;
        while (p[n] && p[n] != ' ' && n < sizeof word - 1) n++;
        for (size_t i = 0; i < n; i++) word[i] = (char)((p[i] >= 'a' && p[i] <= 'z') ? p[i] - 32 : p[i]);
        word[n] = 0;
        p += n;
        const char *rest = word;
        size_t len = strlen(line);
        if (len && len + strlen(rest) + 1 > 18) { at = viewer_line(at, line, end); line[0] = 0; len = 0; }
        while (at && strlen(rest) > 18) {
            char piece[19];
            memcpy(piece, rest, 18); piece[18] = 0;
            at = viewer_line(at, piece, end);
            rest += 18;
        }
        if (len) strcat(line, " ");
        strcat(line, rest);
    }
    if (at && line[0]) at = viewer_line(at, line, end);
    return at;
}

/* Pre shell:func_80261C70: the retail ten rows verbatim, then ten imported costumes and
 * the mod suits, each with its own name and description. */
int hook_Costume_PrepareViewer(CpuContext *c)
{
    (void)c;
    persistent_state();
    for (uint32_t i = 0; i < ORIGINAL * 12u; i += 4) mem_w32(VIEWER_TABLE + i, mem_r32(RETAIL_TABLE + i));
    uint32_t name = VIEWER_NAMES_AT, text = VIEWER_TEXT, count = costume_sm1_viewer_count();
    for (uint32_t i = ORIGINAL; i < count; i++) {
        int mod = i >= STOCK, s = (int)i - STOCK;
        const char *display = mod ? suits_name(s) : VIEWER_NAMES[i];
        char caps[48];
        upper(caps, sizeof caps, display);
        uint32_t n = viewer_line(name, display, VIEWER_TEXT), t = text;
        t = viewer_colour(t, 1, VIEWER_END);
        t = viewer_line(t, mod ? "COSTUME:" : "SPECIAL COSTUME", VIEWER_END);
        t = viewer_colour(t, 0, VIEWER_END);
        t = viewer_line(t, caps, VIEWER_END);
        t = viewer_colour(t, 1, VIEWER_END);
        t = viewer_line(t, "GAME POWERS:", VIEWER_END);
        t = viewer_colour(t, 0, VIEWER_END);
        const char *const *powers_text = mod ? POWER_TEXT[suits_profile(s) < ORIGINAL ? suits_profile(s) : 0]
                                             : IMPORTED_POWER_TEXT[i - ORIGINAL];
        for (int j = 0; j < (mod ? 4 : 3) && powers_text[j]; j++) t = viewer_line(t, powers_text[j], VIEWER_END);
        t = viewer_colour(t, 1, VIEWER_END);
        t = viewer_line(t, mod ? "COMMENTS:" : "UNLOCKED WITH:", VIEWER_END);
        t = viewer_colour(t, 0, VIEWER_END);
        t = mod ? viewer_comments(t, suits_comments(s), VIEWER_END) : viewer_line(t, IMPORTED_UNLOCK_TEXT[i - ORIGINAL], VIEWER_END);
        /* Bounded: a row that does not fit is left out (and the ones after it). */
        if (!n || !t || t + 1 > VIEWER_END) { rt_note("costume viewer text arena full"); break; }
        mem_w8(t++, 255);
        uint32_t row = VIEWER_TABLE + i * 12u;
        mem_w32(row, name);
        mem_w32(row + 4, text);
        mem_w32(row + 8, 0x004C0008u);      /* retail default Spider-Man's style */
        name = n; text = t;
    }
    return 1;
}

/* The retail constructor sets up forty inline 28-byte rows; this viewer's list was
 * allocated for VIEWER_ROWS (the transform), so the rest are set up here alike. */
void costume_sm1_configure_viewer_list(uint32_t list)
{
    for (uint32_t row = 40; row < VIEWER_ROWS; row++) {
        uint32_t target = list + 0x28u + row * 28u;
        for (uint32_t o = 0; o < 28; o++) mem_w8(target + o, 0);
        mem_w16(target + 8, (uint16_t)mem_r16(list + 0x30u));
        mem_w16(target + 10, (uint16_t)mem_r16(list + 0x32u));
        for (uint32_t o = 14; o < 26; o++) mem_w8(target + o, (uint8_t)mem_r8(list + 0x28u + o));
    }
    mem_w32(list + 0x20u, 70);     /* the right column's first text line */
    mem_w8(list + 0x15u, 11);      /* last baseline 170, inside the frame ending at 175 */
}

/* The list frame aligned with the description frame (shell:func_80261C70's right panel). */
void costume_sm1_align_viewer_frame(uint32_t list)
{
    uint32_t f = mem_r32(list + 4u);
    if (!f) return;
    mem_w32(f + 0x20u, 0x3Au);
    mem_w32(f + 0x10u, 0x75u);
}

static const char *bundled_model(const char *model)
{
    if (!strcmp(model, "scarlet-spider")) return "spscar.psx";
    if (!strcmp(model, "symbiote")) return "spsymbi.psx";
    if (!strcmp(model, "quick-change")) return "spquick.psx";
    if (!strcmp(model, "peter-parker")) return "sppark.psx";
    if (!strcmp(model, "sm2-spiderman")) return "sp2default.psx";
    return "spidey.psx";
}

/* Costume.RetailPowers: the retail costume (0..9) whose powers a costume has. */
static int retail_powers(int costume)
{
    if (costume >= STOCK) return suit_profile() < ORIGINAL ? suit_profile() : 0;
    return costume < ORIGINAL ? costume : ABILITY_DONORS[costume];
}

/* Pre func_80047DF8: retail keeps "costume + 1" at gp+0xA8C for the costume powers. */
int hook_Costume_SetRetailCostume(CpuContext *c)
{
    mem_w32(GP + 0xA8Cu, (uint32_t)retail_powers(selected_costume()) + 1u);
    return 1;
}

/* Post func_80047DF8: the player's costume configuration word. */
void hook_Costume_ApplyAbilityProfile(CpuContext *c)
{
    uint32_t player = V0;
    if ((player & 0xFF000000u) != 0x80000000u) return;
    int costume = selected_costume();
    if (costume >= STOCK) costume = suit_profile() < ORIGINAL ? suit_profile() : 0;
    mem_w32(player + 0x584u, ABILITY_CONFIG[costume]);
}

/* ---- the costume viewer's reload (Costume.RunRetailTextureOverlay, pre func_80046B40) -- */
#define TEXTURE_REGISTRY GUEST_TEXTURE_REGISTRY
#define VIEWER_MODEL_NAME GUEST_SCRATCH_NAMES

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
void assets_sm1_find(CpuContext *c, const char *name)
{
    (void)c;
    char source[24];
    if (strlen(name) >= sizeof source) { wad_find(name, 0); return; }
    strcpy(source, name);
    if (rt_strcasecmp(name, "spidey.psx") == 0) {
        int costume = selected_costume();
        g_loaded_costume = costume;
        if (costume >= STOCK && suit_model_file()) { wad_find_file(suit_model_file()); return; }
        strcpy(source, costume >= STOCK ? bundled_model(suit_model()) : DREAMCAST_MODELS[costume]);
    }
    wad_find(name, source);
}

/* Post CdWadFind: the entry's size is the override's. */
void assets_sm1_find_exit(CpuContext *c) { wad_find_exit(c); }

/* The allocation right after an overridden lookup comes from the arena. */
int assets_sm1_heap_alloc(CpuContext *c)
{
    /* FramePackets: only the two primitive pools in func_80061230. */
    if (A0 == 0x17000u && (RA == 0x80061280u || RA == 0x80061298u)) {
        uint32_t pool = wad_scratch(0x80000u);
        if (pool) { V0 = pool; return 1; }
    }
    return wad_heap_alloc(c);
}

int assets_sm1_heap_free(CpuContext *c) { return wad_free(A0); }
int assets_sm1_owns(uint32_t address) { return wad_owns(address); }
int assets_sm1_allocation(uint32_t address, uint32_t *size) { return wad_allocation(address, size); }

/* Pre CdWadRead: read the override into the buffer instead of the disc's entry. */
int hook_AssetOverrides_CdWadRead(CpuContext *c) { return !wad_read(c); }

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
    if (wad_have(file)) {
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

/* DcModelCompatibility's Bag-Man shell depth and the ArmorTextures test are shared:
 * src/patches_common.c. */
/* ---- ArmorTextures.SkipUnmapped (instruction branch at 80046FD4, resume 800471B0) --- */
int hook_ArmorTextures_SkipUnmapped(CpuContext *c) { return armor_skip_unmapped(c, 0x80099658u, 13); }

void assets_sm1_install(void)
{
    wad_init();
    g_next_frame_hook = rt_frame_hook;   /* widescreen's, set up before */
    rt_frame_hook = frame;
    if (texpack_load("D:\\assets\\packs\\dreamcast-sm1-actors")) rt_note("Dreamcast texture pack loaded");
}
