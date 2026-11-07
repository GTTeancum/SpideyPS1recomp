/*
 * Everything the inline RAM path in recomp.h does not cover: the scratchpad,
 * hardware registers and the BIOS ROM, following the PC runtime's PSMemory/Dma.
 * The GPU ports, the sound chip's registers (spu.c) and DMA channels 2 (GPU), 4 (sound
 * RAM) and 6 (ordering-table clear) are emulated; other registers are plain storage
 * that reads back what was written, as on the PC.
 */
#include "gpu.h"
#include "runtime.h"
#include "spu.h"

uint8_t recomp_ram[RECOMP_RAM_SIZE] __attribute__((aligned(16)));
static uint8_t g_scratchpad[1024];
static uint8_t g_io[0x2000];   /* 0x1F801000..0x1F802FFF */
static uint32_t g_dicr;

#define PHYS(a) ((a) & 0x1FFFFFFFu)

static int in_scratchpad(uint32_t p) { return p >= 0x1F800000u && p < 0x1F800400u; }
static int in_io(uint32_t p) { return p >= 0x1F801000u && p < 0x1F803000u; }
static uint32_t io32(uint32_t p) { uint32_t v; __builtin_memcpy(&v, g_io + (p & 0x1FFCu), 4); return v; }
static void io32_set(uint32_t p, uint32_t v) { __builtin_memcpy(g_io + (p & 0x1FFCu), &v, 4); }

static void unmapped(const char *access, uint32_t p)
{
    static uint32_t seen[64];
    static char names[64][24];
    static int count;
    for (int i = 0; i < count; i++) if (seen[i] == p) { rt_missing("memory", names[i]); return; }
    if (count == 64) return;
    seen[count] = p;
    const char *hex = "0123456789ABCDEF";
    char *n = names[count];
    int k = 0;
    while (*access && k < 12) n[k++] = *access++;
    n[k++] = ' ';
    for (int s = 28; s >= 0; s -= 4) n[k++] = hex[(p >> s) & 0xF];
    n[k] = 0;
    rt_missing("memory", n);
    count++;
}

/* ---- DMA ------------------------------------------------------------------------ */
static uint32_t word_count(uint32_t bcr)
{
    uint32_t size = bcr & 0xFFFFu, blocks = (bcr >> 16) & 0xFFFFu;
    uint32_t total = blocks == 0 ? size : size * blocks;
    return total == 0 ? 0x10000u : total;
}

static void dma_gpu(uint32_t madr, uint32_t bcr, uint32_t chcr)
{
    if (((chcr >> 9) & 3u) == 2) {
        uint32_t addr = madr & (RECOMP_RAM_SIZE - 4u);
        for (int guard = 0; guard < 0x100000; guard++) {
            uint32_t header = mem_r32(addr), count = header >> 24;
            for (uint32_t i = 0; i < count; i++) gpu_write_gp0_at(mem_r32(addr + 4u + i * 4u), addr + 4u + i * 4u);
            uint32_t next = header & 0xFFFFFFu;
            if (next == 0xFFFFFFu || (next & 0x800000u)) break;
            addr = next & (RECOMP_RAM_SIZE - 4u);
        }
    } else if (chcr & 1u) {
        uint32_t words = word_count(bcr);
        for (uint32_t i = 0; i < words; i++) gpu_write_gp0_at(mem_r32(madr + i * 4u), madr + i * 4u);
    } else {
        uint32_t words = word_count(bcr);
        for (uint32_t i = 0; i < words; i++) mem_w32(madr + i * 4u, gpu_read_data());
    }
}

static void dma_clear_ot(uint32_t madr, uint32_t bcr)
{
    uint32_t count = bcr & 0xFFFFu, addr = madr;
    if (!count) return;
    for (uint32_t i = 0; i < count - 1; i++) { mem_w32(addr, (addr - 4u) & 0x00FFFFFFu); addr -= 4u; }
    mem_w32(addr, 0x00FFFFFFu);
}

static void dma_run(int channel, uint32_t madr, uint32_t bcr, uint32_t chcr)
{
    switch (channel) {
    case 2: dma_gpu(madr, bcr, chcr); break;
    case 4: if (chcr & 1u) spu_dma_write(madr, word_count(bcr) * 4u); break;   /* to the sound chip; reads ignored, as on the PC */
    case 6: dma_clear_ot(madr, bcr); break;
    default: unmapped("dma", 0x1F801088u + 0x10u * (uint32_t)channel); break;
    }
    int master = (g_dicr >> 23) & 1, enabled = (g_dicr >> (16 + channel)) & 1;
    if (master && enabled) { g_dicr |= 1u << (24 + channel); rt_raise_irq(3); }
}

static void write_dicr(uint32_t v)
{
    uint32_t flags = (g_dicr >> 24) & 0x7Fu;
    flags &= ~((v >> 24) & 0x7Fu);
    g_dicr = (v & 0x00FFFFFFu) | (flags << 24);
    if ((g_dicr & 0x8000u) || (((g_dicr >> 23) & 1u) && flags)) g_dicr |= 0x80000000u;
}

/* ---- accesses --------------------------------------------------------------------- */
uint32_t mem_slow_r32(uint32_t a)
{
    uint32_t p = PHYS(a);
    if (in_scratchpad(p)) { uint32_t v; __builtin_memcpy(&v, g_scratchpad + (p & 0x3FCu), 4); return v; }
    if (p == 0x1F801810u) return gpu_read_data();
    if (p == 0x1F801814u) return gpu_read_status();
    if (p == 0x1F8010F4u) return g_dicr;
    if (spu_is_reg(p)) return spu_read16(p) | ((uint32_t)spu_read16(p + 2u) << 16);
    if (in_io(p)) return io32(p);
    if (p >= 0x1FC00000u && p < 0x1FC80000u) return 0;
    unmapped("read", p);
    return 0;
}

uint32_t mem_slow_r16(uint32_t a)
{
    uint32_t p = PHYS(a);
    if (in_scratchpad(p)) { uint16_t v; __builtin_memcpy(&v, g_scratchpad + (p & 0x3FEu), 2); return v; }
    if (spu_is_reg(p)) return spu_read16(p);
    if (in_io(p)) { uint16_t v; __builtin_memcpy(&v, g_io + (p & 0x1FFEu), 2); return v; }
    if (p >= 0x1FC00000u && p < 0x1FC80000u) return 0;
    unmapped("read", p);
    return 0;
}

uint32_t mem_slow_r8(uint32_t a)
{
    uint32_t p = PHYS(a);
    if (in_scratchpad(p)) return g_scratchpad[p & 0x3FFu];
    if (in_io(p)) return g_io[p & 0x1FFFu];
    if (p >= 0x1FC00000u && p < 0x1FC80000u) return 0;
    unmapped("read", p);
    return 0;
}

void mem_slow_w32(uint32_t a, uint32_t v)
{
    uint32_t p = PHYS(a);
    if (in_scratchpad(p)) { __builtin_memcpy(g_scratchpad + (p & 0x3FCu), &v, 4); return; }
    if (p == 0x1F801810u) { gpu_write_gp0(v); return; }
    if (p == 0x1F801814u) { gpu_write_gp1(v); return; }
    if (p == 0x1F8010F4u) { write_dicr(v); return; }
    if (p >= 0x1F801080u && p < 0x1F8010F0u && (p & 0xFu) == 8u && (v & 0x01000000u)) {
        io32_set(p, v & ~0x01000000u);
        dma_run((int)((p - 0x1F801080u) / 0x10u), io32(p - 8u), io32(p - 4u), v);
        return;
    }
    if (spu_is_reg(p)) { spu_write16(p, (uint16_t)v); spu_write16(p + 2u, (uint16_t)(v >> 16)); return; }
    if (in_io(p)) { io32_set(p, v); return; }
    unmapped("write", p);
}

void mem_slow_w16(uint32_t a, uint16_t v)
{
    uint32_t p = PHYS(a);
    if (in_scratchpad(p)) { __builtin_memcpy(g_scratchpad + (p & 0x3FEu), &v, 2); return; }
    if (spu_is_reg(p)) { spu_write16(p, v); return; }
    if (in_io(p)) { __builtin_memcpy(g_io + (p & 0x1FFEu), &v, 2); return; }
    unmapped("write", p);
}

void mem_slow_w8(uint32_t a, uint8_t v)
{
    uint32_t p = PHYS(a);
    if (in_scratchpad(p)) { g_scratchpad[p & 0x3FFu] = v; return; }
    if (in_io(p)) { g_io[p & 0x1FFFu] = v; return; }
    unmapped("write", p);
}
