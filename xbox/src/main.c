// OpenSpidey for the original Xbox: entry point.
//
// Runs the recompiled game on its own thread: the generated code nests one C call
// per PlayStation call, so it gets a large stack. The disc root is D: (nxdk mounts
// the launch folder there); the test disc carries the player's extracted game files.

#include <hal/debug.h>
#include <hal/video.h>
#include <windows.h>
#include "runtime.h"
#include "SpiderMan.h"

// Read through the XEMU monitor by the test tools: a running build advances it.
volatile unsigned int g_openspidey_heartbeat;

static DWORD WINAPI game_thread(LPVOID arg)
{
    (void)arg;
    rt_boot(&SpiderMan_game, "D:");
    g_stopped = 1;
    return 0;
}

int main(void)
{
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    debugPrint("OpenSpidey - original Xbox\n");

    HANDLE thread = CreateThread(NULL, 4u * 1024u * 1024u, game_thread, NULL, 0, NULL);
    if (!thread) debugPrint("could not start the game thread\n");
    // The game thread never draws; this one redraws the status once a second.
    for (;;) {
        g_openspidey_heartbeat++;
        if (g_openspidey_heartbeat % 10 == 0) rt_show_status();
        Sleep(100);
    }
    return 0;
}
