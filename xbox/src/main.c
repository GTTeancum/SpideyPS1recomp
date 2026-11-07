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
#include "settings.h"


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
    // 720p where the console's video settings allow it, else 480 (p or i, as the
    // settings say). The picture is 16:9 on 720p and on a 480 TV set to widescreen.
    rt_video_720p = XVideoSetMode(1280, 720, 32, REFRESH_DEFAULT);
    if (!rt_video_720p) XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    rt_video_wide = rt_video_720p || (XVideoGetEncoderSettings() & VIDEO_WIDESCREEN) != 0;
    settings_load();
    if (setting("precise_vertices") && setting("precise_vertices")[0] == '0') rt_precise_vertices = 0;
    if (setting("geometry_debug")) rt_geometry_debug = setting("geometry_debug")[0] - '0';
    wide_sm1_install();
    level_sm1_install();
    suits_sm1_install();
    assets_sm1_install();
    retarget_sm1_install();
    shadow_sm1_install();
    debugPrint("OpenSpidey - original Xbox\n");
    input_start();

    HANDLE thread = CreateThread(NULL, 4u * 1024u * 1024u, game_thread, NULL, 0, NULL);
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
