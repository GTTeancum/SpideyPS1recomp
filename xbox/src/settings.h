#pragma once
#include "runtime.h"

/* D:\openspidey.ini, read once at start-up (settings.c). */
void settings_load(void);
/* The value for `key`, or NULL. */
const char *setting(const char *key);

/* The controller (input.c): start once, then poll often from the main thread. */
void input_start(void);
void input_poll(void);

/* Game rules set up from the settings (the src/..._sm1.c files). */
void wide_sm1_install(void);
void level_sm1_install(void);
void suits_sm1_install(void);
void assets_sm1_install(void);
void retarget_sm1_install(void);
void shadow_sm1_install(void);

/* The suit (suits_sm1.c): whether one is chosen, the bundled model it reskins, its
 * stock power profile (0..9), and its own model and rig files (NULL: none). */
int suit_active(void);
const char *suit_model(void);
int suit_profile(void);
const char *suit_model_file(void);
const char *suit_rig_file(void);

/* Character models (assets_sm1.c): the WAD lookup and heap hooks call these. */
void assets_sm1_find(CpuContext *c, const char *name);
void assets_sm1_find_exit(CpuContext *c);
int assets_sm1_heap_alloc(CpuContext *c);        /* 1: served from the arena */
int assets_sm1_heap_free(CpuContext *c);         /* 1: was an arena block */
int assets_sm1_owns(uint32_t address);
int assets_sm1_allocation(uint32_t address, uint32_t *size);
int assets_sm1_suit_loaded(void);                /* the loaded spidey is the suit's */
/* Called from the CdWadFind hook before the overlay lookup. */
void level_sm1_redirect(CpuContext *c);
