// OpenSpidey for the original Xbox: entry point.
//
// Runs the recompiled game on its own thread: the generated code nests one C call
// per PlayStation call (stack size: rt_game_stack_size). The disc root is D: (nxdk mounts
// the launch folder there); the test disc carries the player's extracted game files.

#include "card.h"
#include <hal/debug.h>
#include <hal/video.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include "runtime.h"
#include "game.h"
#include "settings.h"
#include "spu.h"


// Read through the XEMU monitor by the test tools: a running build advances it.
volatile unsigned int g_openspidey_heartbeat;

/* The game thread's stack: its size, and the bounds the kernel gave it, for the status
 * text's depth measurement. The kernel commits the whole stack up front. Measured on
 * the console (2026-11-08) across every level, a Dreamcast movie and a save, the
 * deepest point was 4 KB below the thread's first frame (the PlayStation's call depth
 * is shallow and the generated functions' frames small). 256 KB is 64 times that;
 * 4 MB held 3.75 MB that was never touched. */
const uint32_t rt_game_stack_size = 256u * 1024u;
volatile uintptr_t rt_game_stack_base, rt_game_stack_limit;
#define STACK_MARK 0x5AFE57ACu

static DWORD WINAPI game_thread(LPVOID arg)
{
    (void)arg;
    {
        /* Mark the part of this thread's own stack below the current frame (less a
         * margin for the calls made from here) so the deepest point reached can be
         * read later. Bounds from the kernel's thread record. */
        PKTHREAD t = KeGetCurrentThread();
        uintptr_t limit = (uintptr_t)t->StackLimit, stop = (uintptr_t)__builtin_frame_address(0) - 1024u;
        if (limit && limit < stop && stop < (uintptr_t)t->StackBase) {
            for (volatile uint32_t *w = (volatile uint32_t *)limit; (uintptr_t)w < stop; w++) *w = STACK_MARK;
            rt_game_stack_limit = limit;
            rt_game_stack_base = (uintptr_t)t->StackBase;
        }
    }
    rt_boot(game_program(), "D:");
    g_stopped = 1;
    return 0;
}

int main(void)
{
    // 720p where the console's video settings allow it, else 480 (p or i, as the
    // settings say). The picture is 16:9 on 720p and on a 480 TV set to widescreen.
    rt_mem_mark("start");
    rt_video_720p = XVideoSetMode(1280, 720, 32, REFRESH_DEFAULT);
    if (!rt_video_720p) XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    rt_video_wide = rt_video_720p || (XVideoGetEncoderSettings() & VIDEO_WIDESCREEN) != 0;
    rt_mem_mark("video mode");
    settings_load();
    if (setting("precise_vertices") && setting("precise_vertices")[0] == '0') rt_precise_vertices = 0;
    if (setting("geometry_debug")) rt_geometry_debug = setting("geometry_debug")[0] - '0';
    if (setting("crack_fill")) rt_crack_fill = setting("crack_fill")[0] == '1';
    if (setting("sound") && setting("sound")[0] == '0') spu_enabled = 0;
    card_init();   /* first: the game's set-up reads the remembered suit from the save folder */
    game_install();
    debugPrint("%s - original Xbox\n", game_name);
    input_start();
    rt_mem_mark("set-up");

    HANDLE thread = CreateThread(NULL, rt_game_stack_size, game_thread, NULL, 0, NULL);
    if (!thread) debugPrint("could not start the game thread\n");
    // The game thread never draws; this one reads the controller every 8 ms (twice a
    // vblank) and redraws the status about once a second.
    for (unsigned n = 0;; n++) {
        input_poll();
        g_openspidey_heartbeat++;
        if (n % 125 == 0) rt_show_status();
        Sleep(8);
    }
    return 0;
}
