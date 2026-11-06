/*
 * Where the console's time goes: CPU cycles (rdtsc, 733 MHz) spent in each part of the
 * runtime, reported in the status text as shares of the time since the last report.
 * Parts nest only as listed; the game's own code is whatever is not counted.
 */
#pragma once
#include <stdint.h>
#include <x86intrin.h>

enum {
    PROF_OT,        /* DrawOTag: GPU command parsing and NV2A submission (nests PAGE, PALETTE) */
    PROF_PAGE,      /* texture page conversion */
    PROF_PRESENT,   /* presenting, including waits for the GPU */
    PROF_IDLE,      /* waiting for the next 60 Hz tick */
    PROF_RIG,       /* suit skeleton */
    PROF_SHADOW,    /* shadow capture */
    PROF_CD,        /* CD service */
    PROF_IRQ,       /* the game's vblank handler */
    PROF_TEXTURE,   /* in gpu-list: native texture lookup (nests tex-convert) */
    PROF_REPLACE,   /* in gpu-list: suit/pack replacement lookup */
    PROF_SUBMIT,    /* in gpu-list: NV2A command building */
    PROF_DEPTH,     /* in gpu-list: GTE depth lookup */
    PROF_GTE,       /* in game: GTE commands */
    PROF_COUNT
};

extern volatile uint64_t rt_prof[PROF_COUNT];
/* PROF_BEGIN(t); ... PROF_END(PROF_X, t); Off unless built with RT_PROFILE=1 (make
 * RT_PROFILE=1): the timers cost ~50 cycles a call on paths run millions of times. */
#ifdef RT_PROFILE
#define PROF_BEGIN(t) uint64_t prof_##t = __rdtsc()
#define PROF_END(p, t) (rt_prof[p] += __rdtsc() - prof_##t)
#else
#define PROF_BEGIN(t) do {} while (0)
#define PROF_END(p, t) do {} while (0)
#endif
