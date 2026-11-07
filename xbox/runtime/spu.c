/*
 * The PlayStation sound chip (see spu.h): a port of the PC runtime's Hardware/Spu.cs
 * (voices, envelopes, mixing) and Hardware/XaAudio.cs (the CD audio ring and its
 * resampling to 44.1 kHz). A fix to the PC's belongs here too.
 */
#include <string.h>
#include "audio.h"
#include "runtime.h"
#include "spu.h"

#define BASE 0x1F801C00u
#define RAM_SIZE (512u * 1024u)
#define RATE 44100
#define FRAMES_PER_TICK (RATE / 60)

static uint8_t g_ram[RAM_SIZE];

static const int K0[5] = { 0, 60, 115, 98, 122 }, K1[5] = { 0, 0, -52, -55, -60 };
static const int StepDown[4] = { -8, -7, -6, -5 }, StepUp[4] = { 7, 6, 5, 4 };

static const int16_t Gauss[512] = {
#include "spu_gauss.inc"
};

enum { OFF, ATTACK, DECAY, SUSTAIN, RELEASE };

typedef struct {
    uint16_t vol_l, vol_r, pitch, start, repeat, adsr_lo, adsr_hi;
    int16_t adsr_vol;
    uint32_t addr, counter;
    int phase, adsr_cycles, endx, ignore_loop, has_block;
    int old, older;
    int16_t cur_l, cur_r;
    int cyc_l, cyc_r;
    int16_t buf[31];
} Voice;

static Voice g_v[24];
static uint16_t g_main_l, g_main_r, g_rev_l, g_rev_r;
static int16_t g_main_cur_l, g_main_cur_r;
static int g_main_cyc_l, g_main_cyc_r;
static uint16_t g_kon, g_kon_hi, g_koff, g_koff_hi, g_pmon, g_pmon_hi, g_non, g_non_hi, g_eon, g_eon_hi;
static uint32_t g_endx, g_kon_pending, g_koff_pending;
static uint16_t g_spucnt, g_transfer_addr, g_transfer_ctrl = 4, g_cd_l, g_cd_r, g_ext_l, g_ext_r, g_rev_start;
static int g_mix_ll = 0x80, g_mix_lr, g_mix_rl, g_mix_rr = 0x80;
static int g_noise_level = 1, g_noise_timer;
volatile int spu_cd_muted;

int spu_is_reg(uint32_t p) { return p >= BASE && p < BASE + 0x400u; }

/* ---- registers ----------------------------------------------------------------------- */
uint16_t spu_read16(uint32_t phys)
{
    uint32_t off = phys - BASE;
    if (off < 0x180u) {
        Voice *v = &g_v[off >> 4];
        switch (off & 0xFu) {
        case 0x0: return v->vol_l;
        case 0x2: return v->vol_r;
        case 0x4: return v->pitch;
        case 0x6: return v->start;
        case 0x8: return v->adsr_lo;
        case 0xA: return v->adsr_hi;
        case 0xC: return ((g_kon_pending >> (off >> 4)) & 1u) || (v->phase == ATTACK && v->adsr_vol == 0) ? 1 : (uint16_t)v->adsr_vol;
        case 0xE: return v->repeat;
        }
        return 0;
    }
    switch (off) {
    case 0x180: return g_main_l;
    case 0x182: return g_main_r;
    case 0x184: return g_rev_l;
    case 0x186: return g_rev_r;
    case 0x188: return g_kon;
    case 0x18A: return g_kon_hi;
    case 0x18C: return g_koff;
    case 0x18E: return g_koff_hi;
    case 0x190: return g_pmon;
    case 0x192: return g_pmon_hi;
    case 0x194: return g_non;
    case 0x196: return g_non_hi;
    case 0x198: return g_eon;
    case 0x19A: return g_eon_hi;
    case 0x19C: return (uint16_t)g_endx;
    case 0x19E: return (uint16_t)(g_endx >> 16);
    case 0x1A2: return g_rev_start;
    case 0x1A6: return g_transfer_addr;
    case 0x1AA: return g_spucnt;
    case 0x1AC: return g_transfer_ctrl;
    case 0x1AE: return (uint16_t)(g_spucnt & 0x3F);
    case 0x1B0: return g_cd_l;
    case 0x1B2: return g_cd_r;
    case 0x1B4: return g_ext_l;
    case 0x1B6: return g_ext_r;
    }
    return 0;
}

static void key_on(uint16_t mask, int hi)
{
    uint32_t bits = (uint32_t)mask << (hi ? 16 : 0);
    g_kon_pending |= bits;
    g_koff_pending &= ~bits;
    g_endx &= ~bits;
}

static void key_off(uint16_t mask, int hi)
{
    uint32_t bits = (uint32_t)mask << (hi ? 16 : 0);
    g_koff_pending |= bits & ~g_kon_pending;
}

void spu_write16(uint32_t phys, uint16_t val)
{
    uint32_t off = phys - BASE;
    if (off < 0x180u) {
        Voice *v = &g_v[off >> 4];
        switch (off & 0xFu) {
        case 0x0: v->vol_l = val; break;
        case 0x2: v->vol_r = val; break;
        case 0x4: v->pitch = val; break;
        case 0x6: v->start = val; break;
        case 0x8: v->adsr_lo = val; break;
        case 0xA: v->adsr_hi = val; break;
        case 0xC: v->adsr_vol = (int16_t)val; break;
        case 0xE: v->repeat = val; v->ignore_loop = 1; break;
        }
        return;
    }
    switch (off) {
    case 0x180: g_main_l = val; break;
    case 0x182: g_main_r = val; break;
    case 0x184: g_rev_l = val; break;
    case 0x186: g_rev_r = val; break;
    case 0x188: key_on(val, 0); g_kon = val; break;
    case 0x18A: key_on(val, 1); g_kon_hi = val; break;
    case 0x18C: key_off(val, 0); g_koff = val; break;
    case 0x18E: key_off(val, 1); g_koff_hi = val; break;
    case 0x190: g_pmon = val; break;
    case 0x192: g_pmon_hi = val; break;
    case 0x194: g_non = val; break;
    case 0x196: g_non_hi = val; break;
    case 0x198: g_eon = val; break;
    case 0x19A: g_eon_hi = val; break;
    case 0x1A2: g_rev_start = val; break;
    case 0x1A6: g_transfer_addr = val; break;
    case 0x1AA: g_spucnt = val; break;
    case 0x1AC: g_transfer_ctrl = val; break;
    case 0x1B0: g_cd_l = val; break;
    case 0x1B2: g_cd_r = val; break;
    case 0x1B4: g_ext_l = val; break;
    case 0x1B6: g_ext_r = val; break;
    }
}

void spu_dma_write(uint32_t madr, uint32_t bytes)
{
    uint32_t to = (uint32_t)g_transfer_addr << 3;
    for (uint32_t i = 0; i < bytes; i++) g_ram[(to + i) & (RAM_SIZE - 1u)] = (uint8_t)mem_r8(madr + i);
}

void spu_cd_mix(uint8_t ltol, uint8_t ltor, uint8_t rtor, uint8_t rtol)
{
    g_mix_ll = ltol; g_mix_lr = ltor; g_mix_rl = rtol; g_mix_rr = rtor;
}

/* ---- CD audio ring (XaAudio) ------------------------------------------------------------- */
#define XA_CAPACITY (1u << 17)
#define XA_PRIME 1024           /* frames queued before playback starts */
#define XA_STARTUP 2048         /* silent output frames then, so a late sector leaves no gap */
#define XA_MAX_HOLD 8192        /* frames of underrun before playback stops */
static uint32_t g_xa_ring[XA_CAPACITY];
static uint32_t g_xa_read, g_xa_write, g_xa_count;
static int g_xa_rate = 37800, g_xa_playing, g_xa_startup, g_xa_underrun;
static uint32_t g_xa_pos;      /* 16.16 */
static int16_t g_xa_s0l, g_xa_s0r, g_xa_s1l, g_xa_s1r;

void spu_xa_reset(void)
{
    g_xa_read = g_xa_write = g_xa_count = 0;
    g_xa_playing = 0; g_xa_pos = 0; g_xa_startup = 0; g_xa_underrun = 0;
    g_xa_s0l = g_xa_s0r = g_xa_s1l = g_xa_s1r = 0;
}

int spu_xa_buffered(void) { return (int)g_xa_count; }

void spu_xa_push(const int16_t *pcm, int frames, int rate)
{
    g_xa_rate = rate;
    for (int i = 0; i < frames; i++) {
        g_xa_ring[g_xa_write] = (uint16_t)pcm[i * 2] | ((uint32_t)(uint16_t)pcm[i * 2 + 1] << 16);
        g_xa_write = (g_xa_write + 1u) & (XA_CAPACITY - 1u);
        if (g_xa_count < XA_CAPACITY) g_xa_count++;
        else g_xa_read = (g_xa_read + 1u) & (XA_CAPACITY - 1u);
    }
    if (!g_xa_playing && g_xa_count >= XA_PRIME) { g_xa_playing = 1; g_xa_startup = XA_STARTUP; }
}

static int xa_next(int16_t *l, int16_t *r)
{
    if (!g_xa_playing) return 0;
    if (g_xa_startup > 0) { g_xa_startup--; *l = *r = 0; return 1; }
    while (g_xa_pos >= 0x10000u) {
        g_xa_s0l = g_xa_s1l; g_xa_s0r = g_xa_s1r;
        if (g_xa_count) {
            uint32_t packed = g_xa_ring[g_xa_read];
            g_xa_read = (g_xa_read + 1u) & (XA_CAPACITY - 1u);
            g_xa_count--;
            g_xa_s1l = (int16_t)(packed & 0xFFFFu);
            g_xa_s1r = (int16_t)(packed >> 16);
            g_xa_underrun = 0;
        } else {
            if (++g_xa_underrun > XA_MAX_HOLD) { g_xa_playing = 0; *l = *r = 0; return 0; }
            g_xa_s1l = (int16_t)(g_xa_s1l * 31 / 32);
            g_xa_s1r = (int16_t)(g_xa_s1r * 31 / 32);
        }
        g_xa_pos -= 0x10000u;
    }
    int f = (int)(g_xa_pos >> 1);
    *l = (int16_t)(g_xa_s0l + (((g_xa_s1l - g_xa_s0l) * f) >> 15));
    *r = (int16_t)(g_xa_s0r + (((g_xa_s1r - g_xa_s0r) * f) >> 15));
    g_xa_pos += (uint32_t)((uint64_t)g_xa_rate * 65536u / RATE);
    return 1;
}

/* ---- voices ------------------------------------------------------------------------------- */
static int16_t clamp16(int v) { return (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v); }

static void env_tick(int16_t *level, int *cycles_count, int shift, int step_idx, int exp, int decrease, int neg)
{
    int mag = neg ? -*level : *level;
    int cycles = 1 << (shift - 11 > 0 ? shift - 11 : 0);
    int step = (decrease ? StepDown : StepUp)[step_idx] << (11 - shift > 0 ? 11 - shift : 0);
    if (exp && !decrease && mag > 0x6000) cycles *= 4;
    if (exp && decrease) { step = step * mag / 0x8000; if (step == 0) step = -1; }
    if (++*cycles_count < cycles) return;
    *cycles_count = 0;
    int next = mag + step;
    next = next < 0 ? 0 : next > 0x7FFF ? 0x7FFF : next;
    *level = (int16_t)(neg ? -next : next);
}

static void sweep_tick(uint16_t reg, int16_t *level, int *cycles)
{
    if (!(reg & 0x8000)) { *level = (int16_t)(reg << 1); return; }
    env_tick(level, cycles, (reg >> 2) & 0x1F, reg & 3, (reg & 0x4000) != 0, (reg & 0x2000) != 0, (reg & 0x1000) != 0);
}

static int16_t decode_sample(Voice *v, int nibble, int shift, int k0, int k1)
{
    int s = (nibble << 28) >> 28;
    s = (s << 12) >> shift;
    s += (v->old * k0 + v->older * k1) >> 6;
    s = clamp16(s);
    v->older = v->old;
    v->old = s;
    return (int16_t)s;
}

static void decode_block(Voice *v, int index)
{
    v->buf[0] = v->buf[28]; v->buf[1] = v->buf[29]; v->buf[2] = v->buf[30];
    uint32_t addr = v->addr & (RAM_SIZE - 1u);
    uint8_t hdr = g_ram[addr], flags = g_ram[(addr + 1u) & (RAM_SIZE - 1u)];
    int shift = hdr & 0xF;
    if (shift > 12) shift = 9;
    int filter = (hdr >> 4) & 7;
    if (filter > 4) filter = 4;
    int pos = 3;
    for (int i = 0; i < 14; i++) {
        uint8_t b = g_ram[(addr + 2u + (uint32_t)i) & (RAM_SIZE - 1u)];
        v->buf[pos++] = decode_sample(v, b & 0xF, shift, K0[filter], K1[filter]);
        v->buf[pos++] = decode_sample(v, b >> 4, shift, K0[filter], K1[filter]);
    }
    if ((flags & 4) && !v->ignore_loop) v->repeat = (uint16_t)(addr >> 3);
    v->addr += 16;
    if (flags & 1) {
        v->endx = 1;
        g_endx |= 1u << index;
        v->addr = (uint32_t)v->repeat << 3;
        if (!(flags & 2)) { v->adsr_vol = 0; v->phase = RELEASE; }
    }
}

static void tick_adsr(Voice *v)
{
    int lo = v->adsr_lo, hi = v->adsr_hi;
    int sus_level = ((lo & 0xF) + 1) << 11;
    if (v->phase == ATTACK && v->adsr_vol >= 0x7FFF) { v->phase = DECAY; v->adsr_cycles = 0; }
    if (v->phase == DECAY && v->adsr_vol <= sus_level) { v->phase = SUSTAIN; v->adsr_cycles = 0; }
    int decrease = 0, shift = 0, step = 0, exp = 0;
    switch (v->phase) {
    case ATTACK: decrease = 0; shift = (lo >> 10) & 0x1F; step = (lo >> 8) & 3; exp = (lo >> 15) & 1; break;
    case DECAY: decrease = 1; shift = (lo >> 4) & 0xF; step = 0; exp = 1; break;
    case SUSTAIN: decrease = (hi >> 14) & 1; shift = (hi >> 8) & 0x1F; step = (hi >> 6) & 3; exp = (hi >> 15) & 1; break;
    case RELEASE: decrease = 1; shift = hi & 0x1F; step = 0; exp = (hi >> 5) & 1; break;
    }
    env_tick(&v->adsr_vol, &v->adsr_cycles, shift, step, exp, decrease, 0);
    if (v->phase == RELEASE && v->adsr_vol == 0) v->phase = OFF;
}

static void resolve_keys(void)
{
    if (!g_kon_pending && !g_koff_pending) return;
    for (int i = 0; i < 24; i++) {
        uint32_t bit = 1u << i;
        Voice *v = &g_v[i];
        if (g_kon_pending & bit) {
            v->phase = ATTACK; v->adsr_vol = 0; v->adsr_cycles = 0;
            v->addr = (uint32_t)v->start << 3; v->ignore_loop = 0; v->counter = 0;
            v->old = v->older = 0; v->has_block = 0; v->endx = 0;
            g_endx &= ~bit;
        } else if ((g_koff_pending & bit) && v->phase != OFF) v->phase = RELEASE;
    }
    g_kon_pending = g_koff_pending = 0;
}

static void tick_noise(void)
{
    int shift = (g_spucnt >> 10) & 0xF, step = ((g_spucnt >> 8) & 3) + 4;
    g_noise_timer -= step;
    int parity = ((g_noise_level >> 15) ^ (g_noise_level >> 12) ^ (g_noise_level >> 11) ^ (g_noise_level >> 10) ^ 1) & 1;
    if (g_noise_timer < 0) {
        g_noise_level = ((g_noise_level << 1) | parity) & 0xFFFF;
        g_noise_timer += 0x20000 >> shift;
        if (g_noise_timer < 0) g_noise_timer += 0x20000 >> shift;
    }
}

static void voices(int *out_l, int *out_r)
{
    resolve_keys();
    tick_noise();
    int sum_l = 0, sum_r = 0, prev = 0;
    uint32_t non = (uint32_t)g_non | ((uint32_t)g_non_hi << 16), pmon = (uint32_t)g_pmon | ((uint32_t)g_pmon_hi << 16);
    for (int i = 0; i < 24; i++) {
        Voice *v = &g_v[i];
        if (v->phase == OFF) { prev = 0; continue; }
        tick_adsr(v);
        sweep_tick(v->vol_l, &v->cur_l, &v->cyc_l);
        sweep_tick(v->vol_r, &v->cur_r, &v->cyc_r);
        if (!v->has_block) { memset(v->buf, 0, sizeof v->buf); decode_block(v, i); v->has_block = 1; }
        int sample;
        if (non & (1u << i)) sample = (int16_t)g_noise_level;
        else {
            int idx = (int)(v->counter >> 12), fi = (int)((v->counter >> 4) & 0xFF);
            sample = ((Gauss[0x0FF - fi] * v->buf[idx]) >> 15) + ((Gauss[0x1FF - fi] * v->buf[idx + 1]) >> 15) +
                     ((Gauss[0x100 + fi] * v->buf[idx + 2]) >> 15) + ((Gauss[fi] * v->buf[idx + 3]) >> 15);
        }
        int amp = (sample * v->adsr_vol) >> 15;
        int step = v->pitch;
        if (i > 0 && (pmon & (1u << i))) {
            int factor = (prev < -0x8000 ? -0x8000 : prev > 0x7FFF ? 0x7FFF : prev) + 0x8000;
            step = ((int16_t)(uint16_t)step * factor >> 15) & 0xFFFF;
        }
        if (step > 0x3FFF) step = 0x4000;
        v->counter += (uint32_t)step;
        if ((v->counter >> 12) >= 28) { v->counter -= 28u << 12; decode_block(v, i); }
        prev = amp;
        sum_l += (amp * v->cur_l) >> 15;
        sum_r += (amp * v->cur_r) >> 15;
    }
    *out_l = clamp16(sum_l);
    *out_r = clamp16(sum_r);
}

/* ---- mixing and output ---------------------------------------------------------------------- */
static unsigned g_audio;
volatile uint32_t rt_spu_peak;    /* loudest sample mixed since the last status (log.c) */
volatile uint32_t rt_spu_voices;  /* most voices sounding at once since then */

void spu_tick(void)
{
    static int16_t pcm[FRAMES_PER_TICK * 2];
    if (rt_movie_playing) return;            /* the movie holds the CD and the speakers */
    if (!audio_owned(g_audio)) g_audio = audio_open(RATE, 6);
    for (int n = 0; n < FRAMES_PER_TICK; n++) {
        sweep_tick(g_main_l, &g_main_cur_l, &g_main_cyc_l);
        sweep_tick(g_main_r, &g_main_cur_r, &g_main_cyc_r);
        int l, r;
        voices(&l, &r);
        int16_t xl, xr;
        if (xa_next(&xl, &xr) && !spu_cd_muted) {
            int al = clamp16((xl * g_mix_ll + xr * g_mix_rl) >> 7), ar = clamp16((xl * g_mix_lr + xr * g_mix_rr) >> 7);
            l += (al * (int16_t)g_cd_l) >> 15;
            r += (ar * (int16_t)g_cd_r) >> 15;
        }
        l = clamp16(l) * g_main_cur_l >> 15;
        r = clamp16(r) * g_main_cur_r >> 15;
        pcm[n * 2] = (int16_t)l;
        pcm[n * 2 + 1] = (int16_t)r;
        uint32_t a = (uint32_t)(l < 0 ? -l : l);
        if (a > rt_spu_peak) rt_spu_peak = a;
    }
    if (audio_owned(g_audio)) audio_write(pcm, FRAMES_PER_TICK);
    uint32_t sounding = 0;
    for (int i = 0; i < 24; i++) sounding += g_v[i].phase != OFF;
    if (sounding > rt_spu_voices) rt_spu_voices = sounding;
}
