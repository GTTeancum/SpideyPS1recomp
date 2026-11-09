#include <hal/debug.h>
#include <hal/video.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "gpu.h"
#include "prof.h"
#include "runtime.h"

#define MISSING_MAX 96
#define LOG_LINES 4

static struct { const char *kind, *name; unsigned count; } g_missing[MISSING_MAX];
static int g_missing_count;

/* The last few log lines, redrawn with the status screen. */
static char g_lines[LOG_LINES][96];
static int g_line_next;
static unsigned g_status_draws;

volatile int g_stopped;

/* Named counters (rt_note) and the last few dispatched addresses. */
static struct { const char *name; unsigned long count; } g_notes[32];
static int g_note_count;
uint32_t rt_recent_calls[RECENT];
unsigned rt_recent_next;

void rt_note(const char *name)
{
    for (int i = 0; i < g_note_count; i++)
        if (g_notes[i].name == name) { g_notes[i].count++; return; }
    if (g_note_count < 32) { g_notes[g_note_count].name = name; g_notes[g_note_count].count = 1; g_note_count++; }
}

volatile int32_t rt_mem_kb[MEM_USES], rt_mem_peak_kb[MEM_USES];
static volatile uint32_t g_mem_low_kb = 0xFFFFFFFFu, g_mem_low_vblank;
static struct { const char *stage; uint32_t free_kb; } g_mem_marks[8];
static int g_mem_mark_count;

static uint32_t free_kb(void)
{
    MM_STATISTICS mm = { sizeof mm };
    MmQueryStatistics(&mm);
    return (uint32_t)mm.AvailablePages * 4u;
}

void rt_mem_add(int use, int32_t bytes)
{
    int32_t kb = (bytes < 0 ? -1 : 1) * (int32_t)((((uint32_t)(bytes < 0 ? -bytes : bytes)) + 4095u) / 4096u * 4u);
    rt_mem_kb[use] += kb;
    if (rt_mem_kb[use] > rt_mem_peak_kb[use]) rt_mem_peak_kb[use] = rt_mem_kb[use];
}

void rt_mem_mark(const char *stage)
{
    if (g_mem_mark_count < 8) { g_mem_marks[g_mem_mark_count].stage = stage; g_mem_marks[g_mem_mark_count].free_kb = free_kb(); g_mem_mark_count++; }
    rt_mem_sample();
}

void rt_mem_sample(void)
{
    uint32_t kb = free_kb();
    if (kb < g_mem_low_kb) { g_mem_low_kb = kb; g_mem_low_vblank = rt_vblank_count; }
}

void rt_log(const char *format, ...)
{
    char *line = g_lines[g_line_next % LOG_LINES];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof g_lines[0], format, args);
    va_end(args);
    g_line_next++;
}

unsigned *rt_missing(const char *kind, const char *name)
{
    /* Callers pass string constants: the same pointer first, then the same text. */
    for (int i = 0; i < g_missing_count; i++)
        if (g_missing[i].name == name) { g_missing[i].count++; return &g_missing[i].count; }
    for (int i = 0; i < g_missing_count; i++)
        if (strcmp(g_missing[i].name, name) == 0) { g_missing[i].count++; return &g_missing[i].count; }
    if (g_missing_count < MISSING_MAX) {
        g_missing[g_missing_count].kind = kind;
        g_missing[g_missing_count].name = name;
        g_missing[g_missing_count].count = 1;
        return &g_missing[g_missing_count++].count;
    }
    return 0;
}

static const char *short_name(const char *name)
{
    static const char *prefixes[] = { "RecompOne.Runtime.Sdk.", "RecompOne.Runtime.", "Recompiled." };
    for (unsigned i = 0; i < sizeof prefixes / sizeof prefixes[0]; i++) {
        size_t n = strlen(prefixes[i]);
        if (strncmp(name, prefixes[i], n) == 0) return name + n;
    }
    return name;
}

/*
 * The full status as text, rebuilt with every redraw. The test tools copy it out
 * through the XEMU monitor (xbox/tools/run_xemu.py) using its address from the
 * link map; it starts with a marker so a wrong address is obvious.
 */
volatile uint32_t rt_metric[RT_METRICS];
const char *rt_metric_name[RT_METRICS];

char g_status_text[16384] = "OPENSPIDEY-STATUS";

static void status_text(void)
{
    char *p = g_status_text, *end = g_status_text + sizeof g_status_text - 1;
#define ADD(...) do { if (p < end) p += snprintf(p, (size_t)(end - p), __VA_ARGS__); } while (0)
    ADD("OPENSPIDEY-STATUS\nstatus %u dispatches %lu%s\n", g_status_draws,
        (unsigned long)rt_dispatch_count, g_stopped ? " STOPPED" : "");
    int first = g_line_next > LOG_LINES ? g_line_next - LOG_LINES : 0;
    for (int i = first; i < g_line_next; i++) ADD("log %s\n", g_lines[i % LOG_LINES]);
    for (int i = 0; i < g_missing_count; i++)
        ADD("missing %s %s %u\n", g_missing[i].kind, g_missing[i].name, g_missing[i].count);
    GpuDisplay d;
    gpu_display(&d);
    ADD("vblanks %lu display %d,%d %dx%d enabled %d rgb24 %d presenting %d\n", (unsigned long)rt_vblank_count,
        d.x, d.y, d.width, d.height, d.enabled, d.rgb24, rt_presenting);
    ADD("ticks dropped %lu longest gap %lu ms\n", (unsigned long)rt_ticks_dropped, (unsigned long)rt_longest_wait_ms);
    {
        extern volatile uint32_t rt_cd_blocks, rt_cd_us;
        ADD("disc blocks read %lu in %lu ms\n", (unsigned long)rt_cd_blocks, (unsigned long)(rt_cd_us / 1000));
    }
    {
        extern volatile uint32_t rt_hw_push_frame_peak;
        ADD("push buffer largest frame %lu KB\n", (unsigned long)(rt_hw_push_frame_peak / 1024u));
        extern volatile uintptr_t rt_game_stack_base, rt_game_stack_limit;
        if (rt_game_stack_limit) {
            /* Deepest the game thread's stack has reached: the lowest word no longer
             * holding the mark main.c wrote below its first frame. Read only. */
            const volatile uint32_t *w = (const volatile uint32_t *)rt_game_stack_limit;
            while ((uintptr_t)w < rt_game_stack_base && *w == 0x5AFE57ACu) w++;
            extern volatile uintptr_t rt_tick_sp_low;
            ADD("game stack used %lu KB of %lu KB (base %08lX limit %08lX, tick frame low %08lX)\n",
                (unsigned long)((rt_game_stack_base - (uintptr_t)w) / 1024u),
                (unsigned long)((rt_game_stack_base - rt_game_stack_limit) / 1024u),
                (unsigned long)rt_game_stack_base, (unsigned long)rt_game_stack_limit, (unsigned long)rt_tick_sp_low);
        }
    }
    ADD("hw frames %lu decodes %lu palettes %lu wait %lu\n", (unsigned long)rt_hw_frames, (unsigned long)rt_hw_texture_decodes,
        (unsigned long)rt_hw_palette_builds,
        (unsigned long)rt_hw_wait);
    ADD("where %s\n", (const char *)rt_where);
    MM_STATISTICS mm = { sizeof mm };
    MmQueryStatistics(&mm);
    ADD("memory free %lu KB of %lu KB\n", (unsigned long)mm.AvailablePages * 4ul, (unsigned long)mm.TotalPhysicalPages * 4ul);
    ADD("memory low %lu KB at vblank %lu\n", (unsigned long)g_mem_low_kb, (unsigned long)g_mem_low_vblank);
    for (int i = 0; i < g_mem_mark_count; i++) ADD("memory at %s %lu KB\n", g_mem_marks[i].stage, (unsigned long)g_mem_marks[i].free_kb);
    {
        static const char *const names[MEM_USES] = { "textures", "palettes", "pack", "suits", "movie" };
        for (int i = 0; i < MEM_USES; i++)
            ADD("memory use %s %ld KB peak %ld KB\n", names[i], (long)rt_mem_kb[i], (long)rt_mem_peak_kb[i]);
        extern volatile int32_t rt_movie_state[8];
        ADD("movie state shown %ld video-done %ld flushed %ld sound %ld busy %ld ended %ld queued %ld clock %ld ms\n",
            (long)rt_movie_state[0], (long)rt_movie_state[1], (long)rt_movie_state[2], (long)rt_movie_state[3],
            (long)rt_movie_state[4], (long)rt_movie_state[5], (long)rt_movie_state[6], (long)rt_movie_state[7]);
        extern volatile uint32_t rt_arena_peak;
        ADD("memory asset arena peak %lu KB of %lu KB\n", (unsigned long)(rt_arena_peak / 1024u), 0x480000ul / 1024ul);
    }
    VIDEO_MODE vm = XVideoGetMode();
    ADD("mode %dx%d encoder %08lX\n", vm.width, vm.height, (unsigned long)XVideoGetEncoderSettings());
    ADD("video 720p %d wide-display %d frame-wide %d fov %d/%d\n", rt_video_720p, rt_video_wide, rt_frame_wide,
        rt_fov_num, rt_fov_den);
    ADD("textured triangles perspective %lu affine %lu replaced %lu\n", (unsigned long)rt_hw_perspective, (unsigned long)rt_hw_affine,
        (unsigned long)rt_hw_replaced);
    {
        extern volatile uint32_t rt_crack_frames;
        ADD("gap fill frames %lu\n", (unsigned long)rt_crack_frames);
        extern volatile uint32_t rt_census_frames, rt_census_near[3], rt_census_tjunc[3], rt_census_polys, rt_census_sample[6][6];
        if (rt_census_frames) {
            ADD("census frames %lu polys %lu near %lu/%lu/%lu tjunc %lu/%lu/%lu\n", (unsigned long)rt_census_frames,
                (unsigned long)rt_census_polys, (unsigned long)rt_census_near[0], (unsigned long)rt_census_near[1],
                (unsigned long)rt_census_near[2], (unsigned long)rt_census_tjunc[0], (unsigned long)rt_census_tjunc[1],
                (unsigned long)rt_census_tjunc[2]);
            for (int i = 0; i < 6; i++)
                ADD("census sample %02lX a %ld,%ld b %ld,%ld addr %08lX\n", (unsigned long)rt_census_sample[i][0],
                    (long)(int32_t)rt_census_sample[i][1], (long)(int32_t)rt_census_sample[i][2], (long)(int32_t)rt_census_sample[i][3],
                    (long)(int32_t)rt_census_sample[i][4], (unsigned long)rt_census_sample[i][5]);
        }
    }
    {
        extern volatile uint32_t rt_spu_peak, rt_spu_voices;
        extern int spu_xa_buffered(void);
        ADD("sound peak %lu voices %lu xa buffered %d\n", (unsigned long)rt_spu_peak, (unsigned long)rt_spu_voices, spu_xa_buffered());
        rt_spu_peak = 0;
        rt_spu_voices = 0;
    }
    ADD("vertex tags pages %lu stores %lu refused %lu; textured polygons precise %lu imprecise %lu\n",
        (unsigned long)rt_vtag_pages, (unsigned long)rt_vtag_stores, (unsigned long)rt_vtag_refused,
        (unsigned long)rt_hw_precise, (unsigned long)rt_hw_imprecise);
    {
        extern volatile uint32_t rt_vtag_miss[5], rt_vtag_miss_addr[8], rt_vtag_miss_word[8];
        ADD("untagged world vertices no-address %lu no-page %lu cleared %lu changed %lu other-word %lu; last",
            (unsigned long)rt_vtag_miss[0], (unsigned long)rt_vtag_miss[1], (unsigned long)rt_vtag_miss[2], (unsigned long)rt_vtag_miss[4],
            (unsigned long)rt_vtag_miss[3]);
        for (int i = 0; i < 8; i++) ADD(" %08lX:%08lX", (unsigned long)rt_vtag_miss_addr[i], (unsigned long)rt_vtag_miss_word[i]);
        ADD("%s", "\n");
    }
    {
        extern volatile uint32_t rt_card_ready, rt_card_writes, rt_card_files, rt_card_saves;
        ADD("memory card ready %lu saves %lu files opened %lu writes %lu\n", (unsigned long)rt_card_ready,
            (unsigned long)rt_card_saves, (unsigned long)rt_card_files, (unsigned long)rt_card_writes);
    }
    for (int i = 0; i < RT_METRICS; i++)
        if (rt_metric_name[i]) ADD("metric %s %lu\n", rt_metric_name[i], (unsigned long)rt_metric[i]);
    ADD("shown %08lX %lux%lu\n", (unsigned long)rt_hw_shown, (unsigned long)rt_hw_shown_w, (unsigned long)rt_hw_shown_h);
#ifdef RT_PROFILE
    {
        static const char *const names[PROF_COUNT] = { "gpu-list", "tex-convert", "present", "idle", "rig", "shadow", "cd", "irq", "tex-lookup", "replace", "submit", "depth", "gte" };
        static uint64_t last_total, last[PROF_COUNT];
        uint64_t now_prof[PROF_COUNT];
        /* The game thread updates these 64-bit counters as two 32-bit halves: re-read
         * until a value is stable, or a half-updated one gives nonsense. */
        for (int i = 0; i < PROF_COUNT; i++) { uint64_t a, b; do { a = rt_prof[i]; b = rt_prof[i]; } while (a != b); now_prof[i] = a; }
        uint64_t total = __rdtsc(), span = total - last_total;
        if (last_total && span) {
            ADD("time");
            uint64_t counted = 0;
            for (int i = 0; i < PROF_COUNT; i++) {
                uint64_t d = now_prof[i] - last[i];
                if (i <= PROF_IRQ && i != PROF_PAGE) counted += d;
                ADD(" %s %lu%%", names[i], (unsigned long)(d * 100 / span));
            }
            ADD(" game %lu%%\n", (unsigned long)((span > counted ? span - counted : 0) * 100 / span));
        }
        last_total = total;
        for (int i = 0; i < PROF_COUNT; i++) last[i] = now_prof[i];
    }
#endif
#ifdef RT_PROFILE
    {
        extern volatile uint64_t rt_gte_op_cycles[64];
        extern volatile uint32_t rt_gte_op_count[64];
        ADD("gte ops");
        for (int i = 0; i < 64; i++)
            if (rt_gte_op_count[i] > 1000)
                ADD(" %02x:%luk/%lumc", i, (unsigned long)(rt_gte_op_count[i] / 1000), (unsigned long)(rt_gte_op_cycles[i] / 1000000));
        ADD("%s", "\n");
    }
#endif
    {
        extern volatile uint32_t rt_hw_region_info[], rt_hw_region_swaps;
        ADD("regions %lu:", (unsigned long)rt_hw_region_swaps);
        for (int i = 0; i < 2; i++) ADD(" %lu,%lu w%lu", (unsigned long)(rt_hw_region_info[i] & 1023), (unsigned long)((rt_hw_region_info[i] >> 10) & 1023), (unsigned long)(rt_hw_region_info[i] >> 20));
        ADD("%s", "\n");
    }
    ADD("texture pack hits %lu loads %lu released %lu\n", (unsigned long)rt_pack_hits, (unsigned long)rt_pack_loads,
        (unsigned long)rt_pack_released);
    ADD("drawotag from %08lX at %08lX packets %lu\n", (unsigned long)rt_ot_start, (unsigned long)rt_ot_addr,
        (unsigned long)rt_ot_packets);
    for (int i = 0; i < g_note_count; i++) ADD("count %s %lu\n", g_notes[i].name, (unsigned long)g_notes[i].count);
    ADD("recent calls:");
    for (int i = 0; i < RECENT; i++) ADD(" %08lX", (unsigned long)rt_recent_calls[(rt_recent_next + i) % RECENT]);
    ADD("\n");
#undef ADD
    *p = 0;
}

void rt_show_status(void)
{
    ++g_status_draws;
    status_text();
    if (rt_presenting) return;   /* the game owns the screen now; the text still updates */
    /* The debug screen holds about 26 lines of 56 characters and wipes itself
     * when it overflows, so it shows a summary; the text above has everything. */
    debugClearScreen();
    debugPrint("OpenSpidey Xbox  %u  calls %lu%s\n", g_status_draws,
               (unsigned long)rt_dispatch_count, g_stopped ? "  STOPPED" : "");
    int first = g_line_next > LOG_LINES ? g_line_next - LOG_LINES : 0;
    for (int i = first; i < g_line_next; i++) debugPrint(" %.54s\n", g_lines[i % LOG_LINES]);
    debugPrint("missing services: %d (first 18)\n", g_missing_count);
    for (int i = 0; i < g_missing_count && i < 18; i++)
        debugPrint(" %-40.40s %u\n", short_name(g_missing[i].name), g_missing[i].count);
}

void rt_fatal(CpuContext *c, const char *format, ...)
{
    char line[96];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    rt_log("STOPPED: %s", line);
    if (c) {
        rt_log("a0=%08lX a1=%08lX a2=%08lX a3=%08lX", (unsigned long)c->r[4], (unsigned long)c->r[5],
               (unsigned long)c->r[6], (unsigned long)c->r[7]);
        rt_log("v0=%08lX v1=%08lX ra=%08lX sp=%08lX", (unsigned long)c->r[2], (unsigned long)c->r[3],
               (unsigned long)c->r[31], (unsigned long)c->r[29]);
    }
    g_stopped = 1;
    for (;;) Sleep(1000);
}
