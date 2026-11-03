/*
 * PlayStation BIOS calls (A0/B0/C0 tables), following the PC runtime's high-level
 * BIOS (RecompOne.Runtime.Bios). Memory, string, heap and event bookkeeping are
 * implemented; the rest answer the way the PC runtime does when unused, or are
 * reported as missing by table and number.
 */
#include <stdio.h>
#include "runtime.h"

#define A0 c->r[4]
#define A1 c->r[5]
#define A2 c->r[6]
#define V0 c->r[2]

/* ---- guest strings and memory ------------------------------------------------ */
static uint32_t g_len(uint32_t s) { uint32_t n = 0; while (mem_r8(s + n)) n++; return n; }

static uint32_t g_memcpy(uint32_t d, uint32_t s, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) mem_w8(d + i, (uint8_t)mem_r8(s + i));
    return d;
}

static uint32_t g_memmove(uint32_t d, uint32_t s, uint32_t n)
{
    if (d <= s) return g_memcpy(d, s, n);
    for (uint32_t i = n; i-- > 0;) mem_w8(d + i, (uint8_t)mem_r8(s + i));
    return d;
}

static uint32_t g_memset(uint32_t d, uint8_t v, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) mem_w8(d + i, v);
    return d;
}

static uint32_t g_memcmp(uint32_t a, uint32_t b, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        int d = (int)mem_r8(a + i) - (int)mem_r8(b + i);
        if (d) return (uint32_t)d;
    }
    return 0;
}

static uint32_t g_strcmp(uint32_t a, uint32_t b, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        int x = (int)mem_r8(a + i), y = (int)mem_r8(b + i);
        if (x != y) return (uint32_t)(x - y);
        if (!x) return 0;
    }
    return 0;
}

static uint32_t g_strncpy(uint32_t d, uint32_t s, uint32_t n)
{
    uint32_t i = 0;
    for (; i < n; i++) { uint8_t ch = (uint8_t)mem_r8(s + i); mem_w8(d + i, ch); if (!ch) break; }
    for (; i < n; i++) mem_w8(d + i, 0);
    return d;
}

static uint32_t g_strchr(uint32_t s, uint8_t ch, int last)
{
    uint32_t found = 0;
    for (;; s++) {
        uint8_t v = (uint8_t)mem_r8(s);
        if (v == ch) { found = s; if (!last) return s; }
        if (!v) return found;
    }
}

/* ---- heap (InitHeap/malloc/free): first fit over a free list, as on the PC ---- */
#define BLOCKS 512
static struct { uint32_t start, end, busy; } g_blocks[BLOCKS];
static int g_block_count;

static void heap_init(uint32_t base, uint32_t size)
{
    g_block_count = 1;
    g_blocks[0].start = base; g_blocks[0].end = base + size; g_blocks[0].busy = 0;
}

static uint32_t heap_alloc(uint32_t size)
{
    if (!size) size = 4;
    size = (size + 3u) & ~3u;
    for (int i = 0; i < g_block_count; i++) {
        if (g_blocks[i].busy || g_blocks[i].end - g_blocks[i].start < size) continue;
        if (g_blocks[i].end - g_blocks[i].start > size && g_block_count < BLOCKS) {
            for (int j = g_block_count; j > i + 1; j--) g_blocks[j] = g_blocks[j - 1];
            g_block_count++;
            g_blocks[i + 1].start = g_blocks[i].start + size;
            g_blocks[i + 1].end = g_blocks[i].end;
            g_blocks[i + 1].busy = 0;
            g_blocks[i].end = g_blocks[i].start + size;
        }
        g_blocks[i].busy = 1;
        return g_blocks[i].start;
    }
    return 0;
}

static void heap_free(uint32_t address)
{
    for (int i = 0; i < g_block_count; i++) {
        if (g_blocks[i].start != address || !g_blocks[i].busy) continue;
        g_blocks[i].busy = 0;
        if (i + 1 < g_block_count && !g_blocks[i + 1].busy) {
            g_blocks[i].end = g_blocks[i + 1].end;
            for (int j = i + 1; j < g_block_count - 1; j++) g_blocks[j] = g_blocks[j + 1];
            g_block_count--;
        }
        if (i > 0 && !g_blocks[i - 1].busy) {
            g_blocks[i - 1].end = g_blocks[i].end;
            for (int j = i; j < g_block_count - 1; j++) g_blocks[j] = g_blocks[j + 1];
            g_block_count--;
        }
        return;
    }
}

/* ---- events ---------------------------------------------------------------------- */
#define EVENTS 32
static struct { uint32_t status, cls, spec, mode, func; } g_events[EVENTS];

static int ev_slot(uint32_t ev)
{
    if ((ev & 0xFFFF0000u) != 0xF0000000u) return -1;
    int s = (int)(ev & 0xFFFFu);
    return s < EVENTS ? s : -1;
}

/* ---- dispatch ---------------------------------------------------------------------- */
static void missing(char table, uint32_t fn)
{
    static char names[3][256][8];
    int t = table == 'A' ? 0 : table == 'B' ? 1 : 2;
    char *name = names[t][fn & 0xFF];
    if (!name[0]) snprintf(name, 8, "%c(%02lX)", table, (unsigned long)(fn & 0xFF));
    rt_missing("BIOS", name);
}

static void bios_a(CpuContext *c, uint32_t fn)
{
    switch (fn) {
    case 0x0E: case 0x0F: V0 = (uint32_t)((int32_t)A0 < 0 ? -(int32_t)A0 : (int32_t)A0); break;
    case 0x13:                                                                          /* setjmp */
        mem_w32(A0 + 0x00u, c->r[31]); mem_w32(A0 + 0x04u, c->r[29]);
        mem_w32(A0 + 0x08u, c->r[30]); mem_w32(A0 + 0x0Cu, c->r[28]);
        for (int i = 0; i < 8; i++) mem_w32(A0 + 0x10u + 4u * (uint32_t)i, c->r[16 + i]);
        V0 = 0;
        break;
    case 0x14:                                                                          /* longjmp: registers only, as on the PC */
        c->r[31] = mem_r32(A0 + 0x00u); c->r[29] = mem_r32(A0 + 0x04u);
        c->r[30] = mem_r32(A0 + 0x08u); c->r[28] = mem_r32(A0 + 0x0Cu);
        for (int i = 0; i < 8; i++) c->r[16 + i] = mem_r32(A0 + 0x10u + 4u * (uint32_t)i);
        V0 = A1 ? A1 : 1u;
        break;
    case 0x15: V0 = A0; g_memcpy(A0 + g_len(A0), A1, g_len(A1) + 1); break;           /* strcat */
    case 0x17: V0 = g_strcmp(A0, A1, 0xFFFFFFFFu); break;
    case 0x18: V0 = g_strcmp(A0, A1, A2); break;
    case 0x19: V0 = g_memcpy(A0, A1, g_len(A1) + 1); break;                           /* strcpy */
    case 0x1A: V0 = g_strncpy(A0, A1, A2); break;
    case 0x1B: V0 = g_len(A0); break;
    case 0x1C: case 0x1E: V0 = g_strchr(A0, (uint8_t)A1, 0); break;
    case 0x1D: case 0x1F: V0 = g_strchr(A0, (uint8_t)A1, 1); break;
    case 0x25: V0 = (A0 & 0xFF) >= 'a' && (A0 & 0xFF) <= 'z' ? (A0 & 0xFF) - 32 : (A0 & 0xFF); break;
    case 0x26: V0 = (A0 & 0xFF) >= 'A' && (A0 & 0xFF) <= 'Z' ? (A0 & 0xFF) + 32 : (A0 & 0xFF); break;
    case 0x27: g_memcpy(A1, A0, A2); V0 = A1; break;                                   /* bcopy */
    case 0x28: g_memset(A0, 0, A1); break;                                             /* bzero */
    case 0x29: case 0x2D: V0 = g_memcmp(A0, A1, A2); break;
    case 0x2A: V0 = g_memcpy(A0, A1, A2); break;
    case 0x2B: V0 = g_memset(A0, (uint8_t)A1, A2); break;
    case 0x2C: V0 = g_memmove(A0, A1, A2); break;
    case 0x33: V0 = heap_alloc(A0); break;
    case 0x34: heap_free(A0); break;
    case 0x37: { uint32_t n = A0 * A1, p = heap_alloc(n); if (p) g_memset(p, 0, n); V0 = p; } break;
    case 0x39: heap_init(A0, A1); break;
    case 0x3C: case 0x3D: case 0x3E: case 0x3F: V0 = 0; break;                        /* console output */
    case 0x44: case 0x45: case 0x46: case 0x47: case 0x48: case 0x49:
    case 0x4A: case 0x4B: case 0x4C: case 0x4E: break;
    case 0x4D: V0 = 0; break;
    case 0x51: case 0x53: case 0x54: case 0x55: case 0x56: case 0x5C:
    case 0x67: case 0x68: case 0x70: case 0x71: case 0x72: break;
    default: missing('A', fn); V0 = 0; break;
    }
}

static void bios_b(CpuContext *c, uint32_t fn)
{
    int s;
    switch (fn) {
    case 0x00: case 0x02: case 0x03: V0 = 0; break;
    case 0x01: case 0x04: case 0x05: case 0x06: case 0x10: case 0x11: case 0x17:
    case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F: break;
    case 0x08:                                                                          /* OpenEvent */
        V0 = 0xFFFFFFFFu;
        for (s = 0; s < EVENTS; s++)
            if (!g_events[s].status) {
                g_events[s].status = 1; g_events[s].cls = A0; g_events[s].spec = A1;
                g_events[s].mode = A2; g_events[s].func = c->r[7];
                V0 = 0xF0000000u | (uint32_t)s;
                break;
            }
        break;
    case 0x09: s = ev_slot(A0); if (s >= 0) g_events[s].status = 0; V0 = 1; break;     /* CloseEvent */
    case 0x0A: case 0x0B:                                                               /* Wait/TestEvent */
        s = ev_slot(A0);
        V0 = s >= 0 && g_events[s].status == 4 ? (g_events[s].status = 2, 1u) : 0u;
        break;
    case 0x0C: s = ev_slot(A0); if (s >= 0) g_events[s].status = 2; V0 = 1; break;     /* EnableEvent */
    case 0x0D: s = ev_slot(A0); if (s >= 0) g_events[s].status = 1; V0 = 1; break;     /* DisableEvent */
    case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: break;                       /* pad (HLE elsewhere) */
    case 0x2B: case 0x2C: case 0x2D: case 0x2E: break;
    case 0x2F: case 0x30: case 0x31: V0 = 0; break;
    case 0x3B: case 0x3D: V0 = A0; break;                                               /* putchar */
    case 0x3F: V0 = A0; break;                                                          /* puts */
    case 0x40: V0 = 1; break;
    case 0x4A: case 0x4B: case 0x4C: V0 = 1; break;
    case 0x56: case 0x57: V0 = 0; break;
    case 0x5B: V0 = 0; break;
    default: missing('B', fn); V0 = 0; break;
    }
}

static void bios_c(CpuContext *c, uint32_t fn)
{
    switch (fn) {
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x07: case 0x08:
    case 0x0A: case 0x0B: case 0x0C: case 0x12: case 0x1C: break;
    default: missing('C', fn); V0 = 0; break;
    }
}

void bios_init(void) { g_block_count = 0; }

int bios_dispatch(CpuContext *c, uint32_t address)
{
    switch (address) {
    case 0xA0u: bios_a(c, c->r[9]); return 1;
    case 0xB0u: bios_b(c, c->r[9]); return 1;
    case 0xC0u: bios_c(c, c->r[9]); return 1;
    }
    return (address & 0xFF000000u) == 0xBFC00000u;
}

/* The PC port's generated Bios.Syscall/Break stubs do nothing; match them. */
void bios_syscall(CpuContext *c) { (void)c; }

void bios_break(CpuContext *c) { (void)c; }
