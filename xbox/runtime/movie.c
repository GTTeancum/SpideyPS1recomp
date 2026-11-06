/*
 * Movie playback (see movie.h): the AC97 sound output, the clock a movie runs to, the
 * skip button, and the two players' loops. Follows the PC's DreamcastMovies.TryPlay:
 * the first picture is decoded before anything is taken over; the sound's played
 * position, not the time queued, picks the picture; late pictures are decoded and
 * dropped a few at a time so the console keeps ticking.
 */
#include <stdlib.h>
#include <string.h>
#include <hal/audio.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include "hwgpu.h"
#include "movie.h"
#include "runtime.h"
#include "str.h"

/* ---- sound: the AC97 at 48 kHz ---------------------------------------------------------------- */
/*
 * nxdk's XAudio (hal/audio.h) feeds the chip a list of 32 buffer descriptors. Sixteen
 * buffers of 1024 frames (about 340 ms) are kept queued: buffer i of the ring is the
 * one descriptor i mod 16 plays. Where the chip is, read from its registers, is the
 * movie's clock. Sound is resampled linearly from the movie's rate. (Eight buffers ran
 * dry in XEMU while a late stretch of pictures was decoded.)
 */
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
static int g_started, g_open;
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
    /* Start with two queued; restart after running dry. */
    if ((!g_started && g_desc >= 2) || (g_started && (*(volatile uint16_t *)(AC97 + AC97_SR) & 1))) {
        g_started = 1;
        XAudioPlay();
    }
}

static int audio_open(int rate)
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
    g_open = 1;
    return 1;
}

static void audio_close(void)
{
    if (!g_open) return;
    ac97_reset();
    g_open = 0;
}

/* Input frames the queue can take now. */
static int audio_room(void)
{
    int free_buffers = BUFFERS - pending();
    if (free_buffers <= 0) return 0;
    int64_t out = (int64_t)free_buffers * CHUNK - g_fill;
    int64_t in = (out * g_step >> 16) - 2;
    return in > 0 ? (int)in : 0;
}

static void audio_write(const int16_t *pcm, int frames)
{
    if (frames <= 0) return;
    /* The buffer being filled is ring entry g_desc mod BUFFERS: free unless all
     * BUFFERS are queued. The chip only ever lowers the count, so it is read once. */
    int queued = pending();
    /* Input frame j is g_hist for j = 0, else pcm[j - 1]. */
    for (;;) {
        uint32_t i = g_phase >> 16;
        if ((int)i + 1 > frames) break;
        if (g_fill == CHUNK && queued < BUFFERS) { submit(); queued++; }
        if (g_fill == CHUNK || queued >= BUFFERS) { rt_note("movie sound overrun"); g_phase += g_step; continue; }
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

/* The end of the sound: the part-filled buffer goes out padded with silence. */
static void audio_flush(void)
{
    if (g_fill > 0 && pending() < BUFFERS) {
        memset(g_pcm + ((g_desc % BUFFERS) * CHUNK + (uint32_t)g_fill) * 2, 0, (size_t)(CHUNK - g_fill) * 4);
        submit();
    }
    if (!g_started && g_desc) { g_started = 1; XAudioPlay(); }
}

static double audio_seconds(void) { return (double)((uint64_t)g_desc * CHUNK - frames_left()) / OUT_RATE; }
static int audio_busy(void) { return frames_left() > 0; }

/* ---- a session: the clock and the skip button ---------------------------------------------- */
typedef struct {
    int audio;                       /* the sound output is the clock */
    LARGE_INTEGER start, hz;
    double last_audio, last_change;  /* the sound position, and when it last moved */
    double offset;                   /* added to the timer once the sound no longer leads */
    int released, previous;
} Session;

/* Start or Cross down (pads are active-low). */
static int skip_pressed(void) { return (rt_pad_state() & 0x4008u) != 0x4008u; }

static double elapsed(const Session *s)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - s->start.QuadPart) / (double)s->hz.QuadPart;
}

static void session_begin(Session *s, int rate)
{
    memset(s, 0, sizeof *s);
    s->audio = rate > 0 && audio_open(rate);
    QueryPerformanceFrequency(&s->hz);
    QueryPerformanceCounter(&s->start);
    int p = skip_pressed();
    s->previous = p;
    s->released = !p;
    rt_movie_playing = 1;
    cd_hold(1);
}

static void session_end(Session *s)
{
    if (s->audio) audio_close();
    s->audio = 0;
    cd_hold(0);
    rt_movie_playing = 0;
}

/* Seconds into the movie: the sound played while it plays; after it has all played,
 * the timer onward from there. Sound that stops moving with buffers queued (a device
 * that does not play) is given up after 1.5 seconds and the timer used instead. */
static double session_clock(Session *s)
{
    double now = elapsed(s);
    if (s->audio) {
        double a = audio_seconds();
        if (a != s->last_audio) { s->last_audio = a; s->last_change = now; }
        if (audio_busy()) {
            if (now - s->last_change <= 1.5) return a;
            rt_log("movie: sound stalled, timed by the clock");
            audio_close();
            s->audio = 0;
            s->offset = a - now;
            return a;
        }
        return a + (now - s->last_change);
    }
    return now + s->offset;
}

static int session_skip(Session *s, double seconds)
{
    int p = skip_pressed();
    if (!p) s->released = 1;
    int skip = seconds >= 1.0 && s->released && p && !s->previous;
    s->previous = p;
    return skip;
}

/* ---- PlayStation .STR -------------------------------------------------------------------- */
/* Top up the sound queue from what has been read; called between pictures too. */
static void str_pump(StrMovie *m, const Session *s, int *flushed)
{
    static int16_t pcm[2048 * 2];
    if (!s->audio) return;
    for (int room = audio_room(); room > 0;) {
        int n = str_audio_read(m, pcm, room < 2048 ? room : 2048);
        if (!n) break;
        audio_write(pcm, n);
        room -= n;
    }
    if (!*flushed && str_ended(m) && !str_audio_queued(m)) { audio_flush(); *flushed = 1; }
}

int movie_play_str(const char *path)
{
    StrMovie *m = str_open(path);
    if (!m) return 0;
    int w = str_width(m), h = str_height(m);
    if (!hw_picture_begin(w, h)) { str_close(m); return 0; }
    /* PlayStation movie pixels are square at 320 wide: 320x240 is 4:3. */
    float aspect = (float)w / (float)h;
    unsigned pitch;
    double due, last = 0;
    uint32_t *pixels = hw_picture_next(&pitch);
    str_read_ahead(m, 0.5);
    if (str_next_frame(m, pixels, pitch, &due) != 1) {
        rt_log("movie %s: no first picture", path);
        hw_picture_end();
        str_close(m);
        return 0;
    }
    Session s;
    session_begin(&s, str_audio_rate(m));
    rt_log("movie %s: PS1 %dx%d, sound %d Hz%s", path, w, h, str_audio_rate(m), s.audio ? "" : " (silent)");
    int waiting = 1, video_done = 0, flushed = 0, ok = 0, shown = 0;
    double start = s.audio ? str_audio_start(m) : 0.0;
    for (;;) {
        str_read_ahead(m, 0.5);
        str_pump(m, &s, &flushed);
        double t = session_clock(&s) + start;
        if (session_skip(&s, t)) { rt_log("movie: skipped"); ok = 1; break; }
        int failed = 0;
        for (int work = 0; work < 4; work++) {
            if (waiting) {
                if (t < due) break;
                hw_picture_show(aspect);
                last = due;
                waiting = 0;
                shown++;
            }
            if (video_done) break;
            if (work) str_pump(m, &s, &flushed);
            pixels = hw_picture_next(&pitch);
            int r = str_next_frame(m, pixels, pitch, &due);
            if (r == 1) waiting = 1;
            else if (r == 0) video_done = 1;
            else { failed = 1; break; }
        }
        if (failed) { rt_log("movie %s: undecodable picture after %d", path, shown); ok = shown > 0; break; }
        if (video_done && !waiting && t >= last + 1.0 / 15.0 && (!s.audio || (flushed && !audio_busy()))) { ok = 1; break; }
        rt_tick();
    }
    session_end(&s);
    hw_picture_end();
    str_close(m);
    return ok;
}

/* ---- Dreamcast .SFD (the PC's decoder) ------------------------------------------------------ */
typedef struct {
    unsigned width, height, fps_num, fps_den, sar_num, sar_den;
    unsigned sample_rate, channels, audio_frames, reserved;
} SfdInfo;
typedef int (*SfdReadAt)(void *user, unsigned long long offset, unsigned char *destination, unsigned length);
unsigned sfd_workspace_size(void);
int sfd_open(void *workspace, unsigned bytes, SfdReadAt read, void *user, unsigned long long file_size, SfdInfo *info);
int sfd_video(void *workspace, unsigned char *yuv, unsigned capacity, unsigned *frame_index);
int sfd_audio(void *workspace, short *pcm, unsigned capacity_frames);
void sfd_close(void *workspace);

static int sfd_read(void *user, unsigned long long offset, unsigned char *destination, unsigned length)
{
    LARGE_INTEGER at;
    at.QuadPart = (LONGLONG)offset;
    DWORD got = 0;
    if (!SetFilePointerEx((HANDLE)user, at, 0, FILE_BEGIN) || !ReadFile((HANDLE)user, destination, length, &got, 0)) return -1;
    return (int)got;
}

/* Tight YUV 4:2:0 to the picture, BT.601 limited range (the decoder's sfd_rgba). */
static void yuv_to_argb(const uint8_t *yuv, int w, int h, uint32_t *argb, unsigned pitch)
{
    const uint8_t *u = yuv + w * h, *v = u + (w / 2) * (h / 2);
    for (int row = 0; row < h; row++) {
        uint32_t *out = (uint32_t *)((uint8_t *)argb + (unsigned)row * pitch);
        const uint8_t *y = yuv + row * w, *ur = u + (row / 2) * (w / 2), *vr = v + (row / 2) * (w / 2);
        for (int col = 0; col < w; col++) {
            int yy = 298 * (y[col] - 16), uu = ur[col / 2] - 128, vv = vr[col / 2] - 128;
            int r = (yy + 409 * vv + 128) >> 8, g = (yy - 100 * uu - 208 * vv + 128) >> 8, b = (yy + 516 * uu + 128) >> 8;
            r = r < 0 ? 0 : r > 255 ? 255 : r;
            g = g < 0 ? 0 : g > 255 ? 255 : g;
            b = b < 0 ? 0 : b > 255 ? 255 : b;
            out[col] = 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
        }
    }
}

/* Top up the sound queue; called between pictures too. */
static void sfd_pump(void *ws, Session *s, int *done, const char *path)
{
    static int16_t pcm[2048 * 2];
    if (!s->audio || *done) return;
    for (int room = audio_room(); room > 0;) {
        int n = sfd_audio(ws, pcm, (unsigned)(room < 2048 ? room : 2048));
        if (n < 0) { rt_log("movie %s: bad sound", path); audio_close(); s->audio = 0; return; }
        if (n == 0) { *done = 1; audio_flush(); return; }
        audio_write(pcm, n);
        room -= n;
    }
}

int movie_play_sfd(const char *path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0);
    if (file == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size;
    unsigned bytes = sfd_workspace_size();
    void *raw = malloc(bytes + 16u);
    void *ws = raw ? (void *)(((uintptr_t)raw + 15u) & ~(uintptr_t)15u) : 0;
    uint8_t *yuv = 0;
    SfdInfo info;
    int result;
    if (!ws || !GetFileSizeEx(file, &size) || (result = sfd_open(ws, bytes, sfd_read, file, (unsigned long long)size.QuadPart, &info)) != 0) {
        rt_log("movie %s: not a supported SFD", path);
        goto fail;
    }
    int w = (int)info.width, h = (int)info.height;
    unsigned yuv_bytes = (unsigned)(w * h * 3 / 2), index;
    yuv = malloc(yuv_bytes);
    if (!yuv || !hw_picture_begin(w, h)) goto fail;
    if (sfd_video(ws, yuv, yuv_bytes, &index) != 1) { rt_log("movie %s: no first picture", path); hw_picture_end(); goto fail; }
    unsigned pitch;
    uint32_t *pixels = hw_picture_next(&pitch);
    yuv_to_argb(yuv, w, h, pixels, pitch);
    float aspect = (float)((double)w * info.sar_num / ((double)h * info.sar_den));
    double video_seconds = 0, audio_seconds_total = (double)info.audio_frames / info.sample_rate;
    Session s;
    session_begin(&s, (int)info.sample_rate);
    rt_log("movie %s: Dreamcast %dx%d %u/%u fps, sound %u Hz", path, w, h, info.fps_num, info.fps_den, info.sample_rate);
    hw_picture_show(aspect);
    int decoded = 1, video_ended = 0, audio_done = 0, ok = 0;
    for (;;) {
        sfd_pump(ws, &s, &audio_done, path);
        double t = session_clock(&s);
        if (session_skip(&s, t)) { rt_log("movie: skipped"); ok = 1; break; }
        int wanted = (int)(t * info.fps_num / info.fps_den), changed = 0, failed = 0;
        for (int work = 0; work < 8 && !video_ended && decoded - 1 < wanted; work++) {
            if (work) sfd_pump(ws, &s, &audio_done, path);
            int r = sfd_video(ws, yuv, yuv_bytes, &index);
            if (r < 0) { failed = 1; break; }
            if (r == 0) { video_ended = 1; break; }
            decoded++;
            changed = 1;
        }
        if (failed) { rt_log("movie %s: bad picture after %d", path, decoded); ok = 1; break; }
        if (changed) {
            pixels = hw_picture_next(&pitch);
            yuv_to_argb(yuv, w, h, pixels, pitch);
            hw_picture_show(aspect);
        }
        video_seconds = (double)decoded * info.fps_den / info.fps_num;
        if (video_ended && t >= (video_seconds > audio_seconds_total ? video_seconds : audio_seconds_total) &&
            (!s.audio || !audio_busy())) { ok = 1; break; }
        rt_tick();
    }
    session_end(&s);
    hw_picture_end();
    sfd_close(ws);
    free(yuv);
    free(raw);
    CloseHandle(file);
    return ok;
fail:
    if (ws) sfd_close(ws);
    free(yuv);
    free(raw);
    CloseHandle(file);
    return 0;
}
