/*
 * Start a new game in a chosen level: a port of the PC's LevelSwitch.cs
 * (spiderman/patches). A fix to either belongs in both places.
 *
 * The retail level descriptor for the target is selected before the game looks it up
 * (func_80018800), which keeps its overlay callbacks and HUD elements; archive lookups
 * for the level the game asks for first (normally l1a1) are rewritten to the target,
 * as a fallback for alternate acts that have no descriptor. Once the target's trigger
 * file loads, the descriptor is left alone, and only the first prefix is ever
 * rewritten, so the next act loads normally.
 *
 *   level=l5a3 in D:\openspidey.ini     start in level 5, act 3
 */
#include <ctype.h>
#include <string.h>
#include "runtime.h"
#include "settings.h"

#define A0 c->r[4]

#define SCRATCH 0x802B0000u            /* above the overlay region, never allocated */
#define CURRENT_LEVEL_NAME 0x800A568Cu
#define LEVEL_DESCRIPTORS 0x800974A4u
#define LEVEL_DESCRIPTOR_COUNT 34

static char g_target[16];              /* lower case, e.g. "l5a3"; empty: off */
static char g_source[16];              /* the prefix bound first */
static char g_descriptor_from[24];
static int g_entered;

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

/* lNaM with an optional act letter: the length of the level prefix, or 0. */
static int level_prefix(const char *s)
{
    int i = 0;
    if (tolower((unsigned char)s[i++]) != 'l' || !isdigit((unsigned char)s[i])) return 0;
    while (isdigit((unsigned char)s[i])) i++;
    if (tolower((unsigned char)s[i++]) != 'a' || !isdigit((unsigned char)s[i])) return 0;
    while (isdigit((unsigned char)s[i])) i++;
    if (isalpha((unsigned char)s[i]) && s[i] != '_' && s[i] != '.') i++;
    return i;
}

/* lNaM only (no act letter): the length, or 0. */
static int numeric_prefix(const char *s)
{
    int n = level_prefix(s);
    return n && isalpha((unsigned char)s[n - 1]) ? n - 1 : n;
}

static int is_trigger_name(const char *s)
{
    int n = level_prefix(s);
    return n && rt_strcasecmp(s + n, "_t") == 0 && !isalpha((unsigned char)s[n - 1]);
}

static int has_descriptor(const char *trigger)
{
    char name[24];
    for (int i = 0; i < LEVEL_DESCRIPTOR_COUNT; i++) {
        uint32_t address = mem_r32(LEVEL_DESCRIPTORS + (uint32_t)i * 20u + 4u);
        if (!address) continue;
        read_string(address, name, 16);
        if (rt_strcasecmp(name, trigger) == 0) return 1;
    }
    return 0;
}

int hook_LevelSwitch_SelectDescriptor(CpuContext *c)
{
    if (g_entered || !g_target[0] || A0 != CURRENT_LEVEL_NAME) return 1;
    char target[24], current[24];
    strcpy(target, g_target);
    strcat(target, "_t");
    if (!has_descriptor(target)) return 1;
    read_string(CURRENT_LEVEL_NAME, current, 16);
    if (!is_trigger_name(current)) return 1;
    if (g_descriptor_from[0] && rt_strcasecmp(current, g_descriptor_from) != 0) return 1;
    if (rt_strcasecmp(current, target) != 0) {
        if (!g_descriptor_from[0]) strcpy(g_descriptor_from, current);
        write_string(CURRENT_LEVEL_NAME, target);
        rt_note("level descriptor selected");
    }
    return 1;
}

void level_sm1_redirect(CpuContext *c)
{
    if (!g_target[0]) return;
    char name[64];
    read_string(A0, name, sizeof name);
    int n = level_prefix(name);
    if (!n || n >= 16) return;
    const char *rest = name + n;
    char prefix[16];
    memcpy(prefix, name, (size_t)n); prefix[n] = 0;
    if (!g_entered && rt_strcasecmp(prefix, g_target) == 0 && (rest[0] == '_' && tolower((unsigned char)rest[1]) == 't'))
        g_entered = 1;
    /* Bind to the level the game asks for first and redirect only that one. */
    if (!g_source[0]) for (int i = 0; i <= n; i++) g_source[i] = (char)tolower((unsigned char)prefix[i]);
    if (rt_strcasecmp(prefix, g_source) != 0) return;
    /* Alternate acts (L1A2A) reuse the numeric parent's VAB/SFX pair. */
    char target[16];
    strcpy(target, g_target);
    if (rt_strcasecmp(rest, ".VAB") == 0 || rt_strcasecmp(rest, ".SFX") == 0) target[numeric_prefix(target)] = 0;
    /* Follow the case the game used. */
    if (isupper((unsigned char)prefix[0])) for (char *p = target; *p; p++) *p = (char)toupper((unsigned char)*p);
    char redirected[64];
    if (strlen(target) + strlen(rest) >= sizeof redirected) return;
    strcpy(redirected, target);
    strcat(redirected, rest);
    if (strcmp(redirected, name) == 0) return;
    write_string(SCRATCH, redirected);
    A0 = SCRATCH;
    rt_note("level file redirected");
}

void level_sm1_install(void)
{
    const char *want = setting("level");
    if (!want || strlen(want) >= sizeof g_target || !level_prefix(want)) return;
    for (int i = 0; want[i]; i++) g_target[i] = (char)tolower((unsigned char)want[i]);
}
