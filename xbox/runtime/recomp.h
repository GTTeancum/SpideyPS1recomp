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
 * tag by changing it); x16, y16 the position in sixteenths of a pixel; z the camera
 * depth. A packet vertex uses a tag's position only when its own coordinates are the
 * position's pixel, give or take the one-pixel crack offsets the game adds, or the
 * GTE's saturation of a point beyond the screen -- unless the tag is certified:
 * `native` (the PC's NativeScreen) is 0x80000000 | the SXY word a hook proved the
 * position belongs to (a subdivision grid point, an edge point the game nudged), and
 * then exactly that word qualifies however far the position is from it.
 */
#define VTAG_CERTIFIED 0x80000000u
#define VTAG_SXY 0x07FF07FFu
typedef struct VTag {
    uint32_t value;
    int16_t x16, y16;
    float z;
    uint32_t native;
} VTag;

static inline int16_t vtag_fixed(float v) { v *= 16.0f; return (int16_t)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : (int)(v + (v < 0 ? -0.5f : 0.5f))); }
static inline int vtag_coord_ok(int packet, int precise_floor)
{
    int d = packet - precise_floor;
    return (d >= -1 && d <= 1) || (packet >= 510 && precise_floor > packet) || (packet <= -510 && precise_floor < packet);
}
/* Whether an SXY word (11-bit signed x, y) belongs to the tag's position (the PC's
 * GteScreen.MatchesPackedScreen). */
static inline int vtag_matches(uint32_t word, const VTag *t)
{
    if (t->native & VTAG_CERTIFIED) return (word & VTAG_SXY) == (t->native & VTAG_SXY);
    int px = (int)(word << 21) >> 21, py = (int)((word >> 16) << 21) >> 21;
    return vtag_coord_ok(px, t->x16 >> 4) && vtag_coord_ok(py, t->y16 >> 4);
}

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
const VTag *vtag_containing_slow(uint32_t address);
void vtag_stored(uint32_t address, uint32_t value, const VTag *tag);
const VTag *vtag_pick_slow(const VTag *a, const VTag *b);

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

#ifdef RT_NO_VTAGS   /* timing comparison: the generated code's tag work compiled out */
#define mem_r32_t(c, r, a) mem_r32(a)
#define mem_w32_t(a, v, t) mem_w32(a, v)
#define mem_w16_t(a, v, t) mem_w16(a, v)
#define vtag_containing(a) ((const VTag *)0)
#define vtag_pick(a, b) ((const VTag *)0)
#else
/* LW; LB/LBU/LH/LHU take the containing word's tag (vtag_containing: mesh builders
 * unpack SXY as halves and recombine them, the PC's GteScreen.ContainingVertex). */
/* Whether the word's 4 KB page has ever held a tag: checked inline so the common case
 * (memory no vertex has touched) costs a test instead of a call. A call on every load
 * and store halved the frame rate in level 3. */
static inline int vtag_page_present(uint32_t a)
{
    uint32_t p = a & 0x1FFFFFFFu;
    if (p < RECOMP_RAM_SIZE) return vtag_pages[p >> 12] != 0;
    return (p & ~0x3FFu) == 0x1F800000u && vtag_pages[VTAG_PAGES - 1] != 0;
}

static inline const VTag *vtag_containing(uint32_t a) { return vtag_page_present(a) ? vtag_containing_slow(a) : 0; }
static inline const VTag *vtag_pick(const VTag *a, const VTag *b) { return !a ? b : (!b || a == b) ? a : vtag_pick_slow(a, b); }

static inline uint32_t mem_r32_t(CpuContext *c, int r, uint32_t a)
{
    uint32_t v = mem_r32(a);
    c->t[r] = vtag_page_present(a) ? vtag_load(a, v) : 0;
    return v;
}

/* SW: the register's tag goes with the word; an untagged word clears the slot. */
static inline void mem_w32_t(uint32_t a, uint32_t v, const VTag *t)
{
    mem_w32(a, v);
    if (t || vtag_page_present(a)) vtag_stored(a, v, t);
}
/* SH: a coordinate written as a half; once the halves make up the word the register's
 * tag describes, the word takes the tag (the PC's GteScreen.StoreU16). */
void vtag_half_stored(uint32_t address, const VTag *tag);
static inline void mem_w16_t(uint32_t a, uint16_t v, const VTag *t)
{
    mem_w16(a, v);
    if (t) vtag_half_stored(a, t);
}
#endif

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
