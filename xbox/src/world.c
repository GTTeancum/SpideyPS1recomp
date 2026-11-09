/*
 * Precise projection through the subdivided world faces, both games: the Xbox
 * counterpart of the PC's WorldSubpixel (spiderman/patches/WorldSubpixel.cs, which
 * Spider-Man 2's build also compiles). A fix to either belongs in both places. The
 * routines named below are SM1's; SM2's are func_80089918 (corners), func_8008997C
 * (subdivision) and func_80089B74 (edge), the same code at other addresses.
 *
 * func_8007D33C splits a large face near the camera into a grid by running RTPS through
 * a temporary GTE matrix built from the face's three projected corners, with barycentric
 * weights (a, b, d, summing to 4096) as the input vector. The RTPS results are in that
 * temporary space: their depth is not camera depth, and their positions are rounded
 * corners interpolated. Each grid point gets the perspective-correct blend of the
 * corners' own precise projections instead, so it lies on the same projective edges as
 * an adjacent face that was not split (otherwise the two leave slivers between them).
 * That projection can be pixels away from the routine's own rounded point (most of all
 * on a large face close to the camera, the faces it splits), so the tag is certified
 * for the grid word (VTag.native) as the PC's is, not judged by distance.
 *
 * func_8007D534 then nudges a split edge's points by a pixel to close integer raster
 * cracks; the precise geometry has none, so a nudged point keeps its projection,
 * certified for the nudged word.
 */
#include "runtime.h"
#include "settings.h"

#define A0 c->r[4]
#define A2 c->r[6]
#define T1 c->r[9]
#define T2 c->r[10]
#define T3 c->r[11]
#define T6 c->r[14]

static VTag g_corner[3];
static int g_valid;

/* Pre func_8007D2D8: the face's corners, as the addresses of their screen words. */
int hook_WorldSubpixel_SetSubdivisionCorners(CpuContext *c)
{
    uint32_t at[3] = { T1, T2, T3 };
    g_valid = 0;
    for (int i = 0; i < 3; i++) {
        const VTag *t = vtag_of(at[i], mem_r32(at[i]));
        if (t) { g_corner[i] = *t; g_valid |= 1 << i; }
    }
    return 1;
}

static int coord(uint32_t w, int shift) { return (int)(w << (21 - shift)) >> 21; }

/* The grid's 16-byte records in the scratchpad, as their SXY words are stored (the PC's
 * CertifyClamp). The routine caps X to [-510, 1022] and Y to 510 after RTPS. */
static void grid_point(uint32_t address, uint32_t word, VTag *tag)
{
    if (address < 0x1F800000u || address >= 0x1F800400u || (address & 15u)) return;
    /* The SXY the GTE wrote: the position's pixel, saturated as the GTE saturates. */
    int x = tag->x16 >> 4, y = tag->y16 >> 4;
    x = x < -1024 ? -1024 : x > 1023 ? 1023 : x;
    y = y < -1024 ? -1024 : y > 1023 ? 1023 : y;
    x = x < -510 ? -510 : x > 1022 ? 1022 : x;
    y = y < -1024 ? -1024 : y > 510 ? 510 : y;
    uint32_t expected = (uint32_t)(uint16_t)(int16_t)x | ((uint32_t)(uint16_t)(int16_t)y << 16);
    if ((word & 0x07FF07FFu) != (expected & 0x07FF07FFu)) return;
    uint32_t xy = gte_read_data(0);
    int a = (int16_t)xy, b = (int16_t)(xy >> 16), d = (int16_t)gte_read_data(1);
    /* A clipped third corner must not lose the opposite visible edge: only the corners
     * with a weight need a projection. */
    if (a >= 0 && b >= 0 && d >= 0 && a + b + d == 4096 && (a == 0 || (g_valid & 1)) && (b == 0 || (g_valid & 2)) &&
        (d == 0 || (g_valid & 4))) {
        double za = a * (double)g_corner[0].z, zb = b * (double)g_corner[1].z, zd = d * (double)g_corner[2].z;
        double z = za + zb + zd;
        if (z > 0) {
            tag->z = (float)(z / 4096);
            tag->x16 = (int16_t)((za * g_corner[0].x16 + zb * g_corner[1].x16 + zd * g_corner[2].x16) / z);
            tag->y16 = (int16_t)((za * g_corner[0].y16 + zb * g_corner[1].y16 + zd * g_corner[2].y16) / z);
            tag->native = VTAG_CERTIFIED | (word & VTAG_SXY);
            return;
        }
    }
    /* No corner projections: the temporary space's depth is not camera depth, so the
     * point keeps none and its triangles draw affine. */
    tag->z = 0.0f;
}

static void (*g_outer)(uint32_t, uint32_t, VTag *);

/* func_8007D33C: the grid's stores take the blended projections. */
int hook_WorldSubpixel_SubdivisionEnter(CpuContext *c)
{
    (void)c;
    g_outer = rt_vtag_store_hook;
    rt_vtag_store_hook = grid_point;
    return 1;
}

void hook_WorldSubpixel_SubdivisionExit(CpuContext *c)
{
    (void)c;
    rt_vtag_store_hook = g_outer;
    g_valid = 0;
}

/* func_8007D534 advances A0 by T6, then offsets max(A2 - 1, 1) points. */
#define SAVED 64
static struct { uint32_t address, word; VTag tag; } g_saved[SAVED];
static int g_count;

int hook_WorldSubpixel_EdgeEnter(CpuContext *c)
{
    g_count = 0;
    int count = (int)A2 - 1;
    if (count < 1) count = 1;
    if (count > SAVED) return 1;
    uint32_t address = A0;
    for (int i = 0; i < count; i++) {
        address += T6;
        uint32_t word = mem_r32(address);
        const VTag *t = vtag_of(address, word);
        if (!t || !vtag_matches(word, t)) continue;
        g_saved[g_count].address = address;
        g_saved[g_count].word = word;
        g_saved[g_count].tag = *t;
        g_count++;
    }
    return 1;
}

void hook_WorldSubpixel_EdgeExit(CpuContext *c)
{
    (void)c;
    for (int i = 0; i < g_count; i++) {
        uint32_t word = mem_r32(g_saved[i].address), old = g_saved[i].word;
        int dx = coord(word, 0) - coord(old, 0), dy = coord(word, 16) - coord(old, 16);
        /* Packed 32-bit addition can carry or borrow across the X halfword: crossing
         * X = 0/-1 changes Y by zero or two for a nominal offset of one. */
        int crossed = (coord(old, 0) == 0 && dx == -1) || (coord(old, 0) == -1 && dx == 1);
        if (dx > 1 || dx < -1 || dy > (crossed ? 2 : 1) || dy < -(crossed ? 2 : 1)) continue;
        /* The offsets cover integer raster cracks; the precise projection has none, and
         * moving it would separate shared edges. Keep it, certified for the new word. */
        VTag t = g_saved[i].tag;
        t.native = VTAG_CERTIFIED | (word & VTAG_SXY);
        vtag_store(g_saved[i].address, word, &t);
    }
    g_count = 0;
}
