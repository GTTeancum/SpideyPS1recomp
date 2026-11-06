/*
 * PlayStation .STR movies (see str.h). The video decode follows the PC's MDEC
 * (tools/RecompOne/RecompOne.Runtime/Hardware/Mdec.cs: dequantisation, the IDCT with
 * the hardware's scale table, the colour conversion), fed straight from the frame's
 * bitstream rather than through the game's DecDCTvlc and DMA: the movie is played by
 * the runtime, as the PC plays Dreamcast movies. The bitstream's AC codes are MPEG-1's,
 * read with the shared PL_MPEG table (tools/movies/native/mpeg1_vlc.h).
 */
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "str.h"

typedef struct Vlc Vlc;
#include "../../tools/movies/native/mpeg1_vlc.h"

#define SECTOR 2336                  /* subheader (8) + 2328 */
#define READ_SECTORS 32              /* one disk read */
#define CHUNK_DATA 2016              /* video bytes a sector carries, after its 32-byte header */
#define CHUNKS_MAX 32
#define FRAME_MAX (CHUNKS_MAX * CHUNK_DATA)
#define FRAMES 12                    /* compressed frames read ahead */
#define AUDIO_FRAMES 65536u          /* stereo frames of decoded sound queued, a power of two */
#define SECTORS_PER_SECOND 150.0     /* double-speed CD */

typedef struct { uint8_t *data; uint32_t size, sector; int width, height; } Frame;

struct StrMovie {
    HANDLE file;
    uint32_t sectors, sector;                 /* in the file; the next to read */
    uint8_t *buf;                             /* READ_SECTORS sectors */
    uint32_t buf_first, buf_count;
    int eof;
    int width, height;
    /* the frame being assembled from its sectors */
    Frame build;
    uint32_t build_frame, build_need, build_mask;
    int building;
    Frame queue[FRAMES];
    int queue_head, queue_count;
    /* sound */
    int rate, stereo;
    uint8_t audio_file, audio_channel;
    double audio_start;
    int32_t history[2][2];
    int16_t *ring;                            /* AUDIO_FRAMES stereo frames */
    uint32_t ring_read, ring_write;
};

static uint32_t u16le(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t u32le(const uint8_t *p) { return u16le(p) | u16le(p + 2) << 16; }

/* ---- sectors ------------------------------------------------------------------------------ */
static const uint8_t *next_sector(StrMovie *m)
{
    if (m->sector >= m->sectors) return 0;
    if (m->sector < m->buf_first || m->sector >= m->buf_first + m->buf_count) {
        uint32_t want = m->sectors - m->sector;
        if (want > READ_SECTORS) want = READ_SECTORS;
        LARGE_INTEGER at;
        at.QuadPart = (LONGLONG)m->sector * SECTOR;
        DWORD got = 0;
        if (!SetFilePointerEx(m->file, at, 0, FILE_BEGIN) || !ReadFile(m->file, m->buf, want * SECTOR, &got, 0)) got = 0;
        m->buf_first = m->sector;
        m->buf_count = got / SECTOR;
        if (!m->buf_count) return 0;
    }
    return m->buf + (m->sector++ - m->buf_first) * SECTOR;
}

static int is_video(const uint8_t *s)
{
    const uint8_t *d = s + 8;
    return !(s[2] & 0x04) && d[0] == 0x60 && d[1] == 0x01 && d[2] == 0x01 && d[3] == 0x80;
}

/* ---- XA ADPCM -------------------------------------------------------------------------------- */
static void audio_put(StrMovie *m, int16_t left, int16_t right)
{
    if (m->ring_write - m->ring_read == AUDIO_FRAMES) m->ring_read++;   /* full: lose the oldest */
    uint32_t i = (m->ring_write++ & (AUDIO_FRAMES - 1u)) * 2u;
    m->ring[i] = left;
    m->ring[i + 1] = right;
}

/* 18 sound groups of 128 bytes: 16 bytes of parameters, then 28 words carrying one
 * 4-bit sample of each of the 8 units. Stereo units alternate left, right. */
static void audio_sector(StrMovie *m, const uint8_t *s)
{
    static const int pos[5] = { 0, 60, 115, 98, 122 }, neg[5] = { 0, 0, -52, -55, -60 };
    if (s[0] != m->audio_file || s[1] != m->audio_channel || (s[3] & 0x30)) return;
    const uint8_t *d = s + 8;
    for (int g = 0; g < 18; g++) {
        const uint8_t *group = d + g * 128;
        int16_t out[8][28];
        for (int u = 0; u < 8; u++) {
            int param = group[4 + u], shift = param & 15, filter = (param >> 4) & 3;
            if (shift > 12) shift = 9;
            int ch = m->stereo ? (u & 1) : 0;
            int32_t *h = m->history[ch];
            for (int i = 0; i < 28; i++) {
                int byte = group[16 + i * 4 + (u >> 1)];
                int nibble = (u & 1) ? byte >> 4 : byte & 15;
                int32_t sample = (int32_t)(int16_t)(uint16_t)(nibble << 12) >> shift;
                sample += (h[0] * pos[filter] + h[1] * neg[filter] + 32) >> 6;
                if (sample > 32767) sample = 32767;
                if (sample < -32768) sample = -32768;
                h[1] = h[0];
                h[0] = sample;
                out[u][i] = (int16_t)sample;
            }
        }
        if (m->stereo)
            for (int u = 0; u < 8; u += 2)
                for (int i = 0; i < 28; i++) audio_put(m, out[u][i], out[u + 1][i]);
        else
            for (int u = 0; u < 8; u++)
                for (int i = 0; i < 28; i++) audio_put(m, out[u][i], out[u][i]);
    }
}

/* ---- demultiplexing ----------------------------------------------------------------------- */
static void push_frame(StrMovie *m)
{
    if (m->queue_count == FRAMES) return;
    Frame *slot = &m->queue[(m->queue_head + m->queue_count++) % FRAMES];
    uint8_t *spare = slot->data;
    *slot = m->build;
    m->build.data = spare;
}

static void video_sector(StrMovie *m, const uint8_t *s, uint32_t index)
{
    const uint8_t *d = s + 8;
    uint32_t chunk = u16le(d + 4), count = u16le(d + 6), frame = u32le(d + 8), size = u32le(d + 12);
    if (!count || count > CHUNKS_MAX || chunk >= count || size > count * CHUNK_DATA) return;
    if (!m->building || frame != m->build_frame) {
        /* A new frame: one left incomplete (a sector missing) is dropped. */
        m->building = 1;
        m->build_frame = frame;
        m->build_need = count;
        m->build_mask = 0;
        m->build.size = size;
        m->build.sector = index - chunk;
        m->build.width = (int)u16le(d + 16);
        m->build.height = (int)u16le(d + 18);
    }
    memcpy(m->build.data + chunk * CHUNK_DATA, d + 32, CHUNK_DATA);
    m->build_mask |= 1u << chunk;
    if ((uint32_t)__builtin_popcount(m->build_mask) == m->build_need) {
        push_frame(m);
        m->building = 0;
    }
}

/* Read one sector; 0 at the end of the file. */
static int demux_one(StrMovie *m)
{
    uint32_t index = m->sector;
    const uint8_t *s = next_sector(m);
    if (!s) { m->eof = 1; return 0; }
    if (s[2] & 0x04) { if (m->rate) audio_sector(m, s); }
    else if (is_video(s)) video_sector(m, s, index);
    return 1;
}

void str_read_ahead(StrMovie *m, double seconds)
{
    uint32_t want = m->rate ? (uint32_t)(seconds * m->rate) : 0;
    while (!m->eof && m->queue_count < FRAMES) {
        if (m->rate ? (uint32_t)(m->ring_write - m->ring_read) >= want : m->queue_count >= 3) break;
        if (!demux_one(m)) break;
    }
}

int str_audio_queued(const StrMovie *m) { return (int)(m->ring_write - m->ring_read); }

int str_audio_read(StrMovie *m, int16_t *pcm, int frames)
{
    int n = 0;
    for (; n < frames && m->ring_read != m->ring_write; n++) {
        uint32_t i = (m->ring_read++ & (AUDIO_FRAMES - 1u)) * 2u;
        pcm[n * 2] = m->ring[i];
        pcm[n * 2 + 1] = m->ring[i + 1];
    }
    return n;
}

int str_ended(const StrMovie *m) { return m->eof && m->queue_count == 0; }

/* ---- the bitstream ------------------------------------------------------------------------ */
/* 16-bit little-endian words, each read from its top bit. Past the end reads zeros. */
typedef struct { const uint8_t *p, *end; uint32_t bits; int count; } Bits;

static void refill(Bits *b)
{
    while (b->count <= 16) {
        uint32_t w = 0;
        if (b->p + 1 < b->end) { w = u16le(b->p); b->p += 2; }
        b->bits |= w << (16 - b->count);
        b->count += 16;
    }
}

static uint32_t get(Bits *b, int n)
{
    refill(b);
    uint32_t v = b->bits >> (32 - n);
    b->bits <<= n;
    b->count -= n;
    return v;
}

static int coeff(Bits *b)
{
    int node = 0;
    for (int depth = 0; depth < 32; depth++) {
        int at = node + (int)get(b, 1);
        if (at < 0 || at >= (int)(sizeof Coeff / sizeof Coeff[0])) return -1;
        if (Coeff[at].next == 0) return Coeff[at].value;
        if (Coeff[at].next < 0) return -1;
        node = Coeff[at].next;
    }
    return -1;
}

/* ---- MDEC ----------------------------------------------------------------------------------- */
static const uint8_t Zigzag[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

/* The quantisation table the PlayStation's libpress loads (natural order): MPEG-1's
 * intra matrix with a DC step of 2. */
static const uint8_t Quant[64] = {
    2,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38, 22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83,
};

/* The MDEC's default scale table (Mdec.cs DefaultScale), divided by 8 as it uses it. */
static const int16_t Scale[64] = {
    0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82,
    0x7D8A, 0x6A6D, 0x471C, 0x18F8, -0x18F9, -0x471D, -0x6A6E, -0x7D8B,
    0x7641, 0x30FB, -0x30FC, -0x7642, -0x7642, -0x30FC, 0x30FB, 0x7641,
    0x6A6D, -0x18F9, -0x7D8B, -0x471D, 0x471C, 0x7D8A, 0x18F8, -0x6A6E,
    0x5A82, -0x5A83, -0x5A83, 0x5A82, 0x5A82, -0x5A83, -0x5A83, 0x5A82,
    0x471C, -0x7D8B, 0x18F8, 0x6A6D, -0x6A6E, -0x18F9, 0x7D8A, -0x471D,
    0x30FB, -0x7642, 0x7641, -0x30FC, -0x30FC, 0x7641, -0x7642, 0x30FB,
    0x18F8, -0x471D, 0x6A6D, -0x7D8B, 0x7D8A, -0x6A6E, 0x471C, -0x18F9,
};

static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

static void idct(int32_t *block)
{
    int32_t tmp[64];
    for (int pass = 0; pass < 2; pass++) {
        for (int x = 0; x < 8; x++)
            for (int y = 0; y < 8; y++) {
                int32_t sum = 0;
                for (int z = 0; z < 8; z++) sum += block[y + z * 8] * (Scale[x + z * 8] / 8);
                tmp[x + y * 8] = (sum + 0xFFF) / 0x2000;
            }
        memcpy(block, tmp, sizeof tmp);
    }
}

/* One 8x8 block: a 10-bit DC, then run/level codes up to the end-of-block code "10". */
static int block(Bits *b, int qscale, int32_t *out)
{
    memset(out, 0, 64 * sizeof *out);
    int32_t dc = (int32_t)get(b, 10);
    if (dc & 0x200) dc -= 0x400;
    out[0] = clamp(dc * Quant[0], -0x400, 0x3FF);
    int k = 0;
    for (int steps = 0; steps < 64; steps++) {
        int code = coeff(b), run, level;
        if (code < 0) return 0;
        if (code == 1) {
            if (!get(b, 1)) { idct(out); return 1; }
            run = 0;
            level = get(b, 1) ? -1 : 1;
        } else if (code == 65535) {
            run = (int)get(b, 6);
            level = (int)get(b, 10);
            if (level & 0x200) level -= 0x400;
        } else {
            run = code >> 8;
            level = code & 255;
            if (get(b, 1)) level = -level;
        }
        k += run + 1;
        if (k > 63) return 0;
        int32_t v = qscale ? (level * Quant[Zigzag[k]] * qscale + 4) / 8 : level * 2;
        out[Zigzag[k]] = clamp(v, -0x400, 0x3FF);
    }
    return 0;
}

/* Cr, Cb, then the four 8x8 luma blocks, into the 16x16 at (x0, y0). */
static int macroblock(Bits *b, int qscale, uint32_t *argb, unsigned pitch, int x0, int y0, int w, int h)
{
    int32_t cr[64], cb[64], y[64];
    if (!block(b, qscale, cr) || !block(b, qscale, cb)) return 0;
    for (int q = 0; q < 4; q++) {
        if (!block(b, qscale, y)) return 0;
        int qx = (q & 1) * 8, qy = (q >> 1) * 8;
        for (int py = 0; py < 8; py++) {
            int Y = y0 + qy + py;
            if (Y >= h) break;
            uint32_t *row = (uint32_t *)((uint8_t *)argb + (unsigned)Y * pitch);
            for (int px = 0; px < 8; px++) {
                int X = x0 + qx + px;
                if (X >= w) break;
                int c = (qx + px) / 2 + ((qy + py) / 2) * 8;
                int32_t l = y[px + py * 8], vr = cr[c], vb = cb[c];
                int32_t r = clamp(l + ((vr * 1436 + 512) >> 10), -128, 127) + 128;
                int32_t g = clamp(l + ((-vb * 352 - vr * 731 + 512) >> 10), -128, 127) + 128;
                int32_t bl = clamp(l + ((vb * 1815 + 512) >> 10), -128, 127) + 128;
                row[X] = 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)bl;
            }
        }
    }
    return 1;
}

/* Header: run-length code count, 0x3800, quantiser scale, version. Macroblocks run
 * down each 16-pixel column, then across. */
static int decode(const Frame *f, uint32_t *argb, unsigned pitch, int w, int h)
{
    if (f->size < 8 || u16le(f->data + 2) != 0x3800) return 0;
    int qscale = (int)u16le(f->data + 4), version = (int)u16le(f->data + 6);
    if (version != 1 && version != 2) return 0;
    Bits b = { f->data + 8, f->data + f->size, 0, 0 };
    for (int mx = 0; mx < (w + 15) / 16; mx++)
        for (int my = 0; my < (h + 15) / 16; my++)
            if (!macroblock(&b, qscale, argb, pitch, mx * 16, my * 16, w, h)) return 0;
    return 1;
}

int str_next_frame(StrMovie *m, uint32_t *argb, unsigned pitch, double *time)
{
    while (!m->queue_count && demux_one(m)) {}
    if (!m->queue_count) return 0;
    Frame *f = &m->queue[m->queue_head];
    m->queue_head = (m->queue_head + 1) % FRAMES;
    m->queue_count--;
    *time = f->sector / SECTORS_PER_SECOND;
    int w = f->width < m->width ? f->width : m->width, h = f->height < m->height ? f->height : m->height;
    return decode(f, argb, pitch, w, h) ? 1 : -1;
}

/* ---- opening -------------------------------------------------------------------------------- */
int str_width(const StrMovie *m) { return m->width; }
int str_height(const StrMovie *m) { return m->height; }
int str_audio_rate(const StrMovie *m) { return m->rate; }
double str_audio_start(const StrMovie *m) { return m->audio_start; }

void str_close(StrMovie *m)
{
    if (!m) return;
    if (m->file != INVALID_HANDLE_VALUE) CloseHandle(m->file);
    free(m->build.data);
    for (int i = 0; i < FRAMES; i++) free(m->queue[i].data);
    free(m->ring);
    free(m->buf);
    free(m);
}

StrMovie *str_open(const char *path)
{
    StrMovie *m = calloc(1, sizeof *m);
    if (!m) return 0;
    m->file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
    LARGE_INTEGER size;
    if (m->file == INVALID_HANDLE_VALUE || !GetFileSizeEx(m->file, &size) || size.QuadPart % SECTOR) { str_close(m); return 0; }
    m->sectors = (uint32_t)(size.QuadPart / SECTOR);
    m->buf = malloc(READ_SECTORS * SECTOR);
    m->ring = malloc(AUDIO_FRAMES * 4u);
    m->build.data = malloc(FRAME_MAX);
    int ok = m->buf && m->ring && m->build.data;
    for (int i = 0; i < FRAMES; i++) ok &= (m->queue[i].data = malloc(FRAME_MAX)) != 0;
    if (!ok) { str_close(m); return 0; }
    /* The first sectors give the picture size and the sound's channel and rate. */
    int found_video = 0, found_audio = 0;
    for (uint32_t i = 0; i < 256 && !(found_video && found_audio); i++) {
        const uint8_t *s = next_sector(m);
        if (!s) break;
        if (!found_video && is_video(s)) {
            m->width = (int)u16le(s + 8 + 16);
            m->height = (int)u16le(s + 8 + 18);
            found_video = m->width > 0 && m->width <= 640 && m->height > 0 && m->height <= 512;
        } else if (!found_audio && (s[2] & 0x04) && !(s[3] & 0x30)) {
            m->audio_file = s[0];
            m->audio_channel = s[1];
            m->stereo = s[3] & 1;
            m->rate = (s[3] & 0x04) ? 18900 : 37800;
            m->audio_start = i / SECTORS_PER_SECOND;
            found_audio = 1;
        }
    }
    if (!found_video) { str_close(m); return 0; }
    m->sector = 0;
    return m;
}
