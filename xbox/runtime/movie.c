/*
 * Movie playback (see movie.h): the clock a movie runs to (the sound output, audio.c),
 * the skip button, and the two players' loops. Follows the PC's DreamcastMovies.TryPlay:
 * the first picture is decoded before anything is taken over; the sound's played
 * position, not the time queued, picks the picture; late pictures are decoded and
 * dropped a few at a time so the console keeps ticking.
 */
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include "hwgpu.h"
#include "movie.h"
#include "runtime.h"
#include "str.h"

#include "audio.h"

/* ---- a session: the clock and the skip button ---------------------------------------------- */
typedef struct {
    unsigned audio;                  /* the sound output (its owner token) is the clock */
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
    s->audio = rate > 0 ? audio_open(rate, 2) : 0;
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
    if (s->audio) audio_close(s->audio);
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
            audio_close(s->audio);
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

/* The PS1 player's state, for the status text while a movie plays: pictures shown,
 * video done, sound flushed, sound open, sound busy, file ended, sound still queued,
 * the movie clock in ms. */
volatile int32_t rt_movie_state[8];

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
    double worst_late = 0;   /* how far a picture came after its time, at most (status) */
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
                if (t - due > worst_late) worst_late = t - due;
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
        rt_movie_state[0] = shown; rt_movie_state[1] = video_done; rt_movie_state[2] = flushed;
        rt_movie_state[3] = s.audio != 0; rt_movie_state[4] = s.audio ? audio_busy() : 0;
        rt_movie_state[5] = str_ended(m); rt_movie_state[6] = str_audio_queued(m); rt_movie_state[7] = (int32_t)(t * 1000.0);
        if (video_done && !waiting && t >= last + 1.0 / 15.0 && (!s.audio || (flushed && !audio_busy()))) { ok = 1; break; }
        rt_tick();
    }
    /* Whole milliseconds: the console's printf has no floating point. */
    rt_log("movie timing: %d pictures, %ld ms long, played in %ld ms, worst picture %ld ms late",
           shown, (long)(last * 1000.0), (long)(elapsed(&s) * 1000.0), (long)(worst_late * 1000.0));
    session_end(&s);
    hw_picture_end();
    str_close(m);
    return ok;
}

/* ---- Dreamcast .SFD (the PC's decoder) ------------------------------------------------------ */
/* Commented out (2026-11-09): Dreamcast movies are out on the Xbox for now (about 40%
 * speed on the console; see runtime/sfd_core.cpp). */
#if 0
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
        if (n < 0) { rt_log("movie %s: bad sound", path); audio_close(s->audio); s->audio = 0; return; }
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
    int32_t held = 0;   /* workspace and picture bytes counted in the status */
    if (!ws || !GetFileSizeEx(file, &size) || (result = sfd_open(ws, bytes, sfd_read, file, (unsigned long long)size.QuadPart, &info)) != 0) {
        rt_log("movie %s: not a supported SFD", path);
        goto fail;
    }
    int w = (int)info.width, h = (int)info.height;
    unsigned yuv_bytes = (unsigned)(w * h * 3 / 2), index;
    yuv = malloc(yuv_bytes);
    if (yuv) { held = (int32_t)(bytes + 16u + yuv_bytes); rt_mem_add(MEM_MOVIE, held); }
    if (!yuv || !hw_picture_begin(w, h)) goto fail;
    if (sfd_video(ws, yuv, yuv_bytes, &index) != 1) { rt_log("movie %s: no first picture", path); hw_picture_end(); goto fail; }
    unsigned pitch;
    uint32_t *pixels = hw_picture_next(&pitch);
    yuv_to_argb(yuv, w, h, pixels, pitch);
    /* Square pixels, as the PS1 movies: 320x240 fills 4:3. The files' pixel aspect code
     * (CCIR 601 525-line, the encoder's NTSC default) made them about 9% too narrow. */
    float aspect = (float)w / (float)h;
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
    rt_mem_add(MEM_MOVIE, -held);
    CloseHandle(file);
    return ok;
fail:
    if (ws) sfd_close(ws);
    if (held) rt_mem_add(MEM_MOVIE, -held);
    free(yuv);
    free(raw);
    CloseHandle(file);
    return 0;
}
#endif   /* Dreamcast .SFD */
