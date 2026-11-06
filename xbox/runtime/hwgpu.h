/*
 * NV2A (pbkit) drawing for the PlayStation GPU. gpu.c keeps parsing GP0/GP1 and
 * owns VRAM; primitives drawn inside a display buffer go here instead of the
 * software rasteriser. Each display buffer is a persistent pbkit extra buffer
 * (640x480, the PlayStation buffer at its output scale), so what a game leaves in a
 * buffer stays there as it would in VRAM; presenting copies the displayed one to
 * the back buffer. Textures are decoded from VRAM per page/CLUT and cached until
 * the VRAM they came from is written.
 */
#pragma once
#include <stdint.h>

#define HW_REGIONS 2

/* w: the GTE depth the vertex was projected at (0: none, drawn affine). */
typedef struct HwVertex { float x, y; uint8_t r, g, b; uint8_t u, v; float w; } HwVertex;

typedef struct HwTexture {
    int page_x, page_y, depth, clut;            /* depth: 0 4-bit, 1 8-bit, 2 15-bit */
    int win_mask_x, win_mask_y, win_off_x, win_off_y;
} HwTexture;

int hw_init(void);
int hw_active(void);

/* Which display buffer a draw area belongs to (-1: not a display buffer). */
int hw_region_for(int x, int y);
/* A display buffer seen through GP1(05h)/(08h). */
void hw_note_display(int x, int y, int width, int height);

/* Primitives in VRAM coordinates of region `r`. tex NULL: untextured. */
void hw_triangle(int r, const HwVertex v[3], const HwTexture *tex, int semi, int blend, int raw, int gouraud);
/* tw, th: the texture span, which differs from w, h when a rule resized the sprite. */
void hw_rect(int r, int x, int y, int w, int h, uint8_t u0, uint8_t v0, int tw, int th, uint8_t cr, uint8_t cg, uint8_t cb,
             const HwTexture *tex, int semi, int blend, int raw);
void hw_line(int r, const HwVertex v[2], int semi, int blend);
void hw_fill(int r, int x, int y, int w, int h, uint16_t color15);
/* Copy a VRAM rectangle (just uploaded, copied or filled) onto region `r`. */
void hw_blit(int r, int x, int y, int w, int h);
/* VRAM changed: cached textures reading it are stale. */
void hw_vram_written(int x, int y, int w, int h);
/* The VRAM generation: overall, and the newest under a rectangle (VRAM pixels). */
uint32_t hw_vram_generation(void);
uint32_t hw_vram_newest(int x, int y, int w, int h);

/* Projected depths, for perspective-correct texturing. The GTE notes every screen
 * point it produces; a primitive vertex at that point gets its depth. Two frames are
 * kept, since a frame's ordering table is built before the previous one is drawn. */
void hw_note_depth(int sx, int sy, uint16_t sz);
float hw_depth_at(int sx, int sy);
void hw_depth_frame(void);

/* A ground shadow: `count` triangles (VRAM coordinates of region `r`) that halve the
 * brightness of each pixel they cover once, however many of them overlap -- through
 * the stencil, cleared first, as the PC's PrimFlags.Shadow does. */
void hw_shadow(int r, const float (*xy)[2], int count);

/* The drawing area (GP0 E3/E4, VRAM coordinates, inclusive): primitives are clipped
 * to it, as the PlayStation does. Cutscenes narrow it to keep the scene off the black
 * bars at the top and bottom. */
void hw_area(int left, int top, int right, int bottom);

/* Show the display buffer at (x, y) on the TV: filling a 16:9 screen when `wide`,
 * else at 4:3 (pillarboxed on a 16:9 screen). */
void hw_present(int x, int y, int wide);
