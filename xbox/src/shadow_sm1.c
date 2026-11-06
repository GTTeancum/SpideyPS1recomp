/*
 * Player silhouette shadow (issue #9): a port of the PC's PlayerShadow.cs
 * (spiderman/patches). A fix to either belongs in both places.
 *
 * The console draws a round blob under Spider-Man; the N64 version casts his
 * silhouette. Capture: the per-part mesh draw (func_80077C08 / func_80077CC4, mesh in
 * A0, actor in the caller's S2) runs with the GTE holding that part's camera-space
 * RT/TR, so every player triangle is recorded in camera space as the game projects
 * it. Draw: the blob (tpage 76, CLUT 33, semi-transparent) gives the floor height --
 * the point below the player that projects onto its centre -- and the triangles,
 * flattened onto that floor along a fixed world light, are drawn in its place,
 * darkening each pixel once (hw_shadow, through the stencil).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"
#include "hwgpu.h"
#include "settings.h"

#define A0 c->r[4]
#define S2 c->r[18]
#define GP c->r[28]

#define PLAYER_POINTER 0x800B5268u
#define CAMERA_ROTATION 0x800A57CCu
/* The blob: CLUT 33, subtractive blending. Its texture page moves when a suit's own
 * model takes VRAM (page 76 for the stock actors, 88 with Raimi Symbiote), so the page
 * is not part of the test; the position checks below tie it to the player. */
#define BLOB_CLUT 33
static int is_blob(const RtPrim *e) { return e->clut == BLOB_CLUT && ((e->tpage >> 5) & 3) == 2; }

typedef struct { float x, y, z; } V3;
static V3 v3(float x, float y, float z) { V3 r = { x, y, z }; return r; }
static V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static V3 scale(V3 a, float k) { return v3(a.x * k, a.y * k, a.z * k); }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 normalize(V3 a) { float n = __builtin_sqrtf(dot(a, a)); return n > 0 ? scale(a, 1.0f / n) : a; }

#define MAX_TRIANGLES 16384   /* a Dreamcast actor has several thousand */
static V3 g_tri[MAX_TRIANGLES * 3];
static int g_tri_count;
static uint32_t g_frame = 0xFFFFFFFFu;
static float g_h, g_ofx, g_ofy;
static int16_t g_camera[9];
static V3 g_light;
static int g_debug;   /* shadow=debug: the captured body unflattened (alignment check) */

/* Joints. A model's shared vertices (flag 1) are numbered in order across its parts,
 * and a joint vertex (flag 2) names the one it repeats: its x is 8 x that number once
 * loaded (the file has the number in y). So every shared vertex is recorded by its
 * number as its part is drawn, and a later part's joint takes it from there. */
#define MAX_SOURCES 4096
static V3 g_source[MAX_SOURCES];
static uint32_t g_source_frame[MAX_SOURCES];
static V3 g_current[512];

/* Shared vertices before part `mesh` in the actor's model: its first shared number. */
static int first_source(uint32_t actor, uint32_t mesh)
{
    uint32_t slot = mem_r8(actor + 0x1B);
    if (slot >= 40) return -1;
    uint32_t table = mem_r32(0x800A0904u + slot * 64u + 0x10u);
    if (table < 0x80000004u || table >= 0x80800000u) return -1;
    uint32_t parts = mem_r32(table - 4);
    if (parts > 64) return -1;
    int count = 0;
    for (uint32_t i = 0; i < parts; i++) {
        uint32_t part = mem_r32(table + i * 4);
        if (part == mesh) return count;
        uint32_t nv = mem_r16(part + 2);
        for (uint32_t v = 0; v < nv && nv <= 512; v++) if ((mem_r16(part + 28 + v * 8 + 6) & 3) == 1) count++;
    }
    return -1;
}

int hook_PlayerShadow_MeshDraw(CpuContext *c)
{
    if (!S2 || S2 != mem_r32(PLAYER_POINTER)) return 1;
    if (rt_drawotag_count != g_frame) {
        g_frame = rt_drawotag_count;
        g_tri_count = 0;
        g_h = (float)(int16_t)gte_read_control(26);
        g_ofx = (int32_t)gte_read_control(24) / 65536.0f;
        g_ofy = (int32_t)gte_read_control(25) / 65536.0f;
        for (uint32_t i = 0; i < 9; i++) g_camera[i] = (int16_t)mem_r16(CAMERA_ROTATION + i * 2);
    }
    uint32_t mesh = A0;
    if (mesh < 0x80000000u || mesh >= 0x807FF000u) return 1;
    int nv = (int)mem_r16(mesh + 2), nn = (int)mem_r16(mesh + 4), nf = (int)mem_r16(mesh + 6);
    if (!nv || nv > 512 || nf > 1024 || nn != nv + nf) return 1;
    float rt[9];
    for (int i = 0; i < 4; i++) {
        uint32_t w = gte_read_control(i);
        rt[i * 2] = (int16_t)w / 4096.0f;
        rt[i * 2 + 1] = (int16_t)(w >> 16) / 4096.0f;
    }
    rt[8] = (int16_t)gte_read_control(4) / 4096.0f;
    V3 tr = v3((float)(int32_t)gte_read_control(5), (float)(int32_t)gte_read_control(6), (float)(int32_t)gte_read_control(7));
    int source = first_source(S2, mesh);
    /* With gp+0x1170 clear the game's transform (func_8007B798 / 8007BBD4) divides each
     * vertex by 16 (arithmetic shift) before projecting it; with it set (8007B9CC /
     * 8007BD04) vertices go in whole. Taking them whole in the first case made the
     * silhouette 16 times too big and threw it across the scene (climbing, distant). */
    int shift = mem_r32(GP + 0x1170u) == 0 ? 4 : 0;
    for (int i = 0; i < nv; i++) {
        uint32_t v = mesh + 28 + (uint32_t)i * 8;
        int16_t x = (int16_t)mem_r16(v), y = (int16_t)mem_r16(v + 2), z = (int16_t)mem_r16(v + 4);
        uint32_t kind = mem_r16(v + 6) & 3;
        int16_t sx = (int16_t)(x >> shift), sy = (int16_t)(y >> shift), sz = (int16_t)(z >> shift);
        V3 p;
        if (kind & 2) {
            uint32_t k = (uint16_t)x / 8u;
            if (k < MAX_SOURCES && g_source_frame[k] == g_frame + 1u) p = g_source[k];
            else { p = v3(NAN, NAN, NAN); rt_metric[3]++; }
        } else {
            p = add(tr, v3(rt[0] * sx + rt[1] * sy + rt[2] * sz, rt[3] * sx + rt[4] * sy + rt[5] * sz, rt[6] * sx + rt[7] * sy + rt[8] * sz));
            if (kind == 1 && source >= 0 && source < MAX_SOURCES) { g_source[source] = p; g_source_frame[source] = g_frame + 1u; }
            if (kind == 1 && source >= 0) source++;
        }
        g_current[i] = p;
    }
    /* Faces are variable length (u16 size at +2); vertex indices at +4..+7, the fourth
     * repeating the third for a triangle. */
    uint32_t face = mesh + 28 + (uint32_t)(nv + nn) * 8;
    for (int f = 0; f < nf; f++) {
        uint32_t size = mem_r16(face + 2);
        if (size < 12 || size > 64) return 1;
        int idx[4];
        for (int k = 0; k < 4; k++) idx[k] = (int)mem_r8(face + 4 + (uint32_t)k);
        if (idx[0] < nv && idx[1] < nv && idx[2] < nv) {
            int tris[2][3] = { { idx[0], idx[1], idx[2] }, { idx[1], idx[3], idx[2] } };
            int count = idx[3] < nv && idx[3] != idx[2] ? 2 : 1;
            for (int t = 0; t < count && g_tri_count < MAX_TRIANGLES; t++) {
                V3 a = g_current[tris[t][0]], b = g_current[tris[t][1]], d = g_current[tris[t][2]];
                if (isnan(a.x) || isnan(b.x) || isnan(d.x)) continue;
                g_tri[g_tri_count * 3] = a; g_tri[g_tri_count * 3 + 1] = b; g_tri[g_tri_count * 3 + 2] = d;
                g_tri_count++;
            }
        }
        face += size;
    }
    return 1;
}

static void project(V3 p, float *x, float *y)
{
    *x = g_h * p.x / p.z * (float)rt_fov_num / (float)rt_fov_den + g_ofx;
    *y = g_h * p.y / p.z + g_ofy;
}

static float g_shadow_xy[MAX_TRIANGLES * 3][2];

/* The player's blob: draw the silhouette in its place. */
static int replace_blob(const RtPrim *e, int region)
{
    if (e->count != 4 || !e->textured || !e->semi || !is_blob(e) || !g_tri_count) return 0;
    /* Where the player is on screen, to tell his blob from enemies' and pickups'. */
    V3 centre = v3(0, 0, 0);
    float min_x = 1e30f, max_x = -1e30f, max_y = -1e30f;
    for (int i = 0; i < g_tri_count * 3; i++) {
        V3 p = g_tri[i];
        centre = add(centre, p);
        if (p.z <= 1) continue;
        float sx, sy;
        project(p, &sx, &sy);
        if (sx < min_x) min_x = sx;
        if (sx > max_x) max_x = sx;
        if (sy > max_y) max_y = sy;
    }
    centre = scale(centre, 1.0f / (float)(g_tri_count * 3));
    float bx = 0, by = 0;
    for (int i = 0; i < 4; i++) { bx += (float)(e->x[i] - e->offset_x); by += (float)(e->y[i] - e->offset_y); }
    bx /= 4; by /= 4;
    float width = max_x - min_x;
    if (bx < min_x - width || bx > max_x + width || by < max_y - 2 * width - 40) return 0;
    V3 down = normalize(v3(g_camera[1], g_camera[4], g_camera[7]));
    V3 light = normalize(v3(g_camera[0] * g_light.x + g_camera[1] * g_light.y + g_camera[2] * g_light.z,
                            g_camera[3] * g_light.x + g_camera[4] * g_light.y + g_camera[5] * g_light.z,
                            g_camera[6] * g_light.x + g_camera[7] * g_light.y + g_camera[8] * g_light.z));
    /* Floor: the point centre + s * down that projects onto the blob's centre. */
    float k = by - g_ofy, den = k * down.z - g_h * down.y;
    if (fabsf(den) < 1e-3f) return 0;
    float s0 = (g_h * centre.y - k * centre.z) / den;
    V3 floor_point = add(centre, scale(down, s0));
    float fx, fy;
    project(floor_point, &fx, &fy);
    /* It must also land on the blob horizontally, or the blob is something else's. */
    if (s0 <= 0 || floor_point.z <= 16 || fabsf(fx - bx) > 0.25f * width + 6) return 0;
    float along = dot(light, down);
    if (along < 0.2f) return 0;
    int n = 0;
    for (int t = 0; t < g_tri_count; t++) {
        int ok = 1;
        for (int j = 0; j < 3 && ok; j++) {
            V3 p = g_tri[t * 3 + j];
            if (!g_debug) p = add(p, scale(light, dot(sub(floor_point, p), down) / along));
            if (p.z <= 16) { ok = 0; break; }
            float x, y;
            project(p, &x, &y);
            g_shadow_xy[n * 3 + j][0] = x + (float)e->offset_x;
            g_shadow_xy[n * 3 + j][1] = y + (float)e->offset_y;
        }
        if (ok) n++;
    }
    rt_metric[2] = (uint32_t)n;
    hw_shadow(region, (const float (*)[2])g_shadow_xy, n);
    return 1;
}

void shadow_sm1_install(void)
{
    g_light = normalize(v3(0.45f, 1.0f, 0.55f));   /* SPIDEY_SHADOW_LIGHT's default */
    rt_prim_replace_hook = replace_blob;
    const char *mode = setting("shadow");
    g_debug = mode && !strcmp(mode, "debug");
    rt_metric_name[2] = "shadow triangles";
    rt_metric_name[3] = "shadow joints not found";
}
