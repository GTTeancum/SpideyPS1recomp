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
/* hw_init the first time; whether the NV2A is in use. */
int hw_ready(void);

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
/* Before VRAM is read back (StoreImage, a copy's source): bring what the display
 * buffers' render targets hold for that rectangle into gpu_vram. */
void hw_read_back(int x, int y, int w, int h);
/* The VRAM generation: overall, and the newest under a rectangle (VRAM pixels). */
uint32_t hw_vram_generation(void);
/* Frames presented so far (a texture drawn in frame f may be read by the GPU until f+3). */
uint32_t hw_frame_number(void);
/* Wait until the GPU has finished everything pushed so far: memory it read may then be
 * freed at once. */
void hw_finish(void);
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

/* Whether the primitives drawn next are world geometry (GTE points), for the gap fill. */
void hw_set_world(int world);
/* Whether the primitive drawn next is the frame's background clear (DRAWENV isbg): as
 * on the PC, it leaves its pixels "nothing drawn here", so cracks onto it are filled. */
void hw_set_background(int background);
/* GP1(05h): the game shows the display buffer at (x, y), finished. The gap fill runs
 * on it once, if primitives were drawn into it since the last fill. */
void hw_frame_shown(int x, int y);

/* Show the display buffer at (x, y) on the TV: filling a 16:9 screen when `wide`,
 * else at 4:3 (pillarboxed on a 16:9 screen). */
void hw_present(int x, int y, int wide);

/* Movie pictures (movie.c): w x h A8R8G8B8 images the CPU writes, shown on their own,
 * scaled to fill as much of the screen as their display aspect (width / height)
 * allows. begin allocates them (0: no memory or no NV2A), next hands out the buffer to
 * write the coming picture into, show puts that one on screen, end frees them. */
int hw_picture_begin(int w, int h);
uint32_t *hw_picture_next(unsigned *pitch);
void hw_picture_show(float aspect);
void hw_picture_end(void);
