/*
 * The PSY-Q pad library calls the configuration replaces, following the PC
 * runtime's LibPad: the pad state is written to the game's two buffers every vblank
 * (8 bytes: status, id, buttons low/high, right stick x/y, left stick x/y). Buttons
 * are active-low as on the console.
 *
 * Input comes from rt_pad_buttons (the Xbox controller, see src/input.c) combined
 * with an optional test script on the disc, input.txt: lines "<vblank> <buttons>
 * <hold>", buttons joined with '+' (start, select, cross, circle, square, triangle,
 * up, down, left, right, l1, l2, r1, r2), as in the PC's SPIDEY_SCRIPT.
 */
#include <stdio.h>
#include <string.h>
#include "runtime.h"

static uint32_t g_buf1, g_buf2;
static int g_analog = 1;
volatile uint16_t rt_pad_buttons = 0xFFFF;
volatile uint8_t rt_pad_sticks[4] = { 0x80, 0x80, 0x80, 0x80 };   /* rx, ry, lx, ly */

typedef struct { uint32_t at, hold; uint16_t mask; } Press;
static Press g_script[256];
static int g_script_count;

static uint16_t button(const char *name)
{
    static const struct { const char *name; uint16_t bit; } names[] = {
        { "select", 1 << 0 }, { "start", 1 << 3 }, { "up", 1 << 4 }, { "right", 1 << 5 },
        { "down", 1 << 6 }, { "left", 1 << 7 }, { "l2", 1 << 8 }, { "r2", 1 << 9 },
        { "l1", 1 << 10 }, { "r1", 1 << 11 }, { "triangle", 1 << 12 }, { "circle", 1 << 13 },
        { "cross", 1 << 14 }, { "square", 1 << 15 },
    };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
        if (rt_strcasecmp(names[i].name, name) == 0) return names[i].bit;
    return 0;
}

void rt_pad_load_script(const char *disc_root)
{
    char path[260], line[128];
    snprintf(path, sizeof path, "%s\\input.txt", disc_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f) && g_script_count < 256) {
        unsigned long at, hold;
        char names[96];
        if (sscanf(line, "%lu %95s %lu", &at, names, &hold) != 3) continue;
        uint16_t mask = 0;
        for (char *tok = strtok(names, "+"); tok; tok = strtok(0, "+")) mask |= button(tok);
        g_script[g_script_count].at = (uint32_t)at;
        g_script[g_script_count].hold = (uint32_t)hold;
        g_script[g_script_count].mask = mask;
        g_script_count++;
    }
    fclose(f);
    rt_log("input script: %d presses", g_script_count);
}

static uint16_t scripted(void)
{
    uint16_t held = 0;
    uint32_t now = rt_vblank_count;
    for (int i = 0; i < g_script_count; i++)
        if (now >= g_script[i].at && now < g_script[i].at + g_script[i].hold) held |= g_script[i].mask;
    return held;
}

static void write_pad(uint32_t buf, uint16_t buttons, int present)
{
    mem_w8(buf + 0, present ? 0x00 : 0xFF);
    mem_w8(buf + 1, present ? (g_analog ? 0x73 : 0x41) : 0xFF);
    mem_w8(buf + 2, (uint8_t)buttons);
    mem_w8(buf + 3, (uint8_t)(buttons >> 8));
    for (int i = 0; i < 4; i++) mem_w8(buf + 4u + (uint32_t)i, present ? rt_pad_sticks[i] : 0x80);
}

void rt_pad_set_buffers(uint32_t buf1, uint32_t buf2, int analog)
{
    g_buf1 = buf1;
    g_buf2 = buf2;
    g_analog = analog;
}

uint16_t rt_pad_state(void) { return (uint16_t)(rt_pad_buttons & ~scripted()); }

/* Every vblank. */
void rt_pad_refresh(void)
{
    uint16_t buttons = rt_pad_state();
    if (g_buf1) write_pad(g_buf1, buttons, 1);
    if (g_buf2) write_pad(g_buf2, 0xFFFF, 0);
}

#define A0 c->r[4]
#define V0 c->r[2]

void hook_LibPad_PadStartCom(CpuContext *c) { rt_pad_refresh(); V0 = 0; }
void hook_LibPad_PadStopCom(CpuContext *c) { V0 = 0; }
/* Port 1 is always connected and stable; port 2 is empty. */
void hook_LibPad_PadGetState(CpuContext *c) { V0 = (A0 & 0x10u) == 0 ? 6u : 0u; }
