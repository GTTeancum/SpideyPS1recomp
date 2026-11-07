/*
 * CD-XA ADPCM sound sectors (runtime/xa.c), for PS1 movies (str.c) and streamed music
 * (cd.c). A sector is the 2336 bytes after the sync and header: subheader, then 18
 * sound groups.
 */
#pragma once
#include <stdint.h>

#define XA_MAX_FRAMES 4032   /* a mono sector's samples, each duplicated to stereo */

/* An audio sector the decoder takes: the audio submode bit, 4-bit samples. */
int xa_is_audio(const uint8_t *sector);
int xa_rate(const uint8_t *sector);        /* 37800 or 18900 */
/* Decode into stereo frames (mono duplicated); `history` carries the predictor
 * between sectors of one stream. Returns the frames written. */
int xa_decode(const uint8_t *sector, int32_t history[2][2], int16_t *out);
