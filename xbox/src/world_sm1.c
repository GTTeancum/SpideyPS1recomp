/*
 * Depth for SM1's subdivided world faces: the Xbox counterpart of the PC's
 * WorldSubpixel.SetSubdivisionCorners / SubdivisionEnter (spiderman/patches). A fix to
 * either belongs in both places.
 *
 * func_8007D33C splits a large face near the camera into a grid by running RTPS through
 * a temporary GTE matrix built from the face's three projected corners, with barycentric
 * weights (a, b, d, summing to 4096) as the input vector. The SZ that RTPS yields is in
 * that temporary space, not camera depth, so perspective texturing took it at face value
 * and smeared the floor into a sunburst around the player. Each grid point's true depth
 * is the weighted blend of the corners' depths, recorded instead.
 */
#include "runtime.h"
#include "hwgpu.h"
#include "settings.h"

#define T1 c->r[9]
#define T2 c->r[10]
#define T3 c->r[11]

static float g_corner[3];
static int g_valid;

/* Pre func_8007D2D8: the face's corners, as the addresses of their screen words. */
int hook_WorldSubpixel_SetSubdivisionCorners(CpuContext *c)
{
    uint32_t at[3] = { T1, T2, T3 };
    g_valid = 0;
    for (int i = 0; i < 3; i++) {
        uint32_t w = mem_r32(at[i]);
        g_corner[i] = hw_depth_at((int16_t)w, (int16_t)(w >> 16));
        if (g_corner[i] > 0) g_valid |= 1 << i;
    }
    return 1;
}

static uint16_t blended(int32_t a, int32_t b, int32_t d, uint16_t sz)
{
    if (a < 0 || b < 0 || d < 0 || a + b + d != 4096) return sz;
    /* A weight of zero needs no corner; any other needs that corner's depth. */
    if ((a && !(g_valid & 1)) || (b && !(g_valid & 2)) || (d && !(g_valid & 4))) return 0;
    float z = (a * g_corner[0] + b * g_corner[1] + d * g_corner[2]) / 4096.0f;
    return z > 0 && z < 65535.0f ? (uint16_t)(z + 0.5f) : 0;
}

/* func_8007D33C: the grid's RTPS calls record blended corner depths (0: unknown, so
 * the point draws affine rather than with a wrong depth). */
int hook_WorldSubpixel_SubdivisionEnter(CpuContext *c) { (void)c; rt_rtp_depth_hook = blended; return 1; }
void hook_WorldSubpixel_SubdivisionExit(CpuContext *c) { (void)c; rt_rtp_depth_hook = 0; g_valid = 0; }

/* func_8007D534 nudges a subdivided edge's points by a pixel to close raster cracks.
 * Carrying their depth to the nudged position (as the PC's WorldSubpixel.EdgeExit
 * keeps their projection) smeared the floor into a sunburst here, so a nudged point
 * keeps no depth and its triangles draw affine. */
int hook_WorldSubpixel_EdgeEnter(CpuContext *c) { (void)c; return 1; }
void hook_WorldSubpixel_EdgeExit(CpuContext *c) { (void)c; }
