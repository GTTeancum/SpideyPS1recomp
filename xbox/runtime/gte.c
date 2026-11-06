/*
 * Geometry transformation engine (COP2), a port of the PC runtime's Hardware/Gte.cs
 * without its host-precision screen tracking (widescreen field of view, precise
 * winding and perspective/60 FPS provenance): the console's integer behaviour only.
 */
#include "hwgpu.h"
#include "prof.h"
#include "runtime.h"

/* The arithmetic is inlined into one routine per command word (gte_ops.h), where the
 * command's shift and saturation mode are constants. */
#define INLINE static inline __attribute__((always_inline))

static int16_t V[9];
static uint8_t RGBC_R, RGBC_G, RGBC_B, RGBC_CODE;
static uint16_t OTZ;
static int32_t IR0, IR1, IR2, IR3;
static int16_t SX[3], SY[3];
static uint16_t SZ[4];
static uint32_t RGB[3], RES1;
static int32_t MAC0, MAC1, MAC2, MAC3;
static uint32_t LZCS, LZCR;
static int16_t RT[9], LLM[9], LCM[9];
static int32_t TR[3], BK[3], FC[3], OFX, OFY, DQB;
static uint16_t H;
static int16_t DQA, ZSF3, ZSF4;
static uint32_t FLAG;
static uint8_t UNR[0x101];
static int g_unr_ready;
int rt_fov_num = 1, rt_fov_den = 1;
uint16_t (*rt_rtp_depth_hook)(int32_t vx, int32_t vy, int32_t vz, uint16_t sz);

INLINE void flag(int bit) { FLAG |= 1u << bit; }

INLINE int32_t sat_ir(int n, int32_t v, int lm)
{
    int32_t min = lm ? 0 : -0x8000;
    if (v < min) { v = min; flag(25 - n); }
    else if (v > 0x7FFF) { v = 0x7FFF; flag(25 - n); }
    return v;
}
INLINE int32_t sat_ir0(int32_t v) { if (v < 0) { flag(12); return 0; } if (v > 0x1000) { flag(12); return 0x1000; } return v; }
INLINE int32_t sat_color(int n, int32_t v) { if (v < 0) { flag(21 - n); return 0; } if (v > 0xFF) { flag(21 - n); return 0xFF; } return v; }
INLINE int32_t sat_sz(int32_t v) { if (v < 0) { flag(18); return 0; } if (v > 0xFFFF) { flag(18); return 0xFFFF; } return v; }
INLINE int32_t sat_x(int32_t v) { if (v < -0x400) { flag(14); return -0x400; } if (v > 0x3FF) { flag(14); return 0x3FF; } return v; }
INLINE int32_t sat_y(int32_t v) { if (v < -0x400) { flag(13); return -0x400; } if (v > 0x3FF) { flag(13); return 0x3FF; } return v; }
INLINE int64_t check_mac0(int64_t v) { if (v > 0x7FFFFFFFLL) flag(16); else if (v < -0x80000000LL) flag(15); return v; }
INLINE void check_mac(int n, int64_t v) { if (v >= (1LL << 43)) flag(31 - n); else if (v < -(1LL << 43)) flag(28 - n); }

INLINE void set_mac(int n, int64_t v, int sf, int lm)
{
    check_mac(n, v);
    int32_t m = (int32_t)(v >> sf);
    if (n == 1) { MAC1 = m; IR1 = sat_ir(1, m, lm); }
    else if (n == 2) { MAC2 = m; IR2 = sat_ir(2, m, lm); }
    else { MAC3 = m; IR3 = sat_ir(3, m, lm); }
}

INLINE void mat_vec(const int16_t *mx, int32_t t0, int32_t t1, int32_t t2, int32_t vx, int32_t vy, int32_t vz, int sf, int lm)
{
    set_mac(1, ((int64_t)t0 << 12) + (int64_t)mx[0] * vx + (int64_t)mx[1] * vy + (int64_t)mx[2] * vz, sf, lm);
    set_mac(2, ((int64_t)t1 << 12) + (int64_t)mx[3] * vx + (int64_t)mx[4] * vy + (int64_t)mx[5] * vz, sf, lm);
    set_mac(3, ((int64_t)t2 << 12) + (int64_t)mx[6] * vx + (int64_t)mx[7] * vy + (int64_t)mx[8] * vz, sf, lm);
}

INLINE void push_color(void)
{
    int32_t r = sat_color(0, MAC1 >> 4), g = sat_color(1, MAC2 >> 4), b = sat_color(2, MAC3 >> 4);
    RGB[0] = RGB[1]; RGB[1] = RGB[2];
    RGB[2] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)RGBC_CODE << 24);
}

INLINE void interp(int64_t in1, int64_t in2, int64_t in3, int sf, int lm)
{
    IR1 = sat_ir(1, (int32_t)((((int64_t)FC[0] << 12) - in1) >> sf), 0);
    IR2 = sat_ir(2, (int32_t)((((int64_t)FC[1] << 12) - in2) >> sf), 0);
    IR3 = sat_ir(3, (int32_t)((((int64_t)FC[2] << 12) - in3) >> sf), 0);
    set_mac(1, (int64_t)IR1 * IR0 + in1, sf, lm);
    set_mac(2, (int64_t)IR2 * IR0 + in2, sf, lm);
    set_mac(3, (int64_t)IR3 * IR0 + in3, sf, lm);
    push_color();
}

INLINE void modulate(int sf, int lm)
{
    set_mac(1, ((int64_t)RGBC_R * IR1) << 4, sf, lm);
    set_mac(2, ((int64_t)RGBC_G * IR2) << 4, sf, lm);
    set_mac(3, ((int64_t)RGBC_B * IR3) << 4, sf, lm);
    push_color();
}

static int clz16(uint32_t v) { return v & 0xFFFFu ? __builtin_clz(v & 0xFFFFu) - 16 : 16; }

static uint32_t divide(uint32_t h, uint32_t sz3)
{
    if (!g_unr_ready) {
        for (int i = 0; i < 0x101; i++) {
            int v = (0x40000 / (i + 0x100) + 1) / 2 - 0x101;
            UNR[i] = (uint8_t)(v < 0 ? 0 : v > 0xFF ? 0xFF : v);
        }
        g_unr_ready = 1;
    }
    if (h >= sz3 * 2) { flag(17); return 0x1FFFF; }
    /* The same steps in 32-bit arithmetic where the values fit (d < 2^16, u <= 0x200),
     * one 32x32->64 multiply at the end. */
    int z = clz16(sz3);
    uint32_t n = h << z, d = sz3 << z;
    int32_t idx = d < 0x7FC0 ? 0 : (int32_t)((d - 0x7FC0) >> 7);
    if (idx > 0x100) idx = 0x100;
    uint32_t u = (uint32_t)UNR[idx] + 0x101;
    d = (0x2000080u - d * u) >> 8;
    d = (0x0000080u + d * u) >> 8;
    uint64_t res = ((uint64_t)n * d + 0x8000) >> 16;
    return res > 0x1FFFF ? 0x1FFFFu : (uint32_t)res;
}

/* px * num / den, truncated toward zero as C divides. Widening (num <= den) keeps the
 * quotient no larger than px, so for |px| < 2^31 it fits 32 bits and one 64-by-32
 * divide instruction gives it, without the 64-bit division call (or a double division
 * and two rounding-mode switches to truncate it). */
INLINE int64_t fov_scale(int64_t px)
{
    int64_t n = px * rt_fov_num;
    if (rt_fov_num > 0 && rt_fov_num <= rt_fov_den && px > -0x80000000LL && px < 0x80000000LL) {
        int32_t q, r;
        __asm__("idivl %4" : "=a"(q), "=d"(r) : "a"((uint32_t)n), "d"((uint32_t)((uint64_t)n >> 32)), "rm"((int32_t)rt_fov_den));
        return q;
    }
    return n / rt_fov_den;
}

INLINE void rtp(int32_t vx, int32_t vy, int32_t vz, int sf, int lm, int last)
{
    int64_t m1 = ((int64_t)TR[0] << 12) + (int64_t)RT[0] * vx + (int64_t)RT[1] * vy + (int64_t)RT[2] * vz;
    int64_t m2 = ((int64_t)TR[1] << 12) + (int64_t)RT[3] * vx + (int64_t)RT[4] * vy + (int64_t)RT[5] * vz;
    int64_t m3 = ((int64_t)TR[2] << 12) + (int64_t)RT[6] * vx + (int64_t)RT[7] * vy + (int64_t)RT[8] * vz;
    check_mac(1, m1); check_mac(2, m2); check_mac(3, m3);
    MAC1 = (int32_t)(m1 >> sf); MAC2 = (int32_t)(m2 >> sf); MAC3 = (int32_t)(m3 >> sf);
    IR1 = sat_ir(1, MAC1, lm);
    IR2 = sat_ir(2, MAC2, lm);
    int32_t ir3flag = (int32_t)(m3 >> 12);
    if (ir3flag < -0x8000 || ir3flag > 0x7FFF) flag(22);
    int32_t lo = lm ? 0 : -0x8000;
    IR3 = MAC3 < lo ? lo : MAC3 > 0x7FFF ? 0x7FFF : MAC3;
    int32_t sz = sat_sz((int32_t)(m3 >> 12));
    SZ[0] = SZ[1]; SZ[1] = SZ[2]; SZ[2] = SZ[3]; SZ[3] = (uint16_t)sz;
    uint32_t div = divide(H, SZ[3]);
    /* Widescreen widens the view here, as the PC's Gte.Rtp does: the game culls and
     * orders on these coordinates, so it then submits the extra scenery itself. */
    int64_t px = (int64_t)div * IR1;
    if (rt_fov_num != rt_fov_den) px = fov_scale(px);
    int64_t sx = check_mac0(px + OFX); MAC0 = (int32_t)sx;
    int64_t sy = check_mac0((int64_t)div * IR2 + OFY); MAC0 = (int32_t)sy;
    int32_t nx = sat_x((int32_t)(sx >> 16)), ny = sat_y((int32_t)(sy >> 16));
    SX[0] = SX[1]; SX[1] = SX[2]; SX[2] = (int16_t)nx;
    SY[0] = SY[1]; SY[1] = SY[2]; SY[2] = (int16_t)ny;
    hw_note_depth(nx, ny, rt_rtp_depth_hook ? rt_rtp_depth_hook(vx, vy, vz, (uint16_t)sz) : (uint16_t)sz);
    if (last) {
        int64_t dp = check_mac0((int64_t)div * DQA + DQB);
        MAC0 = (int32_t)dp;
        IR0 = sat_ir0((int32_t)(dp >> 12));
    }
}

INLINE void ncs(int vec, int sf, int lm)
{
    mat_vec(LLM, 0, 0, 0, V[vec * 3], V[vec * 3 + 1], V[vec * 3 + 2], sf, lm);
    mat_vec(LCM, BK[0], BK[1], BK[2], IR1, IR2, IR3, sf, lm);
    push_color();
}
INLINE void ncds(int vec, int sf, int lm)
{
    mat_vec(LLM, 0, 0, 0, V[vec * 3], V[vec * 3 + 1], V[vec * 3 + 2], sf, lm);
    mat_vec(LCM, BK[0], BK[1], BK[2], IR1, IR2, IR3, sf, lm);
    interp(((int64_t)RGBC_R * IR1) << 4, ((int64_t)RGBC_G * IR2) << 4, ((int64_t)RGBC_B * IR3) << 4, sf, lm);
}
INLINE void nccs(int vec, int sf, int lm)
{
    mat_vec(LLM, 0, 0, 0, V[vec * 3], V[vec * 3 + 1], V[vec * 3 + 2], sf, lm);
    mat_vec(LCM, BK[0], BK[1], BK[2], IR1, IR2, IR3, sf, lm);
    modulate(sf, lm);
}

INLINE void mvmva(int sf, int lm, int mx, int vn, int cv)
{
    const int16_t *mat = mx == 1 ? LLM : mx == 2 ? LCM : RT;
    int32_t vx, vy, vz;
    if (vn < 3) { vx = V[vn * 3]; vy = V[vn * 3 + 1]; vz = V[vn * 3 + 2]; }
    else { vx = IR1; vy = IR2; vz = IR3; }
    if (cv == 2) {
        sat_ir(1, (int32_t)((((int64_t)FC[0] << 12) + (int64_t)mat[0] * vx) >> sf), lm);
        sat_ir(2, (int32_t)((((int64_t)FC[1] << 12) + (int64_t)mat[3] * vx) >> sf), lm);
        sat_ir(3, (int32_t)((((int64_t)FC[2] << 12) + (int64_t)mat[6] * vx) >> sf), lm);
        set_mac(1, (int64_t)mat[1] * vy + (int64_t)mat[2] * vz, sf, lm);
        set_mac(2, (int64_t)mat[4] * vy + (int64_t)mat[5] * vz, sf, lm);
        set_mac(3, (int64_t)mat[7] * vy + (int64_t)mat[8] * vz, sf, lm);
        return;
    }
    int32_t t0 = 0, t1 = 0, t2 = 0;
    if (cv == 0) { t0 = TR[0]; t1 = TR[1]; t2 = TR[2]; }
    else if (cv == 1) { t0 = BK[0]; t1 = BK[1]; t2 = BK[2]; }
    mat_vec(mat, t0, t1, t2, vx, vy, vz, sf, lm);
}

INLINE void gte_run(uint32_t cmd)
{
    FLAG = 0;
    int sf = (cmd & (1u << 19)) ? 12 : 0, lm = (cmd >> 10) & 1;
    int mx = (int)((cmd >> 17) & 3), vn = (int)((cmd >> 15) & 3), cv = (int)((cmd >> 13) & 3);
    switch (cmd & 0x3F) {
    case 0x01: rtp(V[0], V[1], V[2], sf, lm, 1); break;
    case 0x30: rtp(V[0], V[1], V[2], sf, lm, 0); rtp(V[3], V[4], V[5], sf, lm, 0); rtp(V[6], V[7], V[8], sf, lm, 1); break;
    case 0x06:
        MAC0 = (int32_t)check_mac0((int64_t)SX[0] * (SY[1] - SY[2]) + (int64_t)SX[1] * (SY[2] - SY[0]) + (int64_t)SX[2] * (SY[0] - SY[1]));
        break;
    case 0x2D: MAC0 = (int32_t)check_mac0((int64_t)ZSF3 * (SZ[1] + SZ[2] + SZ[3])); OTZ = (uint16_t)sat_sz(MAC0 >> 12); break;
    case 0x2E: MAC0 = (int32_t)check_mac0((int64_t)ZSF4 * (SZ[0] + SZ[1] + SZ[2] + SZ[3])); OTZ = (uint16_t)sat_sz(MAC0 >> 12); break;
    case 0x12: mvmva(sf, lm, mx, vn, cv); break;
    case 0x28:
        set_mac(1, (int64_t)IR1 * IR1, sf, lm); set_mac(2, (int64_t)IR2 * IR2, sf, lm); set_mac(3, (int64_t)IR3 * IR3, sf, lm);
        break;
    case 0x0C: {
        int32_t ir1 = IR1, ir2 = IR2, ir3 = IR3;
        set_mac(1, (int64_t)RT[4] * ir3 - (int64_t)RT[8] * ir2, sf, lm);
        set_mac(2, (int64_t)RT[8] * ir1 - (int64_t)RT[0] * ir3, sf, lm);
        set_mac(3, (int64_t)RT[0] * ir2 - (int64_t)RT[4] * ir1, sf, lm);
        break;
    }
    case 0x3D:
        set_mac(1, (int64_t)IR0 * IR1, sf, lm); set_mac(2, (int64_t)IR0 * IR2, sf, lm); set_mac(3, (int64_t)IR0 * IR3, sf, lm);
        push_color();
        break;
    case 0x3E:
        set_mac(1, ((int64_t)MAC1 << sf) + (int64_t)IR0 * IR1, sf, lm);
        set_mac(2, ((int64_t)MAC2 << sf) + (int64_t)IR0 * IR2, sf, lm);
        set_mac(3, ((int64_t)MAC3 << sf) + (int64_t)IR0 * IR3, sf, lm);
        push_color();
        break;
    case 0x10: interp((int64_t)RGBC_R << 16, (int64_t)RGBC_G << 16, (int64_t)RGBC_B << 16, sf, lm); break;
    case 0x2A:
        for (int i = 0; i < 3; i++)
            interp((int64_t)(RGB[0] & 0xFF) << 16, (int64_t)((RGB[0] >> 8) & 0xFF) << 16, (int64_t)((RGB[0] >> 16) & 0xFF) << 16, sf, lm);
        break;
    case 0x11: interp((int64_t)IR1 << 12, (int64_t)IR2 << 12, (int64_t)IR3 << 12, sf, lm); break;
    case 0x29: interp(((int64_t)RGBC_R * IR1) << 4, ((int64_t)RGBC_G * IR2) << 4, ((int64_t)RGBC_B * IR3) << 4, sf, lm); break;
    case 0x1E: ncs(0, sf, lm); break;
    case 0x20: ncs(0, sf, lm); ncs(1, sf, lm); ncs(2, sf, lm); break;
    case 0x13: ncds(0, sf, lm); break;
    case 0x16: ncds(0, sf, lm); ncds(1, sf, lm); ncds(2, sf, lm); break;
    case 0x1B: nccs(0, sf, lm); break;
    case 0x3F: nccs(0, sf, lm); nccs(1, sf, lm); nccs(2, sf, lm); break;
    case 0x1C: mat_vec(LCM, BK[0], BK[1], BK[2], IR1, IR2, IR3, sf, lm); modulate(sf, lm); break;
    case 0x14:
        mat_vec(LCM, BK[0], BK[1], BK[2], IR1, IR2, IR3, sf, lm);
        interp(((int64_t)RGBC_R * IR1) << 4, ((int64_t)RGBC_G * IR2) << 4, ((int64_t)RGBC_B * IR3) << 4, sf, lm);
        break;
    default: rt_missing("GTE", "command"); break;
    }
    if (FLAG & 0x7F87E000u) FLAG |= 0x80000000u;
}

volatile uint64_t rt_gte_op_cycles[64];
volatile uint32_t rt_gte_op_count[64];
#ifdef RT_PROFILE
#define TIMED(cmd)                                                \
    do {                                                          \
        PROF_BEGIN(g);                                            \
        gte_run(cmd);                                             \
        rt_gte_op_cycles[(cmd) & 0x3F] += __rdtsc() - prof_g;     \
        rt_gte_op_count[(cmd) & 0x3F]++;                          \
        PROF_END(PROF_GTE, g);                                    \
    } while (0)
#else
#define TIMED(cmd) gte_run(cmd)
#endif

/* Any command word; recomp.h's gte_execute sends the listed ones to their own routine. */
void gte_execute_any(uint32_t cmd) { TIMED(cmd); }

#define GTE_OP(word) void gte_op_##word(void) { TIMED(0x##word##u); }
GTE_OPS(GTE_OP)

static uint32_t pack(int16_t lo, int16_t hi) { return (uint32_t)(uint16_t)lo | ((uint32_t)(uint16_t)hi << 16); }

uint32_t gte_read_data(int reg)
{
    switch (reg & 31) {
    case 0: return pack(V[0], V[1]);
    case 1: return (uint32_t)(int32_t)V[2];
    case 2: return pack(V[3], V[4]);
    case 3: return (uint32_t)(int32_t)V[5];
    case 4: return pack(V[6], V[7]);
    case 5: return (uint32_t)(int32_t)V[8];
    case 6: return (uint32_t)RGBC_R | ((uint32_t)RGBC_G << 8) | ((uint32_t)RGBC_B << 16) | ((uint32_t)RGBC_CODE << 24);
    case 7: return OTZ;
    case 8: return (uint32_t)IR0;
    case 9: return (uint32_t)IR1;
    case 10: return (uint32_t)IR2;
    case 11: return (uint32_t)IR3;
    case 12: return pack(SX[0], SY[0]);
    case 13: return pack(SX[1], SY[1]);
    case 14: case 15: return pack(SX[2], SY[2]);
    case 16: return SZ[0];
    case 17: return SZ[1];
    case 18: return SZ[2];
    case 19: return SZ[3];
    case 20: return RGB[0];
    case 21: return RGB[1];
    case 22: return RGB[2];
    case 23: return RES1;
    case 24: return (uint32_t)MAC0;
    case 25: return (uint32_t)MAC1;
    case 26: return (uint32_t)MAC2;
    case 27: return (uint32_t)MAC3;
    case 28: case 29: {
        int32_t r = IR1 >> 7, g = IR2 >> 7, b = IR3 >> 7;
        r = r < 0 ? 0 : r > 0x1F ? 0x1F : r; g = g < 0 ? 0 : g > 0x1F ? 0x1F : g; b = b < 0 ? 0 : b > 0x1F ? 0x1F : b;
        return (uint32_t)r | ((uint32_t)g << 5) | ((uint32_t)b << 10);
    }
    case 30: return LZCS;
    default: return LZCR;
    }
}

void gte_write_data(int reg, uint32_t val)
{
    switch (reg & 31) {
    case 0: V[0] = (int16_t)val; V[1] = (int16_t)(val >> 16); break;
    case 1: V[2] = (int16_t)val; break;
    case 2: V[3] = (int16_t)val; V[4] = (int16_t)(val >> 16); break;
    case 3: V[5] = (int16_t)val; break;
    case 4: V[6] = (int16_t)val; V[7] = (int16_t)(val >> 16); break;
    case 5: V[8] = (int16_t)val; break;
    case 6: RGBC_R = (uint8_t)val; RGBC_G = (uint8_t)(val >> 8); RGBC_B = (uint8_t)(val >> 16); RGBC_CODE = (uint8_t)(val >> 24); break;
    case 7: OTZ = (uint16_t)val; break;
    case 8: IR0 = (int16_t)val; break;
    case 9: IR1 = (int16_t)val; break;
    case 10: IR2 = (int16_t)val; break;
    case 11: IR3 = (int16_t)val; break;
    case 12: SX[0] = (int16_t)val; SY[0] = (int16_t)(val >> 16); break;
    case 13: SX[1] = (int16_t)val; SY[1] = (int16_t)(val >> 16); break;
    case 14: SX[2] = (int16_t)val; SY[2] = (int16_t)(val >> 16); break;
    case 15: SX[0] = SX[1]; SY[0] = SY[1]; SX[1] = SX[2]; SY[1] = SY[2]; SX[2] = (int16_t)val; SY[2] = (int16_t)(val >> 16); break;
    case 16: SZ[0] = (uint16_t)val; break;
    case 17: SZ[1] = (uint16_t)val; break;
    case 18: SZ[2] = (uint16_t)val; break;
    case 19: SZ[3] = (uint16_t)val; break;
    case 20: RGB[0] = val; break;
    case 21: RGB[1] = val; break;
    case 22: RGB[2] = val; break;
    case 23: RES1 = val; break;
    case 24: MAC0 = (int32_t)val; break;
    case 25: MAC1 = (int32_t)val; break;
    case 26: MAC2 = (int32_t)val; break;
    case 27: MAC3 = (int32_t)val; break;
    case 28: IR1 = (int32_t)((val & 0x1F) << 7); IR2 = (int32_t)(((val >> 5) & 0x1F) << 7); IR3 = (int32_t)(((val >> 10) & 0x1F) << 7); break;
    case 30: {
        LZCS = val;
        uint32_t test = (val & 0x80000000u) ? ~val : val;
        LZCR = test == 0 ? 32u : (uint32_t)__builtin_clz(test);
        break;
    }
    default: break;
    }
}

uint32_t gte_read_control(int reg)
{
    switch (reg & 31) {
    case 0: return pack(RT[0], RT[1]);
    case 1: return pack(RT[2], RT[3]);
    case 2: return pack(RT[4], RT[5]);
    case 3: return pack(RT[6], RT[7]);
    case 4: return (uint32_t)(int32_t)RT[8];
    case 5: return (uint32_t)TR[0];
    case 6: return (uint32_t)TR[1];
    case 7: return (uint32_t)TR[2];
    case 8: return pack(LLM[0], LLM[1]);
    case 9: return pack(LLM[2], LLM[3]);
    case 10: return pack(LLM[4], LLM[5]);
    case 11: return pack(LLM[6], LLM[7]);
    case 12: return (uint32_t)(int32_t)LLM[8];
    case 13: return (uint32_t)BK[0];
    case 14: return (uint32_t)BK[1];
    case 15: return (uint32_t)BK[2];
    case 16: return pack(LCM[0], LCM[1]);
    case 17: return pack(LCM[2], LCM[3]);
    case 18: return pack(LCM[4], LCM[5]);
    case 19: return pack(LCM[6], LCM[7]);
    case 20: return (uint32_t)(int32_t)LCM[8];
    case 21: return (uint32_t)FC[0];
    case 22: return (uint32_t)FC[1];
    case 23: return (uint32_t)FC[2];
    case 24: return (uint32_t)OFX;
    case 25: return (uint32_t)OFY;
    case 26: return (uint32_t)(int32_t)(int16_t)H;
    case 27: return (uint32_t)(int32_t)DQA;
    case 28: return (uint32_t)DQB;
    case 29: return (uint32_t)(int32_t)ZSF3;
    case 30: return (uint32_t)(int32_t)ZSF4;
    default: return FLAG;
    }
}

void gte_write_control(int reg, uint32_t val)
{
    switch (reg & 31) {
    case 0: RT[0] = (int16_t)val; RT[1] = (int16_t)(val >> 16); break;
    case 1: RT[2] = (int16_t)val; RT[3] = (int16_t)(val >> 16); break;
    case 2: RT[4] = (int16_t)val; RT[5] = (int16_t)(val >> 16); break;
    case 3: RT[6] = (int16_t)val; RT[7] = (int16_t)(val >> 16); break;
    case 4: RT[8] = (int16_t)val; break;
    case 5: TR[0] = (int32_t)val; break;
    case 6: TR[1] = (int32_t)val; break;
    case 7: TR[2] = (int32_t)val; break;
    case 8: LLM[0] = (int16_t)val; LLM[1] = (int16_t)(val >> 16); break;
    case 9: LLM[2] = (int16_t)val; LLM[3] = (int16_t)(val >> 16); break;
    case 10: LLM[4] = (int16_t)val; LLM[5] = (int16_t)(val >> 16); break;
    case 11: LLM[6] = (int16_t)val; LLM[7] = (int16_t)(val >> 16); break;
    case 12: LLM[8] = (int16_t)val; break;
    case 13: BK[0] = (int32_t)val; break;
    case 14: BK[1] = (int32_t)val; break;
    case 15: BK[2] = (int32_t)val; break;
    case 16: LCM[0] = (int16_t)val; LCM[1] = (int16_t)(val >> 16); break;
    case 17: LCM[2] = (int16_t)val; LCM[3] = (int16_t)(val >> 16); break;
    case 18: LCM[4] = (int16_t)val; LCM[5] = (int16_t)(val >> 16); break;
    case 19: LCM[6] = (int16_t)val; LCM[7] = (int16_t)(val >> 16); break;
    case 20: LCM[8] = (int16_t)val; break;
    case 21: FC[0] = (int32_t)val; break;
    case 22: FC[1] = (int32_t)val; break;
    case 23: FC[2] = (int32_t)val; break;
    case 24: OFX = (int32_t)val; break;
    case 25: OFY = (int32_t)val; break;
    case 26: H = (uint16_t)val; break;
    case 27: DQA = (int16_t)val; break;
    case 28: DQB = (int32_t)val; break;
    case 29: ZSF3 = (int16_t)val; break;
    case 30: ZSF4 = (int16_t)val; break;
    default: FLAG = val & 0x7FFFF000u; if (FLAG & 0x7F87E000u) FLAG |= 0x80000000u; break;
    }
}

/* The PC's Gte.GetCondition: the BC2F/BC2T condition is always false. */
int gte_condition(void) { return 0; }
