/* Spider-Man 2: Enter Electro: the generated program and its patches (see game.h).
 * Patches not ported yet run their defaults (build/hooks_default_spiderman2.c), which
 * let the game's own code run. */
#include "game.h"
#include "settings.h"
#include "SpiderMan2.h"

const char game_name[] = "Spider-Man 2: Enter Electro";

const RecompGame *game_program(void) { return &SpiderMan2_game; }

void game_install(void)
{
    wide_install(0x80031EA0u, 0);   /* gameplay swap (the PC's Wide.Install(0x80031EA0)) */
    level_sm2_install();
    costume_sm2_install();
    shadow_install(0x800C21F4u, 0x800B32C4u, 0x800ACED8u, 41, 0x1358u);
}
