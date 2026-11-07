/*
 * The PlayStation sound chip (runtime/spu.c), a port of the PC runtime's Hardware/Spu.cs
 * and Hardware/XaAudio.cs: 24 ADPCM voices with ADSR envelopes, Gaussian interpolation,
 * pitch modulation and noise, the CD audio (XA) input, and the main volume. Mixed at
 * 44.1 kHz in step with the console clock (735 frames a 60 Hz tick) and sent to the
 * AC97 (audio.c) whenever no movie holds it. No reverb, as on the PC.
 */
#pragma once
#include <stdint.h>

int spu_is_reg(uint32_t phys);                  /* 0x1F801C00..0x1F801FFF */
uint16_t spu_read16(uint32_t phys);
void spu_write16(uint32_t phys, uint16_t value);
/* DMA channel 4, RAM to sound RAM at the transfer address. */
void spu_dma_write(uint32_t madr, uint32_t bytes);

/* CD audio: decoded stereo frames at their source rate (cd.c's XA streaming). */
void spu_xa_push(const int16_t *pcm, int frames, int rate);
void spu_xa_reset(void);
int spu_xa_buffered(void);
void spu_cd_mix(uint8_t ltol, uint8_t ltor, uint8_t rtor, uint8_t rtol);   /* CdMix (CdlATV) */
extern volatile int spu_cd_muted;               /* CdControl Mute/Demute */

/* Once a vblank: mix that tick's frames and queue them for the speakers. */
void spu_tick(void);
