/*
 * The game this XBE runs (one per XBE, chosen by the Makefile's GAME): src/game_sm1.c
 * for Spider-Man, src/game_sm2.c for Spider-Man 2: Enter Electro.
 */
#pragma once
#include "recomp.h"

/* The generated program: boot executable, overlays, entry point. */
const RecompGame *game_program(void);
/* The game's own patches and hooks, installed before the game thread starts (after
 * the settings are read). */
void game_install(void);
/* For the start-up text. */
extern const char game_name[];
