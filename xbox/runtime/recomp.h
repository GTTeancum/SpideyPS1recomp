/*
 * OpenSpidey Xbox C runtime: the interface RecompOne's C output is written against
 * (docs/recompiler-c-output.md). Generated code includes this header only.
 *
 * Main RAM is the PC port's 8 MB "devkit" layout (the first 2 MB are the retail
 * console's): the port's loose-WAD arenas and scratch tables live above 2 MB.
 * Every KUSEG/KSEG0/KSEG1 mirror of RAM takes the inline path below; the
 * scratchpad, hardware registers and BIOS go through mem_slow_* in the runtime.
 * The Xbox CPU is little-endian like the PlayStation's, so RAM words are read as is.
 */
#pragma once
#include <stdint.h>
#include <string.h>

/* nxdk builds freestanding (-ffreestanding, -fno-builtin), so a plain memcpy of four
 * bytes is a call. The builtins inline fixed small sizes and call the library (the
 * word-at-a-time runtime/memfast.s) for the rest. */
#define memcpy(d, s, n) __builtin_memcpy(d, s, n)
#define memset(d, c, n) __builtin_memset(d, c, n)

/*
 * Where a projected vertex came from: the PC's GteScreen.VertexTag. The GTE's exact,
 * unrounded screen position and camera depth of an SXY word, carried beside it through
 * registers and RAM until the GPU draws it, so the picture uses the precise geometry
 * rather than whole PlayStation pixels and a depth guessed from the screen position
 * (runtime/vtag.c). `value` is the RAM word as tagged (a later write invalidates the
 * tag by changing it); `native` the SXY word the position belongs to, which a packet
 * vertex must equal for the tag to apply.
 */
typedef struct VTag {
    uint32_t value, native;
    float x, y, z;
} VTag;

typedef struct CpuContext {
    uint32_t r[32];          /* general-purpose registers; r[0] is never written */
    uint32_t hi, lo;
    uint32_t sr, cause, epc, bad_vaddr, prid;   /* COP0 */
    const VTag *t[32];       /* each register's vertex tag, or 0 */
} CpuContext;

typedef void (*RecompFn)(CpuContext *c);

typedef struct RecompFunction {
    uint32_t address;
    RecompFn fn;
} RecompFunction;

typedef struct RecompOverlay {
    const char *name;
    int lba;                 /* disc sector of the overlay image, -1 when not on disc */
    uint32_t base, size;     /* load address and size; 0 for the main executable */
    const RecompFunction *functions;   /* sorted by address */
    int count;
} RecompOverlay;

typedef struct RecompGame {
    const char *title;
    const char *boot_exe;
    uint32_t load_address, text_size, gp, sp, pc;
    RecompFn main;           /* configured main function, or 0 to start at pc */
    const RecompOverlay *const *overlays;
    int overlay_count;
} RecompGame;

/* ---- control flow --------------------------------------------------------- */
void recomp_dispatch(CpuContext *c, uint32_t address);
void bios_syscall(CpuContext *c);
void bios_break(CpuContext *c);

/* ---- memory --------------------------------------------------------------- */
#define RECOMP_RAM_SIZE 0x00800000u
extern uint8_t recomp_ram[RECOMP_RAM_SIZE];

uint32_t mem_slow_r8(uint32_t address);
uint32_t mem_slow_r16(uint32_t address);
uint32_t mem_slow_r32(uint32_t address);
void mem_slow_w8(uint32_t address, uint8_t value);
void mem_slow_w16(uint32_t address, uint16_t value);
void mem_slow_w32(uint32_t address, uint32_t value);

static inline int mem_is_ram(uint32_t address) { return (address & 0x1FFFFFFFu) < RECOMP_RAM_SIZE; }
static inline uint8_t *mem_ram(uint32_t address) { return recomp_ram + (address & (RECOMP_RAM_SIZE - 1u)); }

static inline uint32_t mem_r8(uint32_t a)
{
    return mem_is_ram(a) ? *mem_ram(a) : mem_slow_r8(a);
}
static inline uint32_t mem_r16(uint32_t a)
{
    if (!mem_is_ram(a)) return mem_slow_r16(a);
    uint16_t v; __builtin_memcpy(&v, mem_ram(a), 2); return v;
}
static inline uint32_t mem_r32(uint32_t a)
{
    if (!mem_is_ram(a)) return mem_slow_r32(a);
    uint32_t v; __builtin_memcpy(&v, mem_ram(a), 4); return v;
}
static inline void mem_w8(uint32_t a, uint8_t v)
{
    if (mem_is_ram(a)) *mem_ram(a) = v; else mem_slow_w8(a, v);
}
static inline void mem_w16(uint32_t a, uint16_t v)
{
    if (mem_is_ram(a)) __builtin_memcpy(mem_ram(a), &v, 2); else mem_slow_w16(a, v);
}
static inline void mem_w32(uint32_t a, uint32_t v)
{
    if (mem_is_ram(a)) __builtin_memcpy(mem_ram(a), &v, 4); else mem_slow_w32(a, v);
}

/* Unaligned word halves (LWL/LWR/SWL/SWR), little-endian; same masks as the PC runtime. */
static inline uint32_t mem_lwl(uint32_t old, uint32_t a)
{
    uint32_t shift = (a & 3u) * 8u, word = mem_r32(a & ~3u);
    return (old & (0x00FFFFFFu >> shift)) | (word << (24u - shift));
}
static inline uint32_t mem_lwr(uint32_t old, uint32_t a)
{
    uint32_t shift = (a & 3u) * 8u, word = mem_r32(a & ~3u);
    return (old & (0xFFFFFF00u << (24u - shift))) | (word >> shift);
}
static inline void mem_swl(uint32_t a, uint32_t v)
{
    uint32_t shift = (a & 3u) * 8u, word = mem_r32(a & ~3u);
    mem_w32(a & ~3u, (word & (0xFFFFFF00u << shift)) | (v >> (24u - shift)));
}
static inline void mem_swr(uint32_t a, uint32_t v)
{
    uint32_t shift = (a & 3u) * 8u, word = mem_r32(a & ~3u);
    mem_w32(a & ~3u, (word & (0x00FFFFFFu >> (24u - shift))) | (v << shift));
}

/* ---- vertex tags (runtime/vtag.c) ---------------------------------------------
 * RAM and the scratchpad have a tag per word, in 4 KB pages allocated the first time
 * a tagged word is stored there. The generated code carries register tags as the PC's
 * CpuContext does: moves and arithmetic pass them on (MoveGpr, SetDerived), other
 * register writes clear them, loads pick up the tag of the word read, word stores
 * leave the register's tag on the word. */
#define VTAG_PAGES 2049                      /* 8 MB of RAM in 4 KB pages, then the scratchpad */
extern VTag *vtag_pages[VTAG_PAGES];
void vtag_store(uint32_t address, uint32_t value, const VTag *tag);
/* Out of line, called from every generated load and store: inlined, the tag work
 * doubled the program (19 MB) past the memory the Xbox has spare for it. */
const VTag *vtag_load(uint32_t address, uint32_t value);
const VTag *vtag_containing(uint32_t address);
void vtag_stored(uint32_t address, uint32_t value, const VTag *tag);
const VTag *vtag_pick(const VTag *a, const VTag *b);

/* The tag slot of an aligned word, if its page exists. */
static inline VTag *vtag_slot(uint32_t a)
{
    uint32_t p = a & 0x1FFFFFFFu, page;
    if (p < RECOMP_RAM_SIZE) page = p >> 12;
    else if ((p & ~0x3FFu) == 0x1F800000u) page = VTAG_PAGES - 1;
    else return 0;
    VTag *e = vtag_pages[page];
    return e ? e + ((p & 4095u) >> 2) : 0;
}

/* The tag of the word at a, if the word is still the one tagged. */
static inline const VTag *vtag_of(uint32_t a, uint32_t value)
{
    const VTag *e = vtag_slot(a);
    return e && e->z > 0.0f && e->value == value ? e : 0;
}

/* LW; LB/LBU/LH/LHU take the containing word's tag (vtag_containing: mesh builders
 * unpack SXY as halves and recombine them, the PC's GteScreen.ContainingVertex). */
static inline uint32_t mem_r32_t(CpuContext *c, int r, uint32_t a)
{
    uint32_t v = mem_r32(a);
    c->t[r] = vtag_load(a, v);
    return v;
}

/* SW: the register's tag goes with the word; an untagged word clears the slot. */
static inline void mem_w32_t(uint32_t a, uint32_t v, const VTag *t)
{
    mem_w32(a, v);
    vtag_stored(a, v, t);
}

/* vtag_pick: two sources (the PC's SetDerived with two registers): either one's tag,
 * the same projection if both carry one, else none. */

/* A loop that spins on an interrupt-updated variable calls this so ticks keep
 * coming (inserted by tools/patch_generated_sm1.py). */
void rt_idle(void);

/* ---- geometry transformation engine (COP2) -------------------------------- */
/* The generated code passes constant command words: each listed one goes straight to
 * its own routine, compiled with the word's options fixed (runtime/gte_ops.h, from
 * tools/gen_gte_ops.py); any other word takes the general routine. */
#include "gte_ops.h"
void gte_execute_any(uint32_t instruction);
#define GTE_DECLARE(word) void gte_op_##word(void);
GTE_OPS(GTE_DECLARE)
#undef GTE_DECLARE
static inline __attribute__((always_inline)) void gte_execute(uint32_t instruction)
{
    switch (instruction) {
#define GTE_CASE(word) case 0x##word##u: gte_op_##word(); return;
    GTE_OPS(GTE_CASE)
#undef GTE_CASE
    }
    gte_execute_any(instruction);
}
uint32_t gte_read_data(int reg);
uint32_t gte_read_control(int reg);
void gte_write_data(int reg, uint32_t value);
void gte_write_control(int reg, uint32_t value);
int gte_condition(void);
/* With vertex tags: MFC2 (the SXY registers' tag into the CPU register), MTC2 and
 * LWC2 (a tagged SXY word keeps its projection in the GTE), SWC2 (onto the word). */
uint32_t gte_read_data_t(CpuContext *c, int cpu_reg, int reg);
void gte_write_data_t(int reg, uint32_t value, const VTag *tag);
void gte_load_word(int reg, uint32_t address);
void gte_store_word(uint32_t address, int reg);
