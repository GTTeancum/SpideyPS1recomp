/*
 * OpenSpidey Xbox runtime: services beyond the generated-code interface (recomp.h).
 */
#pragma once
#include "recomp.h"

/* ASCII case-insensitive compare (the Xbox C library has no strcasecmp). */
static inline int rt_strcasecmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y || !x) return x - y;
    }
}

/* ---- log ------------------------------------------------------------------- */
/* Lines go to the screen and to a ring the screen can redraw from. */
void rt_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
/* Report something the runtime does not implement yet, once per name. */
void rt_missing(const char *kind, const char *name);
/* Stop: show the reason, the registers and what is missing; never returns. */
void rt_fatal(CpuContext *c, const char *format, ...) __attribute__((noreturn, format(printf, 2, 3)));
/* Count a named event for the status (pass a string literal). */
void rt_note(const char *name);
#define RECENT 16
extern uint32_t rt_recent_calls[RECENT];
extern unsigned rt_recent_next;

/* Redraw the status screen: counters, recent log lines, missing services. */
void rt_show_status(void);
extern volatile int g_stopped;
extern volatile uint32_t rt_dispatch_count;

/* ---- overlays and dispatch -------------------------------------------------- */
void rt_dispatch_init(const RecompGame *game);
/* Make an overlay's functions callable; one of the same region it covers is dropped. */
int rt_overlay_load(const char *name);
const RecompGame *rt_game(void);

/* ---- BIOS -------------------------------------------------------------------- */
/* Handle a call to a BIOS vector (0xA0, 0xB0, 0xC0, 0xBFCxxxxx); 0 if not one. */
int bios_dispatch(CpuContext *c, uint32_t address);
void bios_init(void);

/* The PSY-Q interrupt environment registered with B(19h), 0 if none yet. */
uint32_t bios_intr_env(void);

/* ---- clock and interrupts ------------------------------------------------------ */
extern volatile uint32_t rt_vblank_count;
/* Named numbers game code reports in the status text (log.c). */
#define RT_METRICS 4
extern volatile uint32_t rt_metric[RT_METRICS];
extern const char *rt_metric_name[RT_METRICS];
extern volatile uint32_t rt_hw_replaced;
extern volatile uint32_t rt_pack_hits, rt_pack_loads;   /* texpack.c */   /* primitives drawn with a replacement texture */
extern volatile uint32_t rt_hw_texture_decodes, rt_hw_palette_builds, rt_hw_frames, rt_hw_wait, rt_hw_perspective, rt_hw_affine;   /* hwgpu.c */
extern volatile uint32_t rt_ot_start, rt_ot_addr, rt_ot_packets;   /* libgpu.c: DrawOTag progress */
/* The last runtime step the game thread entered, for the status text. */
extern volatile const char *rt_where;
#define RT_WHERE(s) (rt_where = (s))
extern volatile int rt_presenting;     /* the game has put a picture on screen */

/* ---- widescreen (timing.c, gte.c, libgpu.c, gpu.c; the game's rules in src/) ---- */
extern int rt_video_720p, rt_video_wide;   /* output mode; wide: the display is 16:9 */
extern int rt_frame_wide;                  /* this frame is presented at 16:9 */
extern int rt_fov_num, rt_fov_den;         /* GTE horizontal projection scale */
/* The camera depth recorded for an RTP's screen point (perspective texturing), given
 * its input vector and SZ. NULL: SZ itself. */
extern uint16_t (*rt_rtp_depth_hook)(int32_t vx, int32_t vy, int32_t vz, uint16_t sz);
extern uint32_t rt_disp_grandparent;       /* PutDispEnv's caller's caller: the frame loop */
/* Called at each PutDispEnv, once a frame, to decide the next frame's projection. */
extern void (*rt_frame_hook)(void);

/* A primitive as the game drew it, offered to the game's rules before drawing. */
typedef struct RtPrim {
    int count;                     /* 3 or 4 vertices, 2 for a sprite's two corners */
    int x[4], y[4];                /* VRAM coordinates, drawing offset applied */
    uint8_t u[4], v[4];
    int depth;                     /* every vertex came from the GTE */
    int textured, semi, gouraud;
    int tpage, clut;               /* as in the GP0 packet */
    int left, top, right, bottom;  /* drawing area */
    int offset_x, offset_y;        /* drawing offset */
} RtPrim;
/* May move x[]; returns nothing. NULL: no rules. */
extern void (*rt_prim_hook)(RtPrim *p);
/* May draw something else in the primitive's place (into display buffer `region`,
 * through hw_* calls) and return 1 to skip it. NULL: none. */
extern int (*rt_prim_replace_hook)(const RtPrim *p, int region);
/* Ordering tables drawn so far (libgpu.c): a new value is a new frame's drawing. */
extern volatile uint32_t rt_drawotag_count;

/* ---- replacement textures (hwgpu.c asks; the game's suit rules in src/ answer) ---- */
/* The part of a texture page a textured primitive samples. */
typedef struct RtTile {
    int page_x, page_y, depth;         /* depth: 0 4-bit, 1 8-bit, 2 15-bit */
    int clut_x, clut_y;
    int u0, v0, w, h;                  /* texel bounds of the primitive's UVs */
} RtTile;
/* A DXT1 texture standing in for a rectangle of PlayStation texels. */
typedef struct RtReplacement {
    const void *dxt1;                  /* contiguous memory the GPU reads */
    int width, height;                 /* powers of two */
    int u0, v0, w, h;                  /* the PlayStation texels it covers */
} RtReplacement;
/* Returns 1 and fills `out` when the tile has a replacement. NULL: none. */
extern int (*rt_texture_hook)(const RtTile *tile, RtReplacement *out);
/* Called as each ordering table is drawn, before its first primitive. */
extern void (*rt_drawotag_hook)(void);
void rt_clock_init(void);
void rt_tick(void);
void rt_raise_irq(int irq);
void rt_wait_counter(CpuContext *c, uint32_t address);
/* Called from code paths a waiting game spins through; ticks when an edge is due. */
void rt_idle(void);
CpuContext *rt_cpu(void);

/* ---- CD ------------------------------------------------------------------------- */
/* Read the disc's sector map (written by tools/make_disc_map.py). */
int cd_init(const char *disc_root);
void cd_tick(void);

/* ---- pads ------------------------------------------------------------------------ */
extern volatile uint16_t rt_pad_buttons;      /* port 1, active-low PlayStation layout */
extern volatile uint8_t rt_pad_sticks[4];     /* right x, right y, left x, left y */
void rt_pad_set_buffers(uint32_t buf1, uint32_t buf2, int analog);
void rt_pad_load_script(const char *disc_root);
void rt_pad_refresh(void);

/* ---- boot -------------------------------------------------------------------- */
/* Load the boot executable from the disc and run the game. Returns only on failure. */
void rt_boot(const RecompGame *game, const char *disc_root);
