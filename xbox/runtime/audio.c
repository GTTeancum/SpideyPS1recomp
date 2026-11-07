/*
 * The AC97 sound output (see audio.h).
 *
 * nxdk's XAudio (hal/audio.h) feeds the chip a list of 32 buffer descriptors. Up to
 * sixteen buffers of 1024 frames (about 340 ms) are kept queued: buffer i of the ring
 * is the one descriptor i mod 16 plays. Where the chip is, read from its registers, is
 * the played position (a movie's clock). Sound is resampled linearly to 48 kHz.
 */
#include <string.h>
#include <hal/audio.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include "audio.h"
#include "runtime.h"

#define OUT_RATE 48000
#define CHUNK 1024
#define BUFFERS 16
#define AC97 ((volatile uint8_t *)0xFEC00000u)
#define AC97_CIV 0x114        /* descriptor playing */
#define AC97_SR 0x116         /* bit 0: halted */
#define AC97_PICB 0x118       /* samples left in it */

extern AC97_DEVICE ac97Device;      /* nxdk's, hal/audio.c */

static int16_t *g_pcm;              /* BUFFERS x CHUNK stereo frames, contiguous */
static uint32_t g_desc;             /* buffers handed to the chip since the reset */
static int g_fill;                  /* frames in the buffer being filled */
static int g_started, g_prebuffer;
static unsigned g_owner, g_tokens;
static uint32_t g_step, g_phase;    /* 16.16: input frames per output frame, position */
static int16_t g_hist[2];           /* the last input frame of the previous write */

/* Stop and forget everything queued (the chip's own reset, as XAudioInit does it). */
static void ac97_reset(void)
{
    AC97[0x11B] = 0x1E;
    while (AC97[0x11B] & 2) {}
    AC97[0x17B] = 0x1E;
    while (AC97[0x17B] & 2) {}
    AC97[0x116] = 0xFF;
    AC97[0x176] = 0xFF;
    for (int i = 0; i < 32; i++) {
        ac97Device.pcmOutDescriptor[i].bufferStartAddress = 0;
        ac97Device.pcmOutDescriptor[i].bufferLengthInSamples = 0;
        ac97Device.pcmOutDescriptor[i].bufferControl = 0;
        ac97Device.pcmSpdifDescriptor[i].bufferStartAddress = 0;
        ac97Device.pcmSpdifDescriptor[i].bufferLengthInSamples = 0;
        ac97Device.pcmSpdifDescriptor[i].bufferControl = 0;
    }
    ac97Device.mmio[0x110 >> 2] = MmGetPhysicalAddress((void *)&ac97Device.pcmOutDescriptor[0]);
    ac97Device.mmio[0x170 >> 2] = MmGetPhysicalAddress((void *)&ac97Device.pcmSpdifDescriptor[0]);
    ac97Device.nextDescriptor = 0;
    XAudioPause();
    g_desc = 0;
    g_fill = 0;
    g_started = 0;
}

/* Buffers handed over and not played to the end. Before it starts, the chip reads as
 * halted; once started, halted means it ran out. */
static int pending(void)
{
    if (!g_started) return (int)g_desc;
    if (!g_desc || (*(volatile uint16_t *)(AC97 + AC97_SR) & 1)) return 0;
    int n = ((int)(g_desc & 31u) - (AC97[AC97_CIV] & 31)) & 31;
    return n > BUFFERS ? BUFFERS : n;
}

static uint32_t frames_left(void)
{
    int p = pending();
    if (!p) return 0;
    if (!g_started) return (uint32_t)p * CHUNK;
    return *(volatile uint16_t *)(AC97 + AC97_PICB) / 2u + (uint32_t)(p - 1) * CHUNK;
}

static void submit(void)
{
    XAudioProvideSamples((unsigned char *)(g_pcm + (g_desc % BUFFERS) * CHUNK * 2), CHUNK * 4, 0);
    g_desc++;
    g_fill = 0;
    /* Start once the prebuffer is queued; restart after running dry. */
    if ((!g_started && g_desc >= (uint32_t)g_prebuffer) || (g_started && (*(volatile uint16_t *)(AC97 + AC97_SR) & 1))) {
        g_started = 1;
        XAudioPlay();
    }
}

unsigned audio_open(int rate, int prebuffer)
{
    static int initialised;
    if (rate < 8000 || rate > 48000) return 0;
    if (!initialised) {
        g_pcm = MmAllocateContiguousMemoryEx(BUFFERS * CHUNK * 4, 0, 0x03ffafff, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
        if (!g_pcm) return 0;
        XAudioInit(16, 2, 0, 0);
        initialised = 1;
    }
    ac97_reset();
    g_step = (uint32_t)rate * 65536u / OUT_RATE;
    g_phase = 0;
    g_hist[0] = g_hist[1] = 0;
    g_prebuffer = prebuffer < 1 ? 1 : prebuffer > BUFFERS ? BUFFERS : prebuffer;
    g_owner = ++g_tokens ? g_tokens : ++g_tokens;
    return g_owner;
}

int audio_owned(unsigned token) { return token && token == g_owner; }

void audio_close(unsigned token)
{
    if (!audio_owned(token)) return;
    ac97_reset();
    g_owner = 0;
}

int audio_room(void)
{
    if (!g_owner) return 0;
    int free_buffers = BUFFERS - pending();
    if (free_buffers <= 0) return 0;
    int64_t out = (int64_t)free_buffers * CHUNK - g_fill;
    int64_t in = (out * g_step >> 16) - 2;
    return in > 0 ? (int)in : 0;
}

void audio_write(const int16_t *pcm, int frames)
{
    if (frames <= 0 || !g_owner) return;
    /* The buffer being filled is ring entry g_desc mod BUFFERS: free unless all
     * BUFFERS are queued. The chip only ever lowers the count, so it is read once. */
    int queued = pending();
    /* Input frame j is g_hist for j = 0, else pcm[j - 1]. */
    for (;;) {
        uint32_t i = g_phase >> 16;
        if ((int)i + 1 > frames) break;
        if (g_fill == CHUNK && queued < BUFFERS) { submit(); queued++; }
        if (g_fill == CHUNK || queued >= BUFFERS) { rt_note("sound overrun"); g_phase += g_step; continue; }
        int32_t f = (int32_t)((g_phase & 0xFFFFu) >> 1);     /* 15 bits: the products stay in range */
        const int16_t *a = i ? pcm + (i - 1) * 2 : g_hist, *b = pcm + i * 2;
        int16_t *out = g_pcm + ((g_desc % BUFFERS) * CHUNK + (uint32_t)g_fill) * 2;
        out[0] = (int16_t)(a[0] + (((b[0] - a[0]) * f) >> 15));
        out[1] = (int16_t)(a[1] + (((b[1] - a[1]) * f) >> 15));
        g_fill++;
        g_phase += g_step;
    }
    if (g_fill == CHUNK && queued < BUFFERS) submit();
    g_phase -= (uint32_t)frames << 16;
    g_hist[0] = pcm[(frames - 1) * 2];
    g_hist[1] = pcm[(frames - 1) * 2 + 1];
}

void audio_flush(void)
{
    if (!g_owner) return;
    if (g_fill > 0 && pending() < BUFFERS) {
        memset(g_pcm + ((g_desc % BUFFERS) * CHUNK + (uint32_t)g_fill) * 2, 0, (size_t)(CHUNK - g_fill) * 4);
        submit();
    }
    if (!g_started && g_desc) { g_started = 1; XAudioPlay(); }
}

double audio_seconds(void) { return (double)((uint64_t)g_desc * CHUNK - frames_left()) / OUT_RATE; }
int audio_busy(void) { return frames_left() > 0; }
