/*
 * Patches both games share, ported from spiderman/patches and spiderman2/patches
 * (GpuPatches.cs and PadPatches.cs are the same file in both; DcModelCompatibility's
 * Bag-Man shell and ArmorTextures' test are the same code with each game's table).
 * Keep in step with the C# (see xbox/README.md).
 */
#include "gpu.h"
#include "runtime.h"
#include "settings.h"

#define A0 c->r[4]
#define A1 c->r[5]
#define A2 c->r[6]
#define V0 c->r[2]
#define S0 c->r[16]
#define S2 c->r[18]
#define S4 c->r[20]
#define SP c->r[29]

/* ---- GpuPatches -------------------------------------------------------------------- */

/* pre-hook: the hardware reset in front of the game's own ResetGraph. */
int hook_GpuPatches_ResetGraph(CpuContext *c)
{
    gpu_write_gp1((A0 & 7u) == 0 ? 0x00000000u : 0x01000000u);
    return 1;
}

void hook_GpuPatches_SetDispMask(CpuContext *c) { gpu_write_gp1(0x03000000u | (A0 ? 0u : 1u)); V0 = 0; }
void hook_GpuPatches_SetGraphDebug(CpuContext *c) { V0 = 0; }

void hook_GpuPatches_DrawPrim(CpuContext *c)
{
    uint32_t p = A0, words = mem_r32(p) >> 24;
    for (uint32_t i = 0; i < words; i++) gpu_write_gp0_at(mem_r32(p + 4u + i * 4u), p + 4u + i * 4u);
    V0 = 0;
}

void hook_GpuPatches_ClearOTagR(CpuContext *c)
{
    uint32_t ot = A0;
    int n = (int)A1;
    V0 = 0;
    if (n <= 0) return;
    for (int i = n - 1; i > 0; i--) mem_w32(ot + (uint32_t)i * 4u, (ot + (uint32_t)(i - 1) * 4u) & 0x00FFFFFFu);
    mem_w32(ot, 0x00FFFFFFu);
}

/* ---- PadPatches: both games set pads up through PadInitMtap; its original runs and
 * the runtime learns the two buffers. The pad is presented as a DualShock (analog). - */
static uint32_t g_pend_buf1, g_pend_buf2;
int hook_PadPatches_PadInitMtapEnter(CpuContext *c) { g_pend_buf1 = A0; g_pend_buf2 = A1; return 1; }
void hook_PadPatches_PadInitMtapExit(CpuContext *c) { (void)c; rt_pad_set_buffers(g_pend_buf1, g_pend_buf2, 1); }

/* ---- DcModelCompatibility: Bag-Man's nested shell (both games) ---------------------- */
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


/* ---- ArmorTextures (both games) -------------------------------------------------------- */
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

/* ArmorTextures.SkipUnmapped's test (the instruction branch at the top of the armour
 * swap's per-texture loop): 1 to skip this texture -- no armor version, a different page
 * size or depth, or a suit (which keeps its own look). table/pairs: the game's retail
 * hash -> armor hash table (SM1 0x80099658, 13 pairs; SM2 0x800A5F38, 14 pairs). */
int armor_skip_unmapped(CpuContext *c, uint32_t table, uint32_t pairs)
{
    uint32_t descriptor = S0, hash = mem_r32(descriptor + 0x14u), armor_hash = 0;
    for (uint32_t i = 0; i < pairs && !armor_hash; i++)
        if (mem_r32(table + i * 8u) == hash) armor_hash = mem_r32(table + i * 8u + 4u);
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
