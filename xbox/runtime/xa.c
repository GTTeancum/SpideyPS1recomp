/*
 * CD-XA ADPCM (see xa.h). 18 sound groups of 128 bytes: 16 bytes of parameters, then
 * 28 words carrying one 4-bit sample of each of the 8 units. Stereo units alternate
 * left, right.
 */
#include "xa.h"

int xa_is_audio(const uint8_t *s) { return (s[2] & 0x04) && !(s[3] & 0x30); }
int xa_rate(const uint8_t *s) { return (s[3] & 0x04) ? 18900 : 37800; }

int xa_decode(const uint8_t *s, int32_t history[2][2], int16_t *out)
{
    static const int pos[5] = { 0, 60, 115, 98, 122 }, neg[5] = { 0, 0, -52, -55, -60 };
    int stereo = s[3] & 1, frames = 0;
    const uint8_t *d = s + 8;
    for (int g = 0; g < 18; g++) {
        const uint8_t *group = d + g * 128;
        int16_t unit[8][28];
        for (int u = 0; u < 8; u++) {
            int param = group[4 + u], shift = param & 15, filter = (param >> 4) & 3;
            if (shift > 12) shift = 9;
            int32_t *h = history[stereo ? (u & 1) : 0];
            for (int i = 0; i < 28; i++) {
                int byte = group[16 + i * 4 + (u >> 1)];
                int nibble = (u & 1) ? byte >> 4 : byte & 15;
                int32_t sample = (int32_t)(int16_t)(uint16_t)(nibble << 12) >> shift;
                sample += (h[0] * pos[filter] + h[1] * neg[filter] + 32) >> 6;
                if (sample > 32767) sample = 32767;
                if (sample < -32768) sample = -32768;
                h[1] = h[0];
                h[0] = sample;
                unit[u][i] = (int16_t)sample;
            }
        }
        if (stereo)
            for (int u = 0; u < 8; u += 2)
                for (int i = 0; i < 28; i++) { out[frames * 2] = unit[u][i]; out[frames * 2 + 1] = unit[u + 1][i]; frames++; }
        else
            for (int u = 0; u < 8; u++)
                for (int i = 0; i < 28; i++) { out[frames * 2] = out[frames * 2 + 1] = unit[u][i]; frames++; }
    }
    return frames;
}
