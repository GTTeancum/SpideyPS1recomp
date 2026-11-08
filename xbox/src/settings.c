/*
 * Player settings from D:\openspidey.ini: `key=value` lines, `#` comments. Read
 * once at start-up. Keys:
 *   level=l2a1        start a new game in that level and act (LevelSwitch)
 *   suit=superior     wear that mod suit (D:\mods\suits\<id>, made by prep_suits.py)
 *   dreamcast_movies=0  play the PS1 movies even where a Dreamcast one is installed
 *   precise_vertices=0  draw as before vertex tags (whole pixels): for comparisons
 *   crack_fill=1        the gap fill (the PC's crack rule) on finished frames (off by default)
 *   geometry_debug=1    world materials in flat colours (cracks show); 2: frames start magenta (holes show);
 *                       4: also the gap fill's world mark green; 5: pixels drawn blue; 6: magenta, fill unconditional
 */
#include <stdio.h>
#include <string.h>
#include "settings.h"

#define SETTINGS 16
static struct { char key[32], value[96]; } g_settings[SETTINGS];
static int g_count;

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = 0;
    return s;
}

void settings_load(void)
{
    FILE *f = fopen("D:\\openspidey.ini", "r");
    if (!f) return;
    char line[160];
    while (g_count < SETTINGS && fgets(line, sizeof line, f)) {
        char *s = trim(line), *eq = strchr(s, '=');
        if (!*s || *s == '#' || !eq) continue;
        *eq = 0;
        char *key = trim(s), *value = trim(eq + 1);
        if (!*key || strlen(key) >= sizeof g_settings[0].key || strlen(value) >= sizeof g_settings[0].value) continue;
        strcpy(g_settings[g_count].key, key);
        strcpy(g_settings[g_count].value, value);
        g_count++;
    }
    fclose(f);
}

const char *setting(const char *key)
{
    for (int i = 0; i < g_count; i++)
        if (strcmp(g_settings[i].key, key) == 0) return g_settings[i].value[0] ? g_settings[i].value : 0;
    return 0;
}
