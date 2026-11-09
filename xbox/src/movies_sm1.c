/*
 * SM1's movies, a port of spiderman/patches/DreamcastMoviePatches.cs and the PC's
 * MovieResolver. The hook sits at 8002AD0C in the movie routine, after the game's
 * allocation, display set-up, STR ring set-up and input polling; returning 1 branches
 * to 8002AF8C, its normal stop/free/restore path.
 *
 * The PC falls back to the game's own STR player when it has no Dreamcast movie; the
 * Xbox has no MDEC, so the runtime plays the PS1 movie itself (runtime/str.c) from the
 * disc's raw CINEMAS file. A Dreamcast .SFD in D:\mods\movies\dreamcast was played in
 * its place (dreamcast_movies=0 turned that off); that is commented out for now, as
 * the SFD decoder plays at about 40% speed on the console. Whatever happens the routine's own
 * player is branched past: a movie that cannot play is skipped, as before.
 */
#include <stdio.h>
#include <string.h>
#include "movie.h"
#include "runtime.h"
#include "settings.h"

#define MOVIE_TABLE 0x80097DECu        /* 24 bytes a movie, its STR path first */
#define MOVIES 24

#if 0   /* Dreamcast movies: commented out for now (see runtime/sfd_core.cpp) */
/* The 23 movies with a verified Dreamcast equivalent of the same name; LOGO has none. */
static const char *const Dreamcast[] = {
    "ATVILOGO", "TTSLOGO", "L1M1", "L1M2", "L2M1", "L2M2", "L2M3", "L3M1", "L4M1", "L4M2",
    "L5M1", "L5M2", "L5M3", "L5M4", "L6M1", "L7M1", "L7M2", "L7M3",
    "L8M1", "L8M2", "L8M3", "L8M4", "L8M5",
};

static int dreamcast_mapped(const char *stem)
{
    for (unsigned i = 0; i < sizeof Dreamcast / sizeof Dreamcast[0]; i++)
        if (rt_strcasecmp(Dreamcast[i], stem) == 0) return 1;
    return 0;
}
#endif

int hook_DreamcastMoviePatches_Play(CpuContext *c)
{
    uint32_t movie = mem_r8(c->r[28] + 0x6D1u);
    if (movie >= MOVIES || mem_r32(c->r[28] + 0x690u) == 0) return 1;
    uint32_t name = mem_r32(MOVIE_TABLE + movie * 24u);
    if (name < 0x80000000u || name > 0x801FFF80u) return 1;
    /* "\CINEMAS\L1M1.STR;1": printable, terminated within 128 bytes. */
    char text[128];
    int n = 0;
    for (; n < 127; n++) {
        char ch = (char)mem_r8(name + (uint32_t)n);
        if (!ch) break;
        if (ch < 32 || ch > 126) return 1;
        text[n] = ch;
    }
    if (n == 127) return 1;
    text[n] = 0;
    char *version = strchr(text, ';');
    if (version) *version = 0;
    for (char *p = text; *p; p++) if (*p == '/') *p = '\\';
    const char *path = text;
    while (*path == '\\') path++;
    const char *file = strrchr(path, '\\');
    file = file ? file + 1 : path;

    char stem[32], target[192];
    snprintf(stem, sizeof stem, "%s", file);
    size_t len = strlen(stem);
    if (len > 4 && rt_strcasecmp(stem + len - 4, ".STR") == 0) stem[len - 4] = 0;
#if 0   /* Dreamcast movies: commented out for now (see runtime/sfd_core.cpp) */
    const char *off = setting("dreamcast_movies");
    if (!(off && strcmp(off, "0") == 0) && dreamcast_mapped(stem)) {
        snprintf(target, sizeof target, "D:\\mods\\movies\\dreamcast\\%s.SFD", stem);
        if (movie_play_sfd(target)) return 1;
        rt_log("movie %s: no Dreamcast version played; PS1 movie", stem);
    }
#endif
    snprintf(target, sizeof target, "D:\\%s", path);
    if (!movie_play_str(target)) rt_log("movie %s: not played", target);
    return 1;
}
