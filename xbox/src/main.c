// OpenSpidey for the original Xbox: bring-up entry point.
//
// Proves the toolchain end to end (nxdk build, XBE, disc image, XEMU boot) before
// any game code is ported. It sets the 4:3 480i/p mode the port starts from and
// prints a heartbeat so a capture can tell a running build from a hung one.

#include <hal/debug.h>
#include <hal/video.h>
#include <windows.h>

// Read through the XEMU monitor by the test tools: a running build advances it.
volatile unsigned int g_openspidey_heartbeat;

int main(void)
{
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

    for (;;) {
        g_openspidey_heartbeat++;
        debugClearScreen();
        debugPrint("OpenSpidey - original Xbox bring-up\n");
        debugPrint("Spider-Man / Spider-Man 2: Enter Electro\n\n");
        debugPrint("nxdk build running.\n");
        debugPrint("heartbeat %u\n", g_openspidey_heartbeat);
        Sleep(1000);
    }
    return 0;
}
