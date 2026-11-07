/*
 * The Xbox controller as PlayStation pad 1, through nxdk's SDL2 game controller API:
 * the PC's default bindings (RecompOne.Runtime/Config/GameConfig.cs GamepadBindings,
 * Host/InputManager.cs). A, B, X, Y are Cross, Circle, Square, Triangle; the shoulder
 * buttons (white and black) L1 and R1; the triggers L2 and R2; the stick clicks L3
 * and R3; Start and Back are Start and Select; the d-pad and left stick steer. The
 * sticks are also reported as the DualShock's analog axes.
 *
 * Polled from the main thread (SDL is not thread-safe); the game thread reads the
 * result at each vblank (runtime/libpad.c).
 */
#include <SDL.h>
#include "runtime.h"
#include "settings.h"

#define AXIS_THRESHOLD 8000      /* triggers as L2/R2 */
#define STICK_THRESHOLD 16000    /* left stick as the d-pad */

static SDL_GameController *g_pad;
static int g_ready;

void input_start(void)
{
    g_ready = SDL_Init(SDL_INIT_GAMECONTROLLER) == 0;
    if (!g_ready) rt_log("controller: SDL init failed: %s", SDL_GetError());
}

static uint8_t axis_byte(int16_t axis)
{
    float f = axis * 1.3f / 32768.0f;
    f = f < -1.0f ? -1.0f : f > 1.0f ? 1.0f : f;
    int v = (int)((f + 1.0f) * 127.5f + 0.5f);
    return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
}

void input_poll(void)
{
    if (!g_ready) return;
    SDL_GameControllerUpdate();
    if (g_pad && !SDL_GameControllerGetAttached(g_pad)) { SDL_GameControllerClose(g_pad); g_pad = 0; }
    if (!g_pad)
        for (int i = 0; i < SDL_NumJoysticks() && !g_pad; i++)
            if (SDL_IsGameController(i)) g_pad = SDL_GameControllerOpen(i);
    if (!g_pad) { rt_pad_buttons = 0xFFFF; return; }

    static const struct { SDL_GameControllerButton button; uint16_t bit; } map[] = {
        { SDL_CONTROLLER_BUTTON_A, 1 << 14 },             /* Cross */
        { SDL_CONTROLLER_BUTTON_B, 1 << 13 },             /* Circle */
        { SDL_CONTROLLER_BUTTON_X, 1 << 15 },             /* Square */
        { SDL_CONTROLLER_BUTTON_Y, 1 << 12 },             /* Triangle */
        { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 1 << 10 },  /* L1 */
        { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1 << 11 }, /* R1 */
        { SDL_CONTROLLER_BUTTON_LEFTSTICK, 1 << 1 },      /* L3 */
        { SDL_CONTROLLER_BUTTON_RIGHTSTICK, 1 << 2 },     /* R3 */
        { SDL_CONTROLLER_BUTTON_START, 1 << 3 },          /* Start */
        { SDL_CONTROLLER_BUTTON_BACK, 1 << 0 },           /* Select */
        { SDL_CONTROLLER_BUTTON_DPAD_UP, 1 << 4 },
        { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1 << 5 },
        { SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1 << 6 },
        { SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1 << 7 },
    };
    uint16_t held = 0;
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
        if (SDL_GameControllerGetButton(g_pad, map[i].button)) held |= map[i].bit;
    if (SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > AXIS_THRESHOLD) held |= 1 << 8;    /* L2 */
    if (SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > AXIS_THRESHOLD) held |= 1 << 9;   /* R2 */
    int16_t lx = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX);
    int16_t ly = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY);
    if (ly < -STICK_THRESHOLD) held |= 1 << 4;
    if (lx > STICK_THRESHOLD) held |= 1 << 5;
    if (ly > STICK_THRESHOLD) held |= 1 << 6;
    if (lx < -STICK_THRESHOLD) held |= 1 << 7;
    rt_pad_sticks[0] = axis_byte(SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTX));
    rt_pad_sticks[1] = axis_byte(SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTY));
    rt_pad_sticks[2] = axis_byte(lx);
    rt_pad_sticks[3] = axis_byte(ly);
    rt_pad_buttons = (uint16_t)~held;   /* active-low, as the console reads it */
}
