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
extern volatile int rt_presenting;     /* the game has put a picture on screen */
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

/* ---- boot -------------------------------------------------------------------- */
/* Load the boot executable from the disc and run the game. Returns only on failure. */
void rt_boot(const RecompGame *game, const char *disc_root);
