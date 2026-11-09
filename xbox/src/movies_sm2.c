/*
 * Spider-Man 2's movies on the Xbox. The game's own player (func_80030350) streams the
 * movie through LibCdStream and decodes it with the MDEC, which the Xbox runtime does
 * not have (the PC runs it on its own MDEC). The runtime plays the PS1 movie itself
 * instead (runtime/movie.c, as for SM1: src/movies_sm1.c), called from
 * func_80030350 just after the game has noted the movie as seen (the gallery's
 * bitmask) -- tools/patch_generated_sm2.py inserts the call -- and the game's player is
 * then skipped to its exit, returning what it would: 1 when the movie played to its
 * end, 0 when the player skipped it (the caller plays a following clip only on 1).
 *
 * The movie number in a0: 0x2NN is the movie table's entry NN (0x800A4308, the
 * \CINEMAS\ STRs), 0x1NN the headline table's (0x800A419C, the \HEADLINE\ news clips);
 * entries are 28 bytes with the path pointer first (func_80030A00's lookup). A movie
 * that cannot play (its file missing, say) is skipped, as on SM1.
 */
#include <stdio.h>
#include <string.h>
#include "movie.h"
#include "runtime.h"

#define V0 c->r[2]

#define MOVIE_TABLE 0x800A4308u
#define HEADLINE_TABLE 0x800A419Cu
#define ENTRY_BYTES 28u
#define MOVIES 26
#define HEADLINES 13

/* Called with func_80030350's own registers: s0 = a0 & 0xFF00, s2 = a0 & 0xFF.
 * 1: the movie was handled here; v0 is the player's result. */
int movie_sm2_play(CpuContext *c)
{
    uint32_t category = c->r[16], index = c->r[18], table;
    if (category == 0x200u && index < MOVIES) table = MOVIE_TABLE;
    else if (category == 0x100u && index < HEADLINES) table = HEADLINE_TABLE;
    else return 0;
    uint32_t name = mem_r32(table + index * ENTRY_BYTES);
    if (name < 0x80000000u || name > 0x801FFF80u) return 0;
    /* "\CINEMAS\E1M0.STR;1": printable, terminated within 64 bytes. */
    char text[64];
    int n = 0;
    for (; n < 63; n++) {
        char ch = (char)mem_r8(name + (uint32_t)n);
        if (!ch) break;
        if (ch < 32 || ch > 126) return 0;
        text[n] = ch;
    }
    if (n == 63) return 0;
    text[n] = 0;
    char *version = strchr(text, ';');
    if (version) *version = 0;
    const char *path = text;
    while (*path == '\\') path++;
    char target[96];
    snprintf(target, sizeof target, "D:\\%s", path);
    if (!movie_play_str(target)) { rt_log("movie %s: not played", target); V0 = 1; return 1; }
    V0 = rt_movie_skipped ? 0u : 1u;
    return 1;
}
