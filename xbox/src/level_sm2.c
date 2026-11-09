/*
 * Start a new game in a chosen level: a port of the PC's LevelSwitch.cs
 * (spiderman2/patches). A fix to either belongs in both places.
 *
 * The retail campaign descriptor for the target is selected before the stage loader
 * looks it up (func_8001A700, called from func_8001A85C), which keeps the level flags
 * and transitions together with its files. For targets without a descriptor, archive
 * lookups for the first non-demo level prefix the game asks for are rewritten to the
 * target; only that prefix is ever rewritten, and never once a descriptor was selected.
 *
 *   level=e3m1 in D:\openspidey.ini     start in episode 3, mission 1
 */
#include <ctype.h>
#include <string.h>
#include "runtime.h"
#include "settings.h"

#define A0 c->r[4]
#define RA c->r[31]

#define SCRATCH 0x802B0000u            /* above the overlay region, never allocated */
#define CURRENT_LEVEL_NAME 0x800B3184u /* func_8001A700's nine-byte trigger-name buffer */
#define LOAD_DESCRIPTOR_RETURN 0x8001A92Cu
#define LEVEL_DESCRIPTORS 0x800A3638u  /* 24 campaign descriptors of 20 bytes, name pointer at +4 */
#define LEVEL_DESCRIPTOR_COUNT 24
#define LEVEL_NAME_CAPACITY 9

static char g_target[16];              /* lower case, e.g. "e3m1"; empty: off */
static char g_source[16];              /* the prefix bound first */
static int g_descriptor_selected;

static void read_string(uint32_t address, char *out, int max)
{
    int i = 0;
    for (; i < max - 1; i++) { char ch = (char)mem_r8(address + (uint32_t)i); if (!ch) break; out[i] = ch; }
    out[i] = 0;
}

static void write_string(uint32_t address, const char *text)
{
    uint32_t i = 0;
    for (; text[i]; i++) mem_w8(address + i, (uint8_t)text[i]);
    mem_w8(address + i, 0);
}

static int digits(const char *s, int i, int at_least, int at_most)
{
    int n = 0;
    while (isdigit((unsigned char)s[i + n])) n++;
    return n >= at_least && (at_most == 0 || n <= at_most) ? n : -1;
}

/* The level prefix of a name (e\d+m\d+, tr\d\d, wr\d\d, xt\d\d or dem\d), its length or 0. */
static int level_prefix(const char *s)
{
    char a = (char)tolower((unsigned char)s[0]), b = s[0] ? (char)tolower((unsigned char)s[1]) : 0;
    int n;
    if (a == 'e' && (n = digits(s, 1, 1, 0)) > 0 && tolower((unsigned char)s[1 + n]) == 'm') {
        int m = digits(s, 2 + n, 1, 0);
        return m > 0 ? 2 + n + m : 0;
    }
    if (((a == 't' && b == 'r') || (a == 'w' && b == 'r') || (a == 'x' && b == 't')) && digits(s, 2, 2, 2) == 2) return 4;
    if (a == 'd' && b == 'e' && tolower((unsigned char)s[2]) == 'm' && digits(s, 3, 1, 1) == 1) return 4;
    return 0;
}

/* A campaign trigger name: eNmM_t. */
static int is_campaign_trigger(const char *s)
{
    int n = level_prefix(s);
    return n && tolower((unsigned char)s[0]) == 'e' && rt_strcasecmp(s + n, "_t") == 0;
}

static int has_descriptor(const char *trigger)
{
    char name[16];
    for (int i = 0; i < LEVEL_DESCRIPTOR_COUNT; i++) {
        uint32_t address = mem_r32(LEVEL_DESCRIPTORS + (uint32_t)i * 20u + 4u);
        if (!address) continue;
        read_string(address, name, LEVEL_NAME_CAPACITY + 1);
        if (rt_strcasecmp(name, trigger) == 0) return 1;
    }
    return 0;
}

int hook_LevelSwitch_SelectDescriptor(CpuContext *c)
{
    if (g_descriptor_selected || !g_target[0] || A0 != CURRENT_LEVEL_NAME || RA != LOAD_DESCRIPTOR_RETURN) return 1;
    char target[16], current[16];
    if (strlen(g_target) + 2 >= LEVEL_NAME_CAPACITY) return 1;
    strcpy(target, g_target);
    strcat(target, "_t");
    if (!is_campaign_trigger(target) || !has_descriptor(target)) return 1;
    read_string(CURRENT_LEVEL_NAME, current, LEVEL_NAME_CAPACITY + 1);
    if (!is_campaign_trigger(current)) return 1;
    if (rt_strcasecmp(current, target) != 0) {
        write_string(CURRENT_LEVEL_NAME, target);
        rt_log("level descriptor %s -> %s", current, target);
    }
    /* Later stages follow the selected descriptor: no fallback redirect from here on. */
    g_descriptor_selected = 1;
    return 1;
}

/* From the CdWadFind pre-hook: point A0 at the redirected name when it changes. */
void level_sm2_redirect(CpuContext *c)
{
    if (!g_target[0] || g_descriptor_selected) return;
    char name[64];
    read_string(A0, name, sizeof name);
    int n = level_prefix(name);
    if (!n || n >= 16) return;
    char prefix[16];
    memcpy(prefix, name, (size_t)n); prefix[n] = 0;
    const char *rest = name + n;
    /* Never bind to the attract demo (dem1..dem4 play behind the title screen), unless
     * a demo is the target. */
    if (!g_source[0] && tolower((unsigned char)prefix[0]) == 'd' && g_target[0] != 'd') return;   /* dem\d */
    if (!g_source[0]) for (int i = 0; i <= n; i++) g_source[i] = (char)tolower((unsigned char)prefix[i]);
    if (rt_strcasecmp(prefix, g_source) != 0) return;
    char redirected[64];
    if (strlen(g_target) + strlen(rest) >= sizeof redirected) return;
    strcpy(redirected, g_target);
    /* Follow the case the game used. */
    if (isupper((unsigned char)prefix[0])) for (char *p = redirected; *p; p++) *p = (char)toupper((unsigned char)*p);
    strcat(redirected, rest);
    if (strcmp(redirected, name) == 0) return;
    write_string(SCRATCH, redirected);
    A0 = SCRATCH;
    rt_log("level %s -> %s", name, redirected);
}

void level_sm2_install(void)
{
    const char *want = setting("level");
    if (!want || strlen(want) >= sizeof g_target || !level_prefix(want)) return;
    for (int i = 0; want[i]; i++) g_target[i] = (char)tolower((unsigned char)want[i]);
}
