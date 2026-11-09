/*
 * Suit models with their own skeleton (the Spider-Man Unlimited conversions): a port
 * of the PC's SuitRetargeting.cs, SuitWebAttachment.cs (spiderman/patches) and
 * NativeRetargetRig.cs (RecompOne.Runtime/Assets/Suits). A fix to either belongs in
 * both places. The rig engine (posing, skinning, writing the meshes, the web maths) is
 * shared with Spider-Man 2: src/rig.c, around the PC's own retarget core.
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
#include "rig.h"
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

/* ---- the hooks (SuitRetargeting) ----------------------------------------------------- */
static float g_driver[18 * 12];
static uint32_t g_paged_actor, g_paged_pose, g_paged_stack, g_paged_table, g_part_visibility;
static int g_page_count, g_next_page;

static int ram(uint32_t p, uint32_t n) { return rig_ram(p, n); }
static int active(void) { return rig_loaded() && assets_sm1_suit_loaded(); }

static int is_spidey(uint32_t entry) { return mem_r32(entry) == 0x64697073u && mem_r16(entry + 4) == 0x7965u && mem_r8(entry + 6) == 0; }

/* Instruction hook at 80010678: packed native animation counts meshes; the authored
 * stream still has exactly the original 18 drivers. */
void hook_SuitRetargeting_UsePreservedAnimationCount(CpuContext *c)
{
    if (!active() || rig_pages() <= 18 || !ram(S5, 0x198)) return;
    uint32_t slot = mem_r8(S5 + 0x1B), entry = 0x800A0904u + slot * 64u;
    if (slot >= 40 || FP != entry || !is_spidey(entry)) return;
    uint32_t table = mem_r32(entry + 0x10);
    if (!ram(table - 4, 4 + (uint32_t)rig_pages() * 4) || mem_r32(table - 4) != (uint32_t)rig_pages() || S3 != (uint32_t)rig_pages()) {
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
    int pages = rig_pages();
    if (pages < 18 || !ram(table - 4, 4 + (uint32_t)pages * 4) || mem_r32(table - 4) != (uint32_t)pages) return;
    rig_read_drivers(pose, g_driver);
    if (!rig_apply(table, pages, g_driver)) return;
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
    if (S0 != rig_mesh((int)S1)) { rt_note("suit rig: paged actor table changed during draw"); return 0; }
    g_next_page = S1 == 0 ? 18 : g_page_count;
    g_part_visibility = mem_r32(GP + 0x1164);
    return 1;
}

/* Branch hook at 800779B0 (resume 800776C8): draw the next extra page under driver 0. */
int hook_SuitRetargeting_TryNextPage(CpuContext *c)
{
    if (!paged_context(c) || S1 != 0 || g_next_page >= g_page_count) return 0;
    S0 = rig_mesh(g_next_page++);
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
        rig_read_drivers(pose, drivers);
        short rotation[9];
        int translation[3], position[3];
        for (uint32_t i = 0; i < 9; i++) rotation[i] = (short)mem_r16(actor + 0x160 + i * 2);
        vec(actor + 0x174, translation); vec(actor + 4, position);
        /* Native hand selection (80046444): clip 0x118 uses part 5, others part 10. */
        unsigned part = mem_r16(actor + 0x126) == 0x118 ? 5u : 10u;
        if (!rig_web_target(drivers, part, rotation, translation, position, (mem_r32(actor + 0x128) & 2) != 0, g_target)) return;
        vec(points + (count - 1) * 16, g_tip);
        g_line = line; g_actor = actor; g_count = count; g_valid = 1;
    }
    /* Original points, not the previous segment's modified scratch endpoint. */
    int start[3], end[3], result[6];
    vec(segment == 0 ? line + 0x44 : points + (segment - 1) * 16, start);
    vec(points + segment * 16, end);
    if (!rig_web_segment(start, end, g_tip, g_target, segment, count, result)) return;
    for (uint32_t i = 0; i < 6; i++) mem_w32(0x1F800000u + i * 4, (uint32_t)result[i]);
}

