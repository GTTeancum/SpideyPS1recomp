/*
 * Spider-Man 2's suits with their own skeleton (the Spider-Man Unlimited conversions):
 * ports of the PC's spiderman2/patches SuitRetargeting.cs and SuitWebAttachment.cs. A
 * fix to either belongs in both places. The rig engine is shared with Spider-Man 1
 * (src/rig.c).
 *
 * Instruction hook at 80083A44, once the game has posed an actor's 18 native parts and
 * before lighting and vertex transforms: for the player's actor (the shared "spidey"
 * record or the suit's private "spcuNN" one -- other 18-part actors, such as Shocker in
 * E1M4, pose here too) the suit's rig is posed from those drivers and written into the
 * resident meshes. SM2's suits have exactly 18 parts. The rig's wrist that frame is kept
 * for the swing web: instruction hook at 80032E7C bends the drawn segments of a web line
 * whose tip lies within 256 units of it onto it (projection inputs only).
 */
#include "rig.h"
#include "runtime.h"
#include "settings.h"

#define S1 c->r[17]
#define S2 c->r[18]
#define S3 c->r[19]
#define FP c->r[30]

#define MODEL_TABLE 0x800ACED8u
#define FRAME_COUNTER 0x800C247Cu

static int active(void) { return rig_loaded() && costume_sm2_suit_loaded(); }
static int ram(uint32_t p, uint32_t n) { return rig_ram(p, n); }

/* "spidey" (the shared player record) or "spcu.." (a dedicated custom suit actor). */
static int is_player_entry(uint32_t entry)
{
    return (mem_r32(entry) == 0x64697073u && mem_r16(entry + 4) == 0x7965u && mem_r8(entry + 6) == 0) ||
           (mem_r32(entry) == 0x75637073u && mem_r8(entry + 6) == 0);
}

static void vec(uint32_t p, int *v) { for (uint32_t i = 0; i < 3; i++) v[i] = (int)mem_r32(p + i * 4); }

static float g_driver[18 * 12];
static int g_web_valid, g_web_target[3];
static uint32_t g_web_actor, g_web_frame;

void hook_SuitRetargeting_ApplyPose(CpuContext *c)
{
    if (!active() || rig_pages() != 18) return;
    uint32_t actor = S2, pose = S3;
    if (!ram(actor, 0x180) || !ram(pose, 18 * 24)) return;
    uint32_t slot = mem_r8(actor + 0x1B);
    if (slot >= 40) return;
    uint32_t entry = MODEL_TABLE + slot * 64u;
    if (!is_player_entry(entry)) return;
    uint32_t table = mem_r32(entry + 0x10);
    if (!ram(table - 4, 4 + 18 * 4) || mem_r32(table - 4) != 18) return;
    rig_read_drivers(pose, g_driver);
    if (!rig_apply(table, 18, g_driver)) return;

    short rotation[9];
    int translation[3], position[3];
    for (uint32_t i = 0; i < 9; i++) rotation[i] = (short)mem_r16(actor + 0x148 + i * 2);
    vec(actor + 0x15C, translation);
    vec(actor + 4, position);
    unsigned part = mem_r16(actor + 0x10E) == 0x118 ? 5u : 10u;
    g_web_valid = rig_web_target(g_driver, part, rotation, translation, position, (mem_r32(actor + 0x110) & 2) != 0, g_web_target);
    g_web_actor = actor;
    g_web_frame = mem_r32(FRAME_COUNTER);
}

/* ---- the swing web (SuitWebAttachment.ProjectSwingSegment) ---------------------------- */
#define OWNERSHIP_RADIUS_Q12 (256LL * 4096)
static uint32_t g_line, g_actor, g_count;
static int g_valid, g_tip[3];

void hook_SuitWebAttachment_ProjectSwingSegment(CpuContext *c)
{
    uint32_t line = S1, segment = FP;
    if (segment == 0) g_valid = 0;
    if (!active() || !ram(line, 0x58)) return;
    uint32_t count = mem_r32(line + 0x3C), points = mem_r32(line + 0x40);
    if (!count || count > 4096 || segment >= count || !ram(points, count * 16)) return;
    if (segment == 0) {
        if (!g_web_valid || mem_r32(FRAME_COUNTER) != g_web_frame) return;
        vec(points + (count - 1) * 16, g_tip);
        long long d2 = 0;
        for (int i = 0; i < 3; i++) { long long d = (long long)g_web_target[i] - g_tip[i]; d2 += d * d; }
        if (d2 > OWNERSHIP_RADIUS_Q12 * OWNERSHIP_RADIUS_Q12) return;
        g_line = line; g_actor = g_web_actor; g_count = count; g_valid = 1;
    }
    if (!g_valid || g_line != line || !g_actor || g_count != count) return;
    int start[3], end[3], result[6];
    vec(segment == 0 ? line + 0x44 : points + (segment - 1) * 16, start);
    vec(points + segment * 16, end);
    if (!rig_web_segment(start, end, g_tip, g_web_target, segment, count, result)) return;
    for (uint32_t i = 0; i < 6; i++) mem_w32(0x1F800000u + i * 4, (uint32_t)result[i]);
}

