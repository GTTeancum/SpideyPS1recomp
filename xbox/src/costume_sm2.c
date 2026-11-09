/*
 * Spider-Man 2's costumes and character models on the Xbox: ports of the PC's
 * spiderman2/patches Costume.cs (actor selection, dedicated actors, Dreamcast assets),
 * CostumeViewer.cs (ApplyModPowers), SuitRules.cs (profiles) and FramePackets.cs. A fix
 * to either belongs in both places. The serving itself is src/wad_overrides.c (the
 * PC's LooseWadOverrides) and the suit's textures src/suits.c.
 *
 * The suit is chosen by suit=<id> in D:\openspidey.ini (the PC's selected-suit.txt). A
 * suit made for SM1's Spider-Man (model "spiderman") or for Quick Change or Peter
 * Parker wears a dedicated actor, loaded once through the game's own LoadPsx into a
 * private resource record and swapped into the shared spidey record before the retail
 * texture loader runs; a suit with its own model is that model, under "spcu00.psx".
 *
 * The costume viewer lists the mod suits after the nineteen stock costumes (below, and
 * tools/patch_generated_sm2.py). The SMU suits' retargeting is retarget_sm2.c; the
 * Bag-Man shell depth and the armour test are shared (patches_common.c).
 */
#include <stdio.h>
#include <string.h>
#include "runtime.h"
#include "guest_layout.h"
#include "settings.h"
#include "texpack.h"
#include "wad_overrides.h"
#include "SpiderMan2.h"

#define V0 c->r[2]
#define A0 c->r[4]
#define A1 c->r[5]
#define A2 c->r[6]
#define GP c->r[28]
#define RA c->r[31]

#define RESOURCE_TABLE 0x800ACED8u
#define RESOURCE_STRIDE 64u
#define RESOURCE_PERMANENT 0x0Bu
#define BINDING_START 0x0Cu
#define BINDING_END 0x38u
#define BINDING_WORDS ((BINDING_END - BINDING_START) / 4u + 1u)
#define SPIDEY_RESOURCE_INDEX 0x800C236Du
#define SCRATCH_NAME GUEST_SCRATCH_NAMES
#define SELECTED 0x800B31F2u            /* the viewer's selected costume and its three power ids */
#define POWER_FLAGS 0x800C2210u         /* seven booleans, power ids 1..7 */

/* ---- profiles (SuitRules.cs) ------------------------------------------------------------- */
static const char *const PROFILES[19] = {
    "spiderman", "spider-phoenix", "prodigy", "dusk", "insulated", "alex-ross-red",
    "alex-ross-white", "venom-earth-x", "negative-zone", "symbiote", "2099",
    "captain-universe", "unlimited", "bagman", "scarlet", "ben-reilly",
    "quick-change", "peter-parker", "battle-damaged",
};
/* Retail shell:80249280's three power ids per profile (jump table 80250340). */
static const uint8_t POWER_IDS[19][3] = {
    {0,0,0}, {4,6,2}, {4,5,6}, {1,0,0}, {4,0,0}, {5,0,0}, {6,0,0},
    {4,3,0}, {0,0,0}, {3,0,0}, {4,0,0}, {4,3,2}, {1,0,0}, {7,0,0},
    {0,0,0}, {0,0,0}, {7,0,0}, {7,0,0}, {0,0,0},
};

static int g_profile;                   /* the suit's SM2 profile */

static int profile_index(const char *name)
{
    for (int i = 0; i < 19; i++) if (!strcmp(PROFILES[i], name)) return i;
    return 0;
}

/* CostumeViewer.ApplyModPowers. */
static void apply_mod_powers(void)
{
    if (!suit_active()) return;
    mem_w8(SELECTED, (uint8_t)g_profile);   /* every unmodified retail table sees 0..18 */
    for (uint32_t i = 0; i < 3; i++) mem_w8(SELECTED + 1u + i, POWER_IDS[g_profile][i]);
    for (uint32_t i = 0; i < 7; i++) mem_w32(POWER_FLAGS + i * 4u, 0);
    for (int i = 0; i < 3; i++) {
        uint8_t id = POWER_IDS[g_profile][i];
        if (id) mem_w32(POWER_FLAGS + (uint32_t)(id - 1) * 4u, 1);
    }
}

/* ---- actors (Costume.ActivateActor) ---------------------------------------------------- */
#define SUIT_SLOT 200                   /* the suit's own model, "spcu00" */
static int g_slot = -1;                 /* the retail costume slot shown, 0..18 */
static int g_active_actor = -1;
static int g_loaded_suit;               /* TextureLibraryLoaded ran with the suit selected */
static uint32_t g_generic[BINDING_WORDS];
static int g_generic_saved;
/* The one dedicated actor resident (Bag-Man, Peter Parker, SM1's Spider-Man, Quick
 * Change or a suit's own model): the costume viewer can switch between all of them, so
 * each switch frees the one before it rather than keeping them all. */
static int g_resident_slot = -1, g_resident_resource = -1, g_resident_suit = -1;

int costume_sm2_suit_loaded(void) { return g_loaded_suit; }

static uint32_t resource_entry(int index) { return RESOURCE_TABLE + (uint32_t)index * RESOURCE_STRIDE; }

static void read_binding(uint32_t entry, uint32_t *binding)
{
    for (uint32_t i = 0; i < BINDING_WORDS; i++) binding[i] = mem_r32(entry + BINDING_START + i * 4u);
}

static void write_binding(uint32_t entry, const uint32_t *binding)
{
    for (uint32_t i = 0; i < BINDING_WORDS; i++) mem_w32(entry + BINDING_START + i * 4u, binding[i]);
}

static void write_name(uint32_t address, const char *text)
{
    uint32_t i = 0;
    for (; text[i]; i++) mem_w8(address + i, (uint8_t)text[i]);
    mem_w8(address + i, 0);
}

/* Load a dedicated actor through the game's own LoadPsx; it stays resident (marked
 * permanent, so level changes keep it) until activate_actor frees it. */
static int load_special(CpuContext *c, int slot)
{
    const char *request = slot == 13 ? "spbagdc" : slot == 17 ? "spparkdc" : slot == 100 ? "spmod01" :
                          slot == 101 ? "spmodqc" : slot == SUIT_SLOT ? "spcu00" : 0;
    if (!request) return -1;
    uint32_t name = SCRATCH_NAME + (slot == 13 ? 0x00u : slot == 17 ? 0x20u : slot == 100 ? 0x40u : slot == 101 ? 0x60u : 0x80u);
    write_name(name, request);
    CpuContext saved = *c;
    A0 = name; A1 = 0;
    SpiderMan2_LoadPsx(c);
    int index = (int)V0;
    *c = saved;
    if (index < 0 || index >= 40 || !mem_r32(resource_entry(index) + 0x14u)) {
        rt_log("costume: dedicated actor %s did not load", request);
        return -1;
    }
    mem_w8(resource_entry(index) + RESOURCE_PERMANENT, 1);
    g_resident_slot = slot; g_resident_resource = index;
    g_resident_suit = slot == SUIT_SLOT ? suits_selected() : -1;
    rt_log("costume: resident actor %s (resource %d)", request, index);
    return index;
}

/* Costume.RebindMorphTargets: the head and hand morph targets of the active actor. */
static void rebind_morph_targets(CpuContext *c, int spidey)
{
    static const uint32_t targets[3][2] = { { 0x800C2258u, 7u }, { 0x800C225Cu, 17u }, { 0x800C2260u, 14u } };
    for (int i = 0; i < 3; i++) {
        mem_w32(targets[i][0], 0);
        CpuContext saved = *c;
        A0 = (uint32_t)spidey; A1 = targets[i][1]; A2 = targets[i][0];
        SpiderMan2_func_8004EB74(c);
        *c = saved;
    }
}

static void activate_actor(CpuContext *c, int slot, const char *model)
{
    int spidey = (int)mem_r8(SPIDEY_RESOURCE_INDEX);
    if (spidey < 0 || spidey >= 40) return;
    uint32_t entry = resource_entry(spidey);
    if (!g_generic_saved) { read_binding(entry, g_generic); g_generic_saved = 1; }
    if (!g_generic[2]) return;
    int actor;
    if (model && !strcmp(model, "spiderman")) actor = 100;          /* SM1's Spider-Man */
    else if (model && !strcmp(model, "quick-change")) actor = 101;
    else if (model && !strcmp(model, "peter-parker")) actor = 17;
    else actor = slot == 13 || slot == 17 ? slot : 0;
    if (model && suit_model_file()) actor = SUIT_SLOT;
    int suit = actor == SUIT_SLOT ? suits_selected() : -1;
    if (actor == g_active_actor && (actor != SUIT_SLOT || g_resident_suit == suit)) return;
    /* Free the resident dedicated actor unless it is the one wanted: the shared binding
     * goes back first, then the retail resource destructor (0x80074754) releases its
     * cache entry and memory (an arena block through the HeapFree hook). */
    if (g_resident_slot >= 0 && (g_resident_slot != actor || g_resident_suit != suit)) {
        write_binding(entry, g_generic);
        /* Only a record still marked as the one this file loaded (+0x0B, set below). */
        if (mem_r8(resource_entry(g_resident_resource) + RESOURCE_PERMANENT) == 1) {
        CpuContext saved = *c;
        A0 = (uint32_t)g_resident_resource; A1 = 1;
        SpiderMan2_func_80074754(c);
        *c = saved;
        rt_log("costume: dedicated actor %d freed", g_resident_slot);
        }
        g_resident_slot = g_resident_resource = g_resident_suit = -1;
        g_active_actor = -1;
    }
    const uint32_t *binding = g_generic;
    uint32_t special[BINDING_WORDS];
    if (actor) {
        int resource = g_resident_slot == actor ? g_resident_resource : load_special(c, actor);
        if (resource >= 0) { read_binding(resource_entry(resource), special); binding = special; }
        else actor = 0;          /* not loaded: the shared model, its morph targets rebound */
    }
    /* The binding is a family of processed pointers, not only the container at +0x14:
     * copying the base alone leaves the generic mesh table active. */
    write_binding(entry, binding);
    rebind_morph_targets(c, spidey);
    g_active_actor = actor;
    rt_log("costume: active actor %d", actor);
}

/* Pre func_8004E4BC(actor data, one-based costume). */
int hook_Costume_SelectTextureLibrary(CpuContext *c)
{
    g_loaded_suit = 0;
    if (suit_active()) {
        apply_mod_powers();
        g_slot = 0;              /* appearance stays default even for Bag-Man/Peter profiles */
        A1 = 1;
        activate_actor(c, 0, suit_model());
        return 1;
    }
    if (A1 >= 1 && A1 <= 19) g_slot = (int)A1 - 1; else return 1;
    activate_actor(c, g_slot, 0);
    return 1;
}

/* Post: the suit's textures bind from here, and its profile is the costume's powers
 * (retail also reads gp+0xAA8 for Insulated Suit's electrical resistance). */
void hook_Costume_TextureLibraryLoaded(CpuContext *c)
{
    if (!suit_active()) return;
    g_loaded_suit = 1;
    mem_w32(GP + 0xAA8u, (uint32_t)g_profile + 1u);
}

/* Pre func_8004EC34(scale, mesh, target): retail slots 13 and 17 rewrite mesh 7 from
 * low-detail Bag-Man/Peter tables shorter than the dedicated Dreamcast heads. */
int hook_Costume_SkipRetailSpecialHeadMorph(CpuContext *c)
{
    return (g_slot != 13 && g_slot != 17) || A1 != 7u;
}

/* ---- the lookup (Costume.DreamcastAssetFor, Costume.CustomModelFor) ------------------- */
/* From the CdWadFind pre-hook. 1: answered here (an alias the disc's WAD does not
 * hold), so the game's own lookup is skipped. */
int costume_sm2_find(CpuContext *c, const char *name)
{
    if (rt_strcasecmp(name, "spcu00.psx") == 0 && suit_model_file()) wad_find_file(suit_model_file());
    else {
        const char *source = name;
        if (!rt_strcasecmp(name, "spbagdc.psx")) source = "spidey-slot13.psx";
        else if (!rt_strcasecmp(name, "spparkdc.psx")) source = "spidey-slot17.psx";
        else if (!rt_strcasecmp(name, "spmod01.psx")) source = "spidey-mod-sm1.psx";
        else if (!rt_strcasecmp(name, "spmodqc.psx")) source = "spidey-mod-quick.psx";
        else if (!rt_strcasecmp(name, "sp_tex13.psx") && g_slot == 13) source = "sp_tex13-dc.psx";
        else if (!rt_strcasecmp(name, "sp_tex17.psx") && g_slot == 17) source = "sp_tex17-dc.psx";
        wad_find(name, source);
    }
    return wad_complete_aliased(c);
}

/* ---- FramePackets.cs: two 256 KiB primitive pools for the denser actors -------------------- */
#define PACKET_CAPACITY 0x40000u

/* From the HeapAlloc pre-hook: only func_8006BD90's two 0x17000-byte pools. 1: served. */
int costume_sm2_heap_alloc(CpuContext *c)
{
    if (A0 != 0x17000u || (RA != 0x8006BDE0u && RA != 0x8006BDF8u)) return 0;
    uint32_t pool = wad_scratch(PACKET_CAPACITY);
    if (!pool) return 0;
    V0 = pool;
    return 1;
}

/* Post func_800314E4: retail computes pool + 0x16F00; keep its 256-byte reserve. */
void hook_FramePackets_SetLimit(CpuContext *c)
{
    uint32_t frame = mem_r32(0x800C247Cu), pool = mem_r32(frame + 0x74u);
    if (!wad_owns(pool)) return;
    mem_w32(GP + 0x8D0u, (pool & 0x7FFFFFFFu) + PACKET_CAPACITY - 0x100u);
}

/* ---- ArmorTextures.SkipUnmapped (instruction branch at 8004E930, resume 8004EB0C) --- */
int hook_ArmorTextures_SkipUnmapped(CpuContext *c) { return armor_skip_unmapped(c, 0x800A5F38u, 14); }

/* ---- the costume viewer (CostumeViewer.cs; tools/patch_generated_sm2.py calls these
 * from shell:func_80249280 and the list helpers) ---------------------------------------- */
#define STOCK 19
#define VIEWER_TABLE GUEST_VIEWER_TABLE
#define VIEWER_NAMES (VIEWER_TABLE + 0x8000u)
#define VIEWER_TEXT (VIEWER_TABLE + 0x10000u)
#define VIEWER_END GUEST_VIEWER_END
#define RETAIL_TABLE 0x80251560u        /* shell's nineteen rows */
#define UNLOCKS 0x800B31F8u

/* SuitRules.PowerText: the viewer's power lines for each SM2 profile. */
static const char *const POWER_TEXT[19][4] = {
    { "SUPER STRENGTH", "SUPER AGILITY", "STICK TO WALLS", "SPIDER SENSE" },
    { "INVULNERABILITY", "ENHANCED STRENGTH", "ENHANCED WEB SWING" },
    { "DOUBLE JUMP", "ENHANCED STRENGTH", "ENHANCED WEB SWING" },
    { "STEALTH" }, { "ENHANCED STRENGTH" }, { "DOUBLE JUMP" }, { "ENHANCED WEB SWING" },
    { "UNLIMITED WEBBING", "ENHANCED STRENGTH" }, { "NONE" }, { "UNLIMITED WEBBING" },
    { "ENHANCED STRENGTH" }, { "INVULNERABLE", "ENHANCED STRENGTH", "UNLIMITED WEBBING" },
    { "STEALTH MODE" }, { "NO SPIDEY BELT" }, { "NONE" }, { "NONE" },
    { "NO SPIDEY BELT" }, { "NO SPIDEY BELT" }, { "NONE" },
};

uint32_t costume_sm2_viewer_count(void)
{
    int n = STOCK + suits_count();
    return (uint32_t)(n < VIEWER_ROWS ? n : VIEWER_ROWS);
}

int costume_sm2_is_unlocked(uint32_t index)
{
    return index < costume_sm2_viewer_count() && (index >= STOCK || (mem_r32(UNLOCKS) & (1u << index)) != 0);
}

uint32_t costume_sm2_read_selected(void)
{
    return suit_active() ? (uint32_t)(STOCK + suits_selected()) : mem_r8(SELECTED);
}

void costume_sm2_write_selected(int selected)
{
    if ((uint32_t)selected >= costume_sm2_viewer_count()) return;
    if (selected >= STOCK && suits_select(selected - STOCK, 1)) {
        g_profile = profile_index(suit_profile_name());
        apply_mod_powers();
    } else {
        suits_select(-1, 1);
        mem_w8(SELECTED, (uint8_t)(selected < STOCK ? selected : 0));
    }
    g_loaded_suit = 0;
}

/* One text line, NUL-terminated; 0 when it would pass `end` (nothing written). */
static uint32_t line(uint32_t at, const char *text, uint32_t end)
{
    uint32_t n = (uint32_t)strlen(text);
    if (!at || at + n + 1 > end) return 0;
    write_name(at, text);
    return at + n + 1;
}

/* A colour token: 2, then the retail charbio.dat heading or body colour. */
static uint32_t colour(uint32_t at, int heading, uint32_t end)
{
    if (!at || at + 4 > end) return 0;
    mem_w8(at, 2);
    mem_w8(at + 1, heading ? 105 : 68);
    mem_w8(at + 2, heading ? 105 : 68);
    mem_w8(at + 3, heading ? 0 : 100);
    return at + 4;
}

/* SuitManifest.WrapComments: upper case, 18-character lines, long words split. */
static uint32_t comment_lines(uint32_t at, const char *text, uint32_t end)
{
    char word[256], out[24] = "";
    const char *p = text;
    while (at && *p) {
        while (*p == ' ') p++;
        if (!*p) break;
        int n = 0;
        while (p[n] && p[n] != ' ' && n < 255) n++;
        for (int i = 0; i < n; i++) word[i] = (char)((p[i] >= 'a' && p[i] <= 'z') ? p[i] - 32 : p[i]);
        word[n] = 0;
        p += n;
        const char *rest = word;
        size_t len = strlen(out);
        if (len && len + strlen(rest) + 1 > 18) { at = line(at, out, end); out[0] = 0; len = 0; }
        while (at && strlen(rest) > 18) {
            char piece[19];
            memcpy(piece, rest, 18); piece[18] = 0;
            at = line(at, piece, end);
            rest += 18;
        }
        if (len) strcat(out, " ");
        strcat(out, rest);
    }
    if (at && out[0]) at = line(at, out, end);
    return at;
}

void costume_sm2_prepare_viewer(CpuContext *c)
{
    (void)c;
    apply_mod_powers();
    for (uint32_t i = 0; i < STOCK * 12u; i += 4) mem_w32(VIEWER_TABLE + i, mem_r32(RETAIL_TABLE + i));
    uint32_t name = VIEWER_NAMES, text = VIEWER_TEXT, count = costume_sm2_viewer_count();
    for (uint32_t i = STOCK; i < count; i++) {
        int s = (int)i - STOCK;
        const char *display = suits_name(s);
        char upper[48];
        size_t k = 0;
        for (; display[k] && k < sizeof upper - 1; k++) upper[k] = (char)((display[k] >= 'a' && display[k] <= 'z') ? display[k] - 32 : display[k]);
        upper[k] = 0;
        uint32_t entry_name = name, entry_text = text;
        uint32_t n = line(name, display, VIEWER_TEXT);
        uint32_t t = colour(text, 1, VIEWER_END);
        t = line(t, "COSTUME:", VIEWER_END);
        t = colour(t, 0, VIEWER_END);
        t = line(t, upper, VIEWER_END);
        t = colour(t, 1, VIEWER_END);
        t = line(t, "GAME POWERS:", VIEWER_END);
        t = colour(t, 0, VIEWER_END);
        int profile = profile_index(suits_profile_name(s));
        for (int j = 0; j < 4 && POWER_TEXT[profile][j]; j++) t = line(t, POWER_TEXT[profile][j], VIEWER_END);
        t = colour(t, 1, VIEWER_END);
        t = line(t, "COMMENTS:", VIEWER_END);
        t = colour(t, 0, VIEWER_END);
        t = comment_lines(t, suits_comments(s), VIEWER_END);
        if (!n || !t || t + 1 > VIEWER_END) { rt_note("costume viewer text arena full"); break; }   /* bounded: row left out */
        mem_w8(t++, 255);
        uint32_t row = VIEWER_TABLE + i * 12u;
        mem_w32(row, entry_name);
        mem_w32(row + 4, entry_text);
        mem_w32(row + 8, mem_r32(RETAIL_TABLE + 8));
        name = n; text = t;
    }
}

/* The retail constructor sets up forty inline 32-byte rows; this viewer's list was
 * allocated for VIEWER_ROWS (the transform), so the rest are set up here alike. */
void costume_sm2_configure_viewer_list(uint32_t list)
{
    for (uint32_t row = 40; row < VIEWER_ROWS; row++) {
        uint32_t target = list + 0x28u + row * 32u;
        for (uint32_t o = 0; o < 32; o++) mem_w8(target + o, 0);
        mem_w16(target + 8, (uint16_t)mem_r16(list + 0x30u));
        mem_w16(target + 10, (uint16_t)mem_r16(list + 0x32u));
        for (uint32_t o = 16; o < 32; o++) mem_w8(target + o, (uint8_t)mem_r8(list + 0x28u + o));
        mem_w32(target + 16, 1);
    }
    mem_w32(list + 0x20u, 70);
    mem_w8(list + 0x15u, 11);
}

void costume_sm2_align_viewer_frame(uint32_t list)
{
    uint32_t frame = mem_r32(list + 4u);
    if (!frame) return;
    mem_w32(frame + 0x20u, 58);
    mem_w32(frame + 0x10u, 117);
}

/* ---- set-up ------------------------------------------------------------------------------ */
void costume_sm2_install(void)
{
    wad_init();
    suits_install(RESOURCE_TABLE, costume_sm2_suit_loaded, "sm2", VIEWER_ROWS - STOCK);
    if (suit_active()) g_profile = profile_index(suit_profile_name());
    if (texpack_load("D:\\assets\\packs\\dreamcast-sm2-special-costumes")) rt_note("Dreamcast texture pack loaded");
}
