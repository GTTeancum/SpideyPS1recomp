/*
 * Sound out of the AC97 (runtime/audio.c): one stream of stereo 16-bit frames at its
 * own rate, resampled to the chip's 48 kHz. Movies (movie.c) and CD-XA music (cd.c)
 * take turns with it; whoever opens it last owns it.
 */
#pragma once
#include <stdint.h>

/* Open (or reopen, discarding what is queued) at `rate` Hz; playback starts once
 * `prebuffer` buffers of about 21 ms are queued. Returns an owner token, 0 on failure. */
unsigned audio_open(int rate, int prebuffer);
int audio_owned(unsigned token);     /* nobody has opened it since */
void audio_close(unsigned token);    /* stops and forgets the queue, if still the owner */
int audio_room(void);                /* input frames the queue takes now */
void audio_write(const int16_t *pcm, int frames);
void audio_flush(void);              /* the end: the part-filled buffer goes out padded */
double audio_seconds(void);          /* played since the open */
int audio_busy(void);                /* frames still to play */
