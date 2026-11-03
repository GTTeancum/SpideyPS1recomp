/*
 * OpenSpidey Xbox runtime: services beyond the generated-code interface (recomp.h).
 */
#pragma once
#include "recomp.h"

/* ---- log ------------------------------------------------------------------- */
/* Lines go to the screen and to a ring the screen can redraw from. */
void rt_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
/* Report something the runtime does not implement yet, once per name. */
void rt_missing(const char *kind, const char *name);
/* Stop: show the reason, the registers and what is missing; never returns. */
void rt_fatal(CpuContext *c, const char *format, ...) __attribute__((noreturn, format(printf, 2, 3)));
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

/* ---- boot -------------------------------------------------------------------- */
/* Load the boot executable from the disc and run the game. Returns only on failure. */
void rt_boot(const RecompGame *game, const char *disc_root);
