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
void level_sm1_install(void);
void assets_sm1_install(void);

/* ArmorTextures' per-texture test (patches_common.c): 1 to skip. */
int armor_skip_unmapped(CpuContext *c, uint32_t table, uint32_t pairs);

/* Shared by both games, set up with each game's addresses: widescreen (wide.c), the
 * player's shadow (shadow.c). */
void wide_install(uint32_t gameplay_swap, int chase_meter_hud);
void shadow_install(uint32_t player_pointer, uint32_t camera_rotation, uint32_t model_table, int blob_clut,
                    uint32_t transform_flag);

/* Mod suits (suits.c, shared): the catalogue of D:\mods\suits, read at start-up with the
 * kept choice restored (or suit=<id> for a test run). actor_table is the game's actor
 * table, suit_loaded whether the loaded spidey is the suit's, game_tag "sm1"/"sm2"
 * (the kept choice's file), capacity the mod rows its costume viewer has room for. */
#define VIEWER_ROWS 253           /* stock + mod rows in either game's viewer (the PC's MaxCount) */
#define SUITS_MAX (VIEWER_ROWS - 19)
void suits_install(uint32_t actor_table, int (*suit_loaded)(void), const char *game_tag, int capacity);
int suits_count(void);
const char *suits_name(int i);
const char *suits_comments(int i);
int suits_profile(int i);                 /* SM1's profile index */
const char *suits_profile_name(int i);
/* Choose suit i (-1: none, the stock costumes); keep=1 remembers it. 1: done. */
int suits_select(int i, int keep);
int suits_selected(void);                 /* the chosen suit, or -1 */
/* The chosen suit: whether there is one, the bundled model it reskins, its stock power
 * profile as SM1's index (0..9) and by name, and its own model and rig files (NULL:
 * none). */
int suit_active(void);
const char *suit_profile_name(void);
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

/* Spider-Man 2 (the src/..._sm2.c files). */
void level_sm2_install(void);
void level_sm2_redirect(CpuContext *c);
void costume_sm2_install(void);
int costume_sm2_find(CpuContext *c, const char *name);   /* 1: answered, skip the lookup */
int costume_sm2_heap_alloc(CpuContext *c);              /* 1: a frame packet pool served */
int costume_sm2_suit_loaded(void);
