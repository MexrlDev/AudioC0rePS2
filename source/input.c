#include <SDL2/SDL.h>
#include "input.h"

static SDL_GameController *g_gc = NULL;
static u32 prev_btn, curr_btn;

void input_init(void)
{
    int n = SDL_NumJoysticks();
    printf("[AudioC0re] input_init: num_joysticks=%d\n", n);
    fflush(stdout);

    if (n > 0) {
        g_gc = SDL_GameControllerOpen(0);
        if (g_gc) {
            printf("[AudioC0re] input_init: opened '%s'\n",
                   SDL_GameControllerName(g_gc));
        } else {
            printf("[AudioC0re] input_init: SDL_GameControllerOpen failed: %s\n",
                   SDL_GetError());
        }
        fflush(stdout);
    } else {
        printf("[AudioC0re] input_init: no controllers\n"); fflush(stdout);
    }
}

void input_poll(void)
{
    prev_btn = curr_btn;
    curr_btn = 0;
    if (!g_gc) return;

    SDL_GameControllerUpdate();

    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_DPAD_UP))    curr_btn |= BTN_UP;
    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  curr_btn |= BTN_DOWN;
    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  curr_btn |= BTN_LEFT;
    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) curr_btn |= BTN_RIGHT;

    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_A))          curr_btn |= BTN_CROSS;
    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_B))          curr_btn |= BTN_CIRCLE;
    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_X))          curr_btn |= BTN_SQUARE;
    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_Y))          curr_btn |= BTN_TRIANGLE;

    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  curr_btn |= BTN_L1;
    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) curr_btn |= BTN_R1;
    if (SDL_GameControllerGetAxis(g_gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT)  > 8000) curr_btn |= BTN_L2;
    if (SDL_GameControllerGetAxis(g_gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8000) curr_btn |= BTN_R2;

    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_BACK))       curr_btn |= BTN_SELECT;
    if (SDL_GameControllerGetButton(g_gc, SDL_CONTROLLER_BUTTON_START))      curr_btn |= BTN_START;

    int ax = SDL_GameControllerGetAxis(g_gc, SDL_CONTROLLER_AXIS_LEFTX);
    int ay = SDL_GameControllerGetAxis(g_gc, SDL_CONTROLLER_AXIS_LEFTY);
    if (ay < -16000) curr_btn |= BTN_UP;
    if (ay >  16000) curr_btn |= BTN_DOWN;
    if (ax < -16000) curr_btn |= BTN_LEFT;
    if (ax >  16000) curr_btn |= BTN_RIGHT;

    /* Trace: print only when the button mask changes and is non-zero
       (avoids flooding log when idle). */
    if (curr_btn != prev_btn && curr_btn != 0) {
        printf("[AudioC0re] input: %08x -> %08x\n",
               (unsigned)prev_btn, (unsigned)curr_btn);
        fflush(stdout);
    }
}

int input_pressed(u32 mask) { return (curr_btn & ~prev_btn) & mask; }
int input_held   (u32 mask) { return curr_btn & mask; }
