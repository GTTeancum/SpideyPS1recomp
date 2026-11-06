/*
 * PlayStation .STR movies: MDEC video (bitstream versions 1 and 2) with interleaved
 * XA ADPCM sound, read from a file of raw 2336-byte sectors (subheader onward), the
 * form the disc map's raw files have. The stream plays at the double-speed CD rate,
 * 150 sectors a second: a frame's time is the time its first sector would be read.
 */
#pragma once
#include <stdint.h>

typedef struct StrMovie StrMovie;

/* NULL: no such file, not raw sectors, or no video in its first sectors. */
StrMovie *str_open(const char *path);
void str_close(StrMovie *m);

int str_width(const StrMovie *m);
int str_height(const StrMovie *m);
/* Sound: its rate (0: the movie has none) and when, from the stream's start, its
 * first sector plays. */
int str_audio_rate(const StrMovie *m);
double str_audio_start(const StrMovie *m);

/* Read sectors ahead until `seconds` of sound is queued (or a few frames, for a
 * movie without sound), the frame queue is full, or the file ends. */
void str_read_ahead(StrMovie *m, double seconds);

/* Decode the next frame into `argb` (str_width x str_height, `pitch` bytes a row)
 * and give its time in seconds. 1: a frame, 0: the end, -1: data it cannot decode. */
int str_next_frame(StrMovie *m, uint32_t *argb, unsigned pitch, double *time);

/* Stereo frames of sound decoded so far and not yet taken; take up to `frames`. */
int str_audio_queued(const StrMovie *m);
int str_audio_read(StrMovie *m, int16_t *pcm, int frames);

/* The whole file is read and every frame taken. */
int str_ended(const StrMovie *m);
