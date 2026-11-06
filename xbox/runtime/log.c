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
    ADD("hw frames %lu decodes %lu palettes %lu wait %lu\n", (unsigned long)rt_hw_frames, (unsigned long)rt_hw_texture_decodes,
        (unsigned long)rt_hw_palette_builds,
        (unsigned long)rt_hw_wait);
    ADD("where %s\n", (const char *)rt_where);
    MM_STATISTICS mm = { sizeof mm };
    MmQueryStatistics(&mm);
    ADD("memory free %lu KB of %lu KB\n", (unsigned long)mm.AvailablePages * 4ul, (unsigned long)mm.TotalPhysicalPages * 4ul);
    VIDEO_MODE vm = XVideoGetMode();
    ADD("mode %dx%d encoder %08lX\n", vm.width, vm.height, (unsigned long)XVideoGetEncoderSettings());
    ADD("video 720p %d wide-display %d frame-wide %d fov %d/%d\n", rt_video_720p, rt_video_wide, rt_frame_wide,
        rt_fov_num, rt_fov_den);
    ADD("textured triangles perspective %lu affine %lu replaced %lu\n", (unsigned long)rt_hw_perspective, (unsigned long)rt_hw_affine,
        (unsigned long)rt_hw_replaced);
    ADD("vertex tags pages %lu stores %lu refused %lu; textured polygons precise %lu imprecise %lu\n",
        (unsigned long)rt_vtag_pages, (unsigned long)rt_vtag_stores, (unsigned long)rt_vtag_refused,
        (unsigned long)rt_hw_precise, (unsigned long)rt_hw_imprecise);
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
    ADD("texture pack hits %lu loads %lu\n", (unsigned long)rt_pack_hits, (unsigned long)rt_pack_loads);
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
