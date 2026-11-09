/*
 * CD.WAD entries served from files instead: a port of the PC's LooseWadOverrides
 * (tools/RecompOne/RecompOne.Runtime/Assets/LooseWadOverrides.cs), shared by both
 * games. A fix to either belongs in both places.
 *
 * A lookup the game makes (CdWadFind) for a name the disc's prepared actors hold
 * (D:\assets\builtin, listed in index.txt by xbox/tools/prep_assets.py) -- or for a
 * name the game's patches alias to one of them, or to a suit's own model -- answers
 * with the file's size; the game's next allocation of exactly that size comes from an
 * arena in the expanded RAM above the PlayStation's 2 MB, and its CdWadRead reads the
 * file there. The retail heap is never handed a block it did not make.
 *
 * Arena: 0x80300000-0x80780000, the same bounds as the PC's (ArenaLo/ArenaHi), inside
 * the 8 MB RAM window, above the code overlays and below the texture registry.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "files.h"
#include "runtime.h"
#include "guest_layout.h"
#include "wad_overrides.h"

#define V0 c->r[2]
#define A0 c->r[4]

#define ARENA_LO GUEST_ARENA_LO
#define ARENA_HI GUEST_ARENA_HI
#define BLOCKS 128
#define MAX_FILE (2u * 1024u * 1024u)

/* ---- the arena ------------------------------------------------------------------------ */
typedef struct { uint32_t address, size; } Block;
static Block g_free[BLOCKS], g_used[BLOCKS];
static int g_free_count, g_used_count;
extern volatile uint32_t rt_arena_peak, rt_arena_used;   /* runtime/log.c (status) */

static void arena_reset(void)
{
    g_free[0].address = ARENA_LO; g_free[0].size = ARENA_HI - ARENA_LO;
    g_free_count = 1; g_used_count = 0;
}

uint32_t wad_scratch(uint32_t requested)
{
    uint32_t size = (requested + 15u) & ~15u;
    if (!requested || size < requested) return 0;
    for (int i = 0; i < g_free_count; i++) {
        if (g_free[i].size < size || g_used_count >= BLOCKS) continue;
        uint32_t address = g_free[i].address;
        g_free[i].address += size; g_free[i].size -= size;
        if (!g_free[i].size) g_free[i] = g_free[--g_free_count];
        g_used[g_used_count].address = address; g_used[g_used_count].size = size; g_used_count++;
        if (address + size - ARENA_LO > rt_arena_peak) rt_arena_peak = address + size - ARENA_LO;
        rt_arena_used += size;
        return address;
    }
    rt_note("asset arena exhausted");
    return 0;
}

int wad_allocation(uint32_t address, uint32_t *size)
{
    for (int i = 0; i < g_used_count; i++)
        if (g_used[i].address == address) { if (size) *size = g_used[i].size; return 1; }
    return 0;
}

int wad_owns(uint32_t address) { return wad_allocation(address, 0); }

int wad_free(uint32_t address)
{
    int u = -1;
    for (int i = 0; i < g_used_count; i++) if (g_used[i].address == address) { u = i; break; }
    if (u < 0) return 0;
    uint32_t start = address, size = g_used[u].size;
    g_used[u] = g_used[--g_used_count];
    rt_arena_used -= size;
    for (int i = 0; i < g_free_count; i++)
        if (g_free[i].address + g_free[i].size == start) { start = g_free[i].address; size += g_free[i].size; g_free[i--] = g_free[--g_free_count]; }
        else if (g_free[i].address == start + size) { size += g_free[i].size; g_free[i--] = g_free[--g_free_count]; }
    if (g_free_count < BLOCKS) { g_free[g_free_count].address = start; g_free[g_free_count].size = size; g_free_count++; }
    else rt_note("asset arena: free list full, a block not reused");
    return 1;
}

/* ---- the prepared actors ---------------------------------------------------------------- */
#define ACTORS 128
static char g_actor_names[ACTORS][24];
static int g_actor_count;

static void load_index(void)
{
    g_actor_count = 0;
    FILE *f = fopen("D:\\assets\\builtin\\index.txt", "r");
    if (!f) return;
    char line[64];
    while (g_actor_count < ACTORS && fgets(line, sizeof line, f)) {
        int n = (int)strcspn(line, "\r\n");
        if (n <= 0 || n >= (int)sizeof g_actor_names[0]) continue;
        line[n] = 0;
        strcpy(g_actor_names[g_actor_count++], line);
    }
    fclose(f);
}

int wad_have(const char *name)
{
    for (int i = 0; i < g_actor_count; i++) if (rt_strcasecmp(g_actor_names[i], name) == 0) return 1;
    return 0;
}

void wad_init(void)
{
    arena_reset();
    load_index();
}

/* ---- the lookup ------------------------------------------------------------------------ */
static char g_pending_path[160];
static uint32_t g_pending_bytes, g_pending_rounded;
static int g_pending, g_pending_aliased;

static int plain_name(const char *name) { return !strchr(name, '\\') && !strchr(name, '/') && !strstr(name, ".."); }

static int pend(const char *path, int aliased)
{
    long bytes = rt_file_size(path);   /* at each served lookup: not stdio (files.h) */
    if (bytes <= 0 || (uint32_t)bytes > MAX_FILE) return 0;
    snprintf(g_pending_path, sizeof g_pending_path, "%s", path);
    g_pending_bytes = (uint32_t)bytes;
    g_pending_rounded = (g_pending_bytes + 0x7FFu) & ~0x7FFu;
    g_pending = 1;
    g_pending_aliased = aliased;
    return 1;
}

int wad_find(const char *name, const char *alias)
{
    g_pending = g_pending_aliased = 0;
    const char *source = alias ? alias : name;
    if (!plain_name(name) || !plain_name(source) || strlen(source) >= sizeof g_actor_names[0] || !wad_have(source)) return 0;
    char path[160];
    snprintf(path, sizeof path, "D:\\assets\\builtin\\%s", source);
    return pend(path, rt_strcasecmp(name, source) != 0);
}

int wad_find_file(const char *path)
{
    g_pending = g_pending_aliased = 0;
    return pend(path, 1);
}

int wad_pending(void) { return g_pending; }

void wad_find_exit(CpuContext *c) { if (g_pending) V0 = g_pending_rounded; }

int wad_complete_aliased(CpuContext *c)
{
    if (!g_pending || !g_pending_aliased) return 0;
    wad_find_exit(c);
    return 1;
}

int wad_heap_alloc(CpuContext *c)
{
    if (!g_pending || A0 != g_pending_rounded) return 0;
    uint32_t address = wad_scratch(A0);
    if (!address) return 0;
    V0 = address;
    return 1;
}

int wad_read(CpuContext *c)
{
    if (!g_pending) return 0;
    g_pending = g_pending_aliased = 0;
    uint32_t to = A0;
    /* Bounds: the whole rounded block lies in RAM, else nothing is written. */
    if (!mem_is_ram(to) || !mem_is_ram(to + g_pending_rounded - 1u) ||
        (to & (RECOMP_RAM_SIZE - 1u)) + g_pending_rounded > RECOMP_RAM_SIZE) {
        rt_note("asset read refused: outside RAM");
        return 0;
    }
    /* This can run mid-level (a trigger loading an actor): one kernel read straight into
     * PlayStation RAM, not the C library's 1 KB reads. */
    uint8_t *at = mem_ram(to);
    memset(at, 0, g_pending_rounded);
    HANDLE f = CreateFileA(g_pending_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD got = 0;
        ReadFile(f, at, g_pending_bytes, &got, 0);
        CloseHandle(f);
    }
    V0 = 0xFFFFFFFFu;
    rt_note("actor served from the disc's assets");
    return 1;
}
