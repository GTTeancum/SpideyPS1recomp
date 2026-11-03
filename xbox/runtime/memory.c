/*
 * Everything the inline RAM path in recomp.h does not cover: the scratchpad,
 * hardware registers and the BIOS ROM. Hardware is not emulated yet; reads return
 * zero and each register first touched is reported.
 */
#include <stdio.h>
#include "runtime.h"

uint8_t recomp_ram[RECOMP_RAM_SIZE] __attribute__((aligned(16)));
static uint8_t g_scratchpad[1024];

#define PHYS(a) ((a) & 0x1FFFFFFFu)

static int in_scratchpad(uint32_t a) { return PHYS(a) >= 0x1F800000u && PHYS(a) < 0x1F800400u; }

static void hardware(const char *access, uint32_t a)
{
    /* One report per register; the names live in a small static table. */
    static uint32_t seen[128];
    static char names[128][24];
    static int count;
    uint32_t p = PHYS(a);
    for (int i = 0; i < count; i++) if (seen[i] == p) { rt_missing(access, names[i]); return; }
    if (count == 128) return;
    seen[count] = p;
    snprintf(names[count], sizeof names[count], "%s %08lX", access, (unsigned long)p);
    rt_missing("hardware", names[count]);
    count++;
}

uint32_t mem_slow_r8(uint32_t a)
{
    if (in_scratchpad(a)) return g_scratchpad[PHYS(a) & 0x3FFu];
    hardware("read8", a);
    return 0;
}

uint32_t mem_slow_r16(uint32_t a)
{
    if (in_scratchpad(a)) { uint16_t v; __builtin_memcpy(&v, g_scratchpad + (PHYS(a) & 0x3FEu), 2); return v; }
    hardware("read16", a);
    return 0;
}

/* GPUSTAT with the bits the PC runtime's Gpu.ReadStat always sets (ready to receive
 * commands, DMA and VRAM-to-CPU) and the interlace field toggling per read. */
static uint32_t gpu_status(void)
{
    static uint32_t field;
    field ^= 1u;
    return (1u << 10) | (1u << 13) | (1u << 26) | (1u << 27) | (1u << 28) | (field << 31);
}

uint32_t mem_slow_r32(uint32_t a)
{
    if (in_scratchpad(a)) { uint32_t v; __builtin_memcpy(&v, g_scratchpad + (PHYS(a) & 0x3FCu), 4); return v; }
    if (PHYS(a) == 0x1F801814u) return gpu_status();
    hardware("read32", a);
    return 0;
}

void mem_slow_w8(uint32_t a, uint8_t v)
{
    if (in_scratchpad(a)) { g_scratchpad[PHYS(a) & 0x3FFu] = v; return; }
    hardware("write8", a);
}

void mem_slow_w16(uint32_t a, uint16_t v)
{
    if (in_scratchpad(a)) { __builtin_memcpy(g_scratchpad + (PHYS(a) & 0x3FEu), &v, 2); return; }
    hardware("write16", a);
}

void mem_slow_w32(uint32_t a, uint32_t v)
{
    if (in_scratchpad(a)) { __builtin_memcpy(g_scratchpad + (PHYS(a) & 0x3FCu), &v, 4); return; }
    hardware("write32", a);
}
