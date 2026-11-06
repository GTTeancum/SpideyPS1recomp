/*
 * Suit models with their own skeleton (the Spider-Man Unlimited conversions): a port
 * of the PC's SuitRetargeting.cs, SuitWebAttachment.cs (spiderman/patches) and
 * NativeRetargetRig.cs (RecompOne.Runtime/Assets/Suits). A fix to either belongs in
 * both places. The rig maths is the PC's own retarget core, compiled in
 * (runtime/retarget_core.cpp).
 *
 * Each frame, once the game has posed its 18 native parts (instruction hook at
 * 80077418, before lighting and vertex transforms), the suit's preserved rig is posed
 * from those 18 drivers, skinned, and its vertices and normals written back into the
 * resident native meshes, so the game's own renderer draws the suit. Actors with more
 * than 18 mesh pages draw the extra pages under driver 0 (BeginPagedPart/TryNextPage),
 * and the swing web attaches to the rig's wrist (8002CF80).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <x86intrin.h>
#include "prof.h"
#include "runtime.h"
#include "settings.h"

#define V0 c->r[2]
#define S0 c->r[16]
#define S1 c->r[17]
#define S2 c->r[18]
#define S3 c->r[19]
#define S5 c->r[21]
#define S6 c->r[22]
#define GP c->r[28]
#define SP c->r[29]
#define FP c->r[30]

/* The core's C ABI. */
unsigned rtg_abi(void);
int rtg_validate(const unsigned char *d, unsigned len);
int rtg_pose(const unsigned char *d, unsigned len, const float *driver, unsigned flags, float *bones, unsigned bone_floats);
int rtg_evaluate(const unsigned char *d, unsigned len, const float *driver, unsigned flags, float *bones, unsigned bone_floats,
                 float *out, unsigned out_floats);
int rtg_to_part(const float *m, const float *world, unsigned count, float *local);
int rtg_web_target(const unsigned char *d, unsigned len, const float *bones, unsigned bone_floats, unsigned part,
                   const short *actor_r, const int *actor_t, const int *actor_position, unsigned mirror, int *out);
int rtg_web_segment(const int *start, const int *end, const int *tip, const int *target, unsigned index, unsigned count, int *out);

/* ---- the rig (NativeRetargetRig) ------------------------------------------------------ */
#define MAX_PAGES 64
#define MAX_BONES 256
#define MAX_VERTICES 8192
#define MAX_PACKET_VERTICES (MAX_PAGES * 256)
static unsigned char *g_rig;
static unsigned g_rig_len;
static int g_pages, g_bones, g_vertices;
static int g_packet_count[MAX_PAGES], g_packet_first[MAX_PAGES], g_packet_driver[MAX_PAGES];
static uint32_t g_packet_index[MAX_PACKET_VERTICES];
static float g_bone_pose[MAX_BONES * 12], g_world[MAX_VERTICES * 6];
static float g_packet_world[256 * 6], g_local[MAX_PACKET_VERTICES * 6];

static uint32_t u32(unsigned p) { uint32_t v; memcpy(&v, g_rig + p, 4); return v; }

static int unpack_rig(void);

static int load_rig(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 280 || len > 4 * 1024 * 1024 || !(g_rig = (unsigned char *)malloc((size_t)len)) ||
        fread(g_rig, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(g_rig); g_rig = 0; return 0; }
    fclose(f);
    g_rig_len = (unsigned)len;
    if (rtg_abi() != 0x00020002u || rtg_validate(g_rig, g_rig_len)) { rt_note("suit rig rejected"); free(g_rig); g_rig = 0; return 0; }
    int paged = u32(4) == 3;
    g_bones = (int)u32(12); g_vertices = (int)u32(16);
    g_pages = paged ? (int)u32(280) : 18;
    if (g_bones > MAX_BONES || g_vertices > MAX_VERTICES || g_pages > MAX_PAGES) { free(g_rig); g_rig = 0; return 0; }
    unsigned table = u32(40);
    int first = 0;
    for (int i = 0; i < g_pages; i++) {
        g_packet_driver[i] = paged ? (int)u32(u32(284) + (unsigned)i * 4u) : i;
        int count = (int)u32(table + (unsigned)i * 8u);
        unsigned offset = u32(table + (unsigned)i * 8u + 4u);
        if (count > 256 || first + count > MAX_PACKET_VERTICES) { free(g_rig); g_rig = 0; return 0; }
        g_packet_count[i] = count; g_packet_first[i] = first;
        for (int v = 0; v < count; v++) g_packet_index[first + v] = u32(offset + (unsigned)v * 4u);
        first += count;
    }
    if (!unpack_rig()) { free(g_rig); g_rig = 0; return 0; }
    return 1;
}


static int finite(float x) { uint32_t b; memcpy(&b, &x, 4); return (b & 0x7f800000u) != 0x7f800000u; }

/*
 * The rig unpacked once at load. The core (rtg_pose, rtg_evaluate) re-validates the
 * whole file on every call and rebuilds a bone's skin matrix for every vertex weight;
 * at 733 MHz that is 15 ms a pose. pose() and skin() below do the core's arithmetic in
 * the core's order -- the same per-bone matrices, the same sums -- so the results are
 * the same, with validation done once (rtg_validate at load) and one skin matrix per
 * bone per pose. The core stays the reference: a change there belongs here too.
 */
typedef struct { int parent, driver; float inverse[12], rest[12], local[12], hand[9]; } Bone;
typedef struct { float p[6]; uint32_t first, count; } Vertex;
typedef struct { uint32_t bone; float weight; } Weight;
static Bone *g_bone;
static Vertex *g_vertex;
static Weight *g_weight;
static int g_anchor;
static float g_scale, g_origin[3], g_skin[MAX_BONES * 12];

static float f32(unsigned p) { float v; memcpy(&v, g_rig + p, 4); return v; }

static int unpack_rig(void)
{
    unsigned bo = u32(28), vo = u32(32), wo = u32(36), nw = u32(20);
    g_anchor = (int)u32(24);
    g_scale = f32(60);
    for (int j = 0; j < 3; j++) g_origin[j] = f32(64 + (unsigned)j * 4);
    g_bone = (Bone *)malloc(sizeof(Bone) * (size_t)g_bones);
    g_vertex = (Vertex *)malloc(sizeof(Vertex) * (size_t)g_vertices);
    g_weight = (Weight *)malloc(sizeof(Weight) * (size_t)(nw ? nw : 1));
    if (!g_bone || !g_vertex || !g_weight) return 0;
    for (int i = 0; i < g_bones; i++) {
        unsigned b = bo + (unsigned)i * 196u;
        g_bone[i].parent = (int)u32(b); g_bone[i].driver = (int)u32(b + 4);
        for (int k = 0; k < 12; k++) {
            g_bone[i].inverse[k] = f32(b + 16 + (unsigned)k * 4);
            g_bone[i].rest[k] = f32(b + 64 + (unsigned)k * 4);
            g_bone[i].local[k] = f32(b + 112 + (unsigned)k * 4);
        }
        for (int k = 0; k < 9; k++) g_bone[i].hand[k] = f32(b + 160 + (unsigned)k * 4);
    }
    for (int i = 0; i < g_vertices; i++) {
        unsigned v = vo + (unsigned)i * 32u;
        for (int k = 0; k < 6; k++) g_vertex[i].p[k] = f32(v + (unsigned)k * 4);
        g_vertex[i].first = u32(v + 24); g_vertex[i].count = u32(v + 28);
    }
    for (unsigned i = 0; i < nw; i++) { g_weight[i].bone = u32(wo + i * 8); g_weight[i].weight = f32(wo + i * 8 + 4); }
    return 1;
}

/* The core's mul(): row-major affine 3x4, c = a * b. */
static void mul(const float *a, const float *b, float *c)
{
    for (int r = 0; r < 3; r++)
        for (int k = 0; k < 4; k++) {
            float t = k == 3 ? a[r * 4 + 3] : 0;
            for (int j = 0; j < 3; j++) t += a[r * 4 + j] * b[j * 4 + k];
            c[r * 4 + k] = t;
        }
}

/* rtg_pose with flags 0 (fists). */
static void pose_rig(const float *driver)
{
    float delta[3];
    for (int j = 0; j < 3; j++) delta[j] = (driver[j * 4 + 3] - g_origin[j]) * g_scale;
    for (int i = 0; i < g_bones; i++) {
        const Bone *b = &g_bone[i];
        float *out = g_bone_pose + i * 12;
        if (b->parent >= 0) mul(g_bone_pose + b->parent * 12, b->local, out);
        else {
            for (int j = 0; j < 12; j++) out[j] = b->rest[j];
            for (int j = 0; j < 3; j++) out[j * 4 + 3] += delta[j];
        }
        if (i == g_anchor) for (int j = 0; j < 3; j++) out[j * 4 + 3] = b->rest[j * 4 + 3] + delta[j];
        if (b->driver >= 0) {
            const float *m = driver + b->driver * 12;
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++) {
                    float q = 0;
                    for (int k = 0; k < 3; k++) q += m[r * 4 + k] * b->rest[k * 4 + c];
                    out[r * 4 + c] = q;
                }
        }
        float temp[9];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) {
                float q = 0;
                for (int k = 0; k < 3; k++) q += out[r * 4 + k] * b->hand[k * 3 + c];
                temp[r * 3 + c] = q;
            }
        for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) out[r * 4 + c] = temp[r * 3 + c];
    }
}

/* rtg_evaluate's skinning, one skin matrix per bone. The three rows go through the SSE
 * unit side by side: each lane does its row's operations in the core's order, single
 * precision as the scalar code, so the sums are the same to the bit (checked against
 * the core at load). Columns: g_column[bone][k] holds the matrix's column k. */
static __m128 g_column[MAX_BONES][4];

static int skin(void)
{
    for (int i = 0; i < g_bones; i++) {
        float *s = g_skin + i * 12;
        mul(g_bone_pose + i * 12, g_bone[i].inverse, s);
        for (int k = 0; k < 4; k++) g_column[i][k] = _mm_setr_ps(s[k], s[4 + k], s[8 + k], 0);
    }
    for (int i = 0; i < g_vertices; i++) {
        const Vertex *v = &g_vertex[i];
        const float *p = v->p;
        __m128 p0 = _mm_set1_ps(p[0]), p1 = _mm_set1_ps(p[1]), p2 = _mm_set1_ps(p[2]);
        __m128 p3 = _mm_set1_ps(p[3]), p4 = _mm_set1_ps(p[4]), p5 = _mm_set1_ps(p[5]);
        __m128 position = _mm_setzero_ps(), normal = _mm_setzero_ps();
        for (uint32_t j = 0; j < v->count; j++) {
            const Weight *w = &g_weight[v->first + j];
            const __m128 *m = g_column[w->bone];
            __m128 weight = _mm_set1_ps(w->weight);
            __m128 at = _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(m[0], p0), _mm_mul_ps(m[1], p1)), _mm_mul_ps(m[2], p2)), m[3]);
            __m128 turned = _mm_add_ps(_mm_add_ps(_mm_mul_ps(m[0], p3), _mm_mul_ps(m[1], p4)), _mm_mul_ps(m[2], p5));
            position = _mm_add_ps(position, _mm_mul_ps(weight, at));
            normal = _mm_add_ps(normal, _mm_mul_ps(weight, turned));
        }
        float result[6], lanes[4];
        _mm_storeu_ps(lanes, position);
        result[0] = lanes[0]; result[1] = lanes[1]; result[2] = lanes[2];
        _mm_storeu_ps(lanes, normal);
        result[3] = lanes[0]; result[4] = lanes[1]; result[5] = lanes[2];
        float n = result[3] * result[3] + result[4] * result[4] + result[5] * result[5];
        if (n > 1e-12f) { float a = 1 / __builtin_sqrtf(n); for (int j = 3; j < 6; j++) result[j] *= a; }
        for (int j = 0; j < 6; j++) {
            uint32_t bits;
            memcpy(&bits, &result[j], 4);
            if ((bits & 0x7f800000u) == 0x7f800000u) return 0;
            g_world[i * 6 + j] = result[j];
        }
    }
    return 1;
}

/* Pose, skin and bring every page into its driver part's space (Evaluate). */
static int evaluate(const float *drivers)
{
    for (int j = 0; j < 216; j++) if (!finite(drivers[j])) return 0;
    pose_rig(drivers);
    if (!skin()) return 0;
    for (int i = 0; i < g_pages; i++) {
        int count = g_packet_count[i];
        if (!count) continue;
        for (int v = 0; v < count; v++) memcpy(&g_packet_world[v * 6], &g_world[g_packet_index[g_packet_first[i] + v] * 6], 24);
        float *local = &g_local[g_packet_first[i] * 6];
        if (rtg_to_part(drivers + g_packet_driver[i] * 12, g_packet_world, (unsigned)count, local)) return 0;
        for (int v = 0; v < count; v++)
            for (int k = 0; k < 3; k++) {
                float x = local[v * 6 + k];
                if (!finite(x) || x > 32760.0f || x < -32760.0f) return 0;
            }
    }
    return 1;
}

/* ---- the hooks (SuitRetargeting) ----------------------------------------------------- */
static float g_driver[18 * 12];
static uint32_t g_mesh[MAX_PAGES];
static uint32_t g_paged_actor, g_paged_pose, g_paged_stack, g_paged_table, g_part_visibility;
static int g_page_count, g_next_page;
volatile uint64_t rt_rig_cycles;
volatile uint32_t rt_rig_calls;

static int ram(uint32_t p, uint32_t n) { return n <= 0x800000u && p >= 0x80000000u && p <= 0x80800000u - n; }
static int active(void) { return g_rig && assets_sm1_suit_loaded(); }

/* Round half to even, as the PC's MathF.Round (nxdk's lroundf asserts): the SSE
 * conversion in the default rounding mode, nearest-even. */
static int16_t q16(float x) { return (int16_t)_mm_cvtss_si32(_mm_set_ss(x)); }

static int is_spidey(uint32_t entry) { return mem_r32(entry) == 0x64697073u && mem_r16(entry + 4) == 0x7965u && mem_r8(entry + 6) == 0; }

static void read_drivers(uint32_t pose, float *out)
{
    for (int i = 0; i < 18; i++) {
        uint32_t p = pose + (uint32_t)i * 24u;
        for (int r = 0; r < 3; r++) {
            for (int k = 0; k < 3; k++) out[i * 12 + r * 4 + k] = (int16_t)mem_r16(p + (uint32_t)(r * 3 + k) * 2u) / 4096.0f;
            out[i * 12 + r * 4 + 3] = (int16_t)mem_r16(p + 18u + (uint32_t)r * 2u);
        }
    }
}

static void write_normal(uint32_t p, float x, float y, float z)
{
    float n = x * x + y * y + z * z;
    if (n > 1e-12f) { float a = 1.0f / __builtin_sqrtf(n); x *= a; y *= a; z *= a; }
    else { x = 0; y = -1; z = 0; }
    mem_w16(p, (uint16_t)q16(x * 4096));
    mem_w16(p + 2, (uint16_t)q16(y * 4096));
    mem_w16(p + 4, (uint16_t)q16(z * 4096));
}

/* Instruction hook at 80010678: packed native animation counts meshes; the authored
 * stream still has exactly the original 18 drivers. */
void hook_SuitRetargeting_UsePreservedAnimationCount(CpuContext *c)
{
    if (!active() || g_pages <= 18 || !ram(S5, 0x198)) return;
    uint32_t slot = mem_r8(S5 + 0x1B), entry = 0x800A0904u + slot * 64u;
    if (slot >= 40 || FP != entry || !is_spidey(entry)) return;
    uint32_t table = mem_r32(entry + 0x10);
    if (!ram(table - 4, 4 + (uint32_t)g_pages * 4) || mem_r32(table - 4) != (uint32_t)g_pages || S3 != (uint32_t)g_pages) {
        rt_note("suit rig: paged animation count differs from resident meshes");
        return;
    }
    S3 = 18;
}

/* Instruction hook at 80077418: the native pose is ready; skin the suit onto it. */
static void hook_SuitRetargeting_ApplyPose_timed(CpuContext *c)
{
    g_page_count = 0;
    if (!active()) return;
    uint32_t actor = S2, pose = S3;
    if (!ram(actor, 0x198) || !ram(pose, 18 * 24)) return;
    uint32_t slot = mem_r8(actor + 0x1B);
    if (slot >= 40) return;
    uint32_t entry = 0x800A0904u + slot * 64u;
    if (!is_spidey(entry)) return;
    uint32_t table = mem_r32(entry + 0x10);
    int pages = g_pages;
    if (pages < 18 || !ram(table - 4, 4 + (uint32_t)pages * 4) || mem_r32(table - 4) != (uint32_t)pages) return;
    read_drivers(pose, g_driver);
    for (int i = 0; i < pages; i++) {
        g_mesh[i] = mem_r32(table + (uint32_t)i * 4);
        if (!ram(g_mesh[i], 28) || mem_r16(g_mesh[i] + 2) != (uint32_t)g_packet_count[i]) { rt_note("suit rig does not match the resident mesh"); return; }
        uint32_t nv = mem_r16(g_mesh[i] + 2), nn = mem_r16(g_mesh[i] + 4), nf = mem_r16(g_mesh[i] + 6);
        if (nn != nv + nf || !ram(g_mesh[i], 28 + nv * 8 + nn * 8 + nf * 36)) { rt_note("suit rig: invalid mesh bounds"); return; }
        uint32_t faces = g_mesh[i] + 28 + (nv + nn) * 8;
        for (uint32_t f = 0; f < nf; f++)
            if (mem_r8(faces + f * 36 + 4) >= nv || mem_r8(faces + f * 36 + 5) >= nv || mem_r8(faces + f * 36 + 6) >= nv) {
                rt_note("suit rig: invalid face"); return;
            }
    }
    /* All validation and evaluation precede guest writes. */
    uint64_t t0 = __rdtsc();
    int ok = evaluate(g_driver);
    rt_rig_cycles += __rdtsc() - t0;
    rt_rig_calls++;
    rt_metric[0] = (uint32_t)(rt_rig_cycles / rt_rig_calls / 733u);   /* microseconds at 733 MHz */
    rt_metric[1] = rt_rig_calls;
    if (!ok) { rt_note("suit rig evaluation failed"); return; }
    for (int i = 0; i < pages; i++) {
        const float *v = &g_local[g_packet_first[i] * 6];
        uint32_t mesh = g_mesh[i];
        int count = g_packet_count[i];
        uint32_t start = mesh + 28, normals = start + (uint32_t)count * 8;
        int nf = (int)mem_r16(mesh + 6);
        uint32_t faces = normals + (uint32_t)(count + nf) * 8;
        /* The largest length, as one root of the largest square: a correctly rounded
         * root never decreases, so this is the largest of the roots. */
        float radius2 = 0, mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
        for (int j = 0; j < count; j++) {
            const float *p = &v[j * 6];
            float len2 = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
            if (len2 > radius2) radius2 = len2;
            for (int k = 0; k < 3; k++) { if (p[k] < mn[k]) mn[k] = p[k]; if (p[k] > mx[k]) mx[k] = p[k]; }
            uint32_t address = start + (uint32_t)j * 8;
            mem_w16(address, (uint16_t)q16(p[0]));
            mem_w16(address + 2, (uint16_t)q16(p[1]));
            mem_w16(address + 4, (uint16_t)q16(p[2]));
            write_normal(normals + (uint32_t)j * 8, p[3], p[4], p[5]);
        }
        for (int j = 0; j < nf; j++) {
            uint32_t f = faces + (uint32_t)j * 36;
            /* Native on-disk winding is (a, c, b). */
            int a = (int)mem_r8(f + 4), b = (int)mem_r8(f + 6), d = (int)mem_r8(f + 5);
            if (a >= count || b >= count || d >= count) { rt_note("suit rig: invalid face"); return; }
            const float *pa = &v[a * 6], *pb = &v[b * 6], *pd = &v[d * 6];
            float e1[3] = { pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2] }, e2[3] = { pd[0] - pa[0], pd[1] - pa[1], pd[2] - pa[2] };
            write_normal(normals + (uint32_t)(count + j) * 8, e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                         e1[0] * e2[1] - e1[1] * e2[0]);
        }
        /* Culling bounds follow the skin. */
        float radius = __builtin_sqrtf(radius2);
        mem_w32(mesh + 8, (uint32_t)(-floor(-(double)radius)) * 256u);
        if (count > 0)
            for (int k = 0; k < 3; k++) {
                mem_w16(mesh + 12 + (uint32_t)k * 4, (uint16_t)(int16_t)(-floor(-(double)mx[k] / 16)));
                mem_w16(mesh + 14 + (uint32_t)k * 4, (uint16_t)(int16_t)floor((double)mn[k] / 16));
            }
    }
    if (pages > 18) {
        g_paged_actor = actor; g_paged_pose = pose; g_paged_stack = SP; g_paged_table = table;
        g_page_count = pages; g_next_page = pages;
    }
}

void hook_SuitRetargeting_ApplyPose(CpuContext *c)
{
    PROF_BEGIN(t);
    hook_SuitRetargeting_ApplyPose_timed(c);
    PROF_END(PROF_RIG, t);
}


static int paged_context(CpuContext *c)
{
    return g_page_count > 18 && S2 == g_paged_actor && SP == g_paged_stack && S1 < 18 &&
           S3 == g_paged_pose + S1 * 24 && mem_r32(SP + 0xA8) == g_paged_table;
}

/* Branch hook at 800775B0 (resume 800776C8): extra pages are geometry, not parts or LODs. */
int hook_SuitRetargeting_BeginPagedPart(CpuContext *c)
{
    if (!paged_context(c)) return 0;
    if (S0 != g_mesh[S1]) { rt_note("suit rig: paged actor table changed during draw"); return 0; }
    g_next_page = S1 == 0 ? 18 : g_page_count;
    g_part_visibility = mem_r32(GP + 0x1164);
    return 1;
}

/* Branch hook at 800779B0 (resume 800776C8): draw the next extra page under driver 0. */
int hook_SuitRetargeting_TryNextPage(CpuContext *c)
{
    if (!paged_context(c) || S1 != 0 || g_next_page >= g_page_count) return 0;
    S0 = g_mesh[g_next_page++];
    mem_w32(GP + 0x1164, g_part_visibility);
    return 1;
}

/* ---- the swing web (SuitWebAttachment, instruction hook at 8002CF80) ----------------- */
static uint32_t g_line, g_actor, g_count;
static int g_valid, g_target[3], g_tip[3];

static void vec(uint32_t p, int *v) { for (uint32_t i = 0; i < 3; i++) v[i] = (int)mem_r32(p + i * 4); }

static int current_pose(uint32_t actor, uint32_t entry, uint32_t *pose)
{
    if (mem_r16(actor) & 4) *pose = mem_r32(actor + 0x180);
    else {
        uint32_t anim = mem_r32(entry + 0x18);
        if (!ram(anim - 8, 12) || mem_r32(anim - 8) != 0x2C || mem_r16(actor + 0x136) != mem_r16(actor + 0x126) ||
            mem_r16(actor + 0x138) != mem_r16(actor + 0x124)) return 0;
        *pose = mem_r32(actor + 0x130);
    }
    return ram(*pose, 18 * 24);
}

/* After a line segment's two world positions reach scratch RAM, before projection:
 * bend the drawn web onto the rig's wrist. Projection inputs only. */
void hook_SuitWebAttachment_ProjectSwingSegment(CpuContext *c)
{
    uint32_t line = S0, segment = S6;
    if (segment == 0) g_valid = 0;
    if (!active()) { g_valid = 0; return; }
    uint32_t actor = mem_r32(0x800B5268u);
    if (!ram(actor, 0xF88) || !ram(line, 0x78)) return;
    uint32_t controller = mem_r32(actor + 0xF84);
    if (!ram(controller, 0x18C)) return;
    uint32_t primary = mem_r32(controller + 0x178);
    if (!ram(primary, 0x78) || (line != primary && line != mem_r32(primary + 0x68))) return;
    uint32_t count = mem_r32(line + 0x3C), points = mem_r32(line + 0x40);
    if (!count || count > 4096 || segment >= count || !ram(points, count * 16)) return;
    if (segment == 0 || !g_valid || g_line != line || g_actor != actor || g_count != count) {
        g_valid = 0;
        uint32_t slot = mem_r8(actor + 0x1B);
        if (slot >= 40) return;
        uint32_t entry = 0x800A0904u + slot * 64u, pose;
        if (!is_spidey(entry) || !current_pose(actor, entry, &pose)) return;
        float drivers[18 * 12];
        read_drivers(pose, drivers);
        short rotation[9];
        int translation[3], position[3];
        for (uint32_t i = 0; i < 9; i++) rotation[i] = (short)mem_r16(actor + 0x160 + i * 2);
        vec(actor + 0x174, translation); vec(actor + 4, position);
        /* Native hand selection (80046444): clip 0x118 uses part 5, others part 10. */
        unsigned part = mem_r16(actor + 0x126) == 0x118 ? 5u : 10u;
        for (int j = 0; j < 216; j++) if (!finite(drivers[j])) return;
        pose_rig(drivers);
        if (rtg_web_target(g_rig, g_rig_len, g_bone_pose, (unsigned)g_bones * 12u, part, rotation, translation, position,
                           (mem_r32(actor + 0x128) & 2) != 0, g_target)) return;
        vec(points + (count - 1) * 16, g_tip);
        g_line = line; g_actor = actor; g_count = count; g_valid = 1;
    }
    /* Original points, not the previous segment's modified scratch endpoint. */
    int start[3], end[3], result[6];
    vec(segment == 0 ? line + 0x44 : points + (segment - 1) * 16, start);
    vec(points + segment * 16, end);
    if (rtg_web_segment(start, end, g_tip, g_target, segment, count, result)) return;
    for (uint32_t i = 0; i < 6; i++) mem_w32(0x1F800000u + i * 4, (uint32_t)result[i]);
}

void retarget_sm1_install(void)
{
    const char *rig = suit_rig_file();
    if (!rig || !load_rig(rig)) return;
    rt_note("suit rig loaded");
    rt_metric_name[0] = "suit rig microseconds per pose";
    rt_metric_name[1] = "suit rig poses";
    /* The unpacked evaluation must match the core exactly: compare once on a pose. */
    float drivers[18 * 12];
    for (int i = 0; i < 18; i++)
        for (int k = 0; k < 12; k++) drivers[i * 12 + k] = (k == 0 || k == 5 || k == 10) ? 1.0f : k == 3 ? (float)(i * 7) : 0.0f;
    static float reference[MAX_VERTICES * 6];
    if (rtg_evaluate(g_rig, g_rig_len, drivers, 0, g_bone_pose, (unsigned)g_bones * 12u, reference, (unsigned)g_vertices * 6u)) return;
    pose_rig(drivers);
    if (skin() && !memcmp(reference, g_world, (size_t)g_vertices * 6u * sizeof(float))) rt_note("suit rig evaluation matches the core");
    else rt_note("suit rig evaluation DIFFERS from the core");
}
