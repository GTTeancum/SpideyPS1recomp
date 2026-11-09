/* Spider-Man (1): the generated program and its patches (see game.h). */
#include "game.h"
#include "SpiderMan.h"
#include "settings.h"

const char game_name[] = "Spider-Man";

const RecompGame *game_program(void) { return &SpiderMan_game; }

void game_install(void)
{
    wide_install(0x8002C2ACu, 1);   /* gameplay swap; the Chase Venom meter */
    level_sm1_install();
    suits_install(0x800A0904u, assets_sm1_suit_loaded, "sm1", VIEWER_ROWS - 20);   /* SM1's actor table; 20 stock rows */
    assets_sm1_install();
    shadow_install(0x800B5268u, 0x800A57CCu, 0x800A0904u, 33, 0x1170u);
}
