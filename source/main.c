/* AudioC0re - PS2 */
#include <SDL2/SDL.h>
#include <sifrpc.h>
#include <kernel.h>
#include <unistd.h>
#include <sys/stat.h>
#include "app.h"
#include "input.h"
#include "ui.h"
#include "library.h"
#include "audio.h"
#include "splash.h"
#include "menu.h"
#include "player.h"
#include "log.h"
#include "lang.h"
#include "config.h"
#include "settings.h"

u32 *g_fb = NULL;
static SDL_Renderer *g_ren = NULL;
static SDL_Texture  *g_tex = NULL;

int g_force_full_present = 1;
int g_dirty_y0 = 0;
int g_dirty_y1 = SCR_H;

static void present_frame(void) {
    if (!g_fb || !g_tex || !g_ren) return;
    SDL_UpdateTexture(g_tex, NULL, g_fb, SCR_W * 4);
    SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
    SDL_RenderPresent(g_ren);
}

#define TRACE(fmt, ...) do { \
    printf("[AudioC0re] " fmt "\n", ##__VA_ARGS__); \
    if (log_enabled()) log_write(fmt "\n", ##__VA_ARGS__); \
    fflush(stdout); \
} while (0)

static void handle_source_gone(void)
{
    if (log_enabled()) log_disable();
    library_force_empty();
}

int main(int argc, char **argv)
{
    TRACE("main() entered");
    SifInitRpc(0);
    ChangeThreadPriority(GetThreadId(), 0x10);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER |
                 SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER |
                 SDL_INIT_AUDIO) != 0) {
        TRACE("SDL_Init FAILED: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("AudioC0re - PS2",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       SCR_W, SCR_H, 0);
    if (!win) { TRACE("SDL_CreateWindow FAILED"); return 1; }
    g_ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_ren) g_ren = SDL_CreateRenderer(win, -1, 0);
    if (!g_ren) { TRACE("SDL_CreateRenderer FAILED"); return 1; }
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, SCR_W, SCR_H);
    if (!g_tex) { TRACE("SDL_CreateTexture FAILED"); return 1; }
    g_fb = (u32 *)calloc(SCR_W * SCR_H, 4);
    if (!g_fb) { TRACE("calloc FAILED"); return 1; }

    config_load();
    lang_init();
    lang_set(g_cfg.lang);

    TRACE("input_init...");
    input_init();
    TRACE("input_init done");

    TRACE("splash_show...");
    splash_show(g_fb, present_frame);
    TRACE("splash_show done");

    TRACE("audio_init...");
    audio_init();
    TRACE("audio_init done");

    if (g_cfg.source >= 0) {
        TRACE("saved source index=%d", g_cfg.source);
        library_set_source(g_cfg.source);
    }

    TRACE("library_scan...");
    library_scan();
    TRACE("library_scan done, count=%d, err=%d",
          library_count(), library_last_error());

    const char *root0 = library_root();
    if (root0 && root0[0]) {
        char prefix[16] = "";
        const char *colon = strchr(root0, ':');
        if (colon) {
            int n = (int)(colon - root0) + 1;
            if (n < (int)sizeof(prefix) - 1 && root0[n] == '/') n++;
            memcpy(prefix, root0, n);
            prefix[n] = 0;
        }
        if (prefix[0]) log_set_base(prefix);
    }

    int screen = SCREEN_MENU;

    int running = 1;
    u32 last_draw_ms = 0;
    u32 last_src_check = SDL_GetTicks();
    int last_screen = -1;
    u32 fps_t0 = SDL_GetTicks(), fps_loop = 0, fps_draws = 0;

    TRACE("entering main loop");

    while (running) {
        input_poll();
        fps_loop++;
        u32 now = SDL_GetTicks();

        int should_draw = 0;
        if (screen != last_screen) should_draw = 1;
        if (input_pressed(0xFFFFFFFFu)) should_draw = 1;
        if (screen == SCREEN_PLAYER   && (now - last_draw_ms) >= 16) should_draw = 1;
        if (screen == SCREEN_MENU     && (now - last_draw_ms) >= 40) should_draw = 1;
        if (screen == SCREEN_SETTINGS && (now - last_draw_ms) >= 33) should_draw = 1;
        if (screen == SCREEN_PLAYER &&
            (audio_state() == AUDIO_LOADING || audio_state() == AUDIO_ERROR))
            should_draw = 1;

        if (now - fps_t0 >= 5000) {
            u32 e = now - fps_t0;
            TRACE("FPS: loop=%u draw=%u", fps_loop*1000/e, fps_draws*1000/e);
            fps_loop = 0; fps_draws = 0; fps_t0 = now;
        }

        if (audio_device_lost()) {
            TRACE("audio device lost");
            audio_shutdown(); audio_init(); library_scan();
        }

        if ((now - last_src_check) >= 2000 && screen != SCREEN_PLAYER) {
            last_src_check = now;
            if (library_count() > 0 && library_source_gone()) {
                TRACE("source gone detected on menu");
                handle_source_gone();
                should_draw = 1;
            }
        }

        if (input_pressed(BTN_START)) { running = 0; break; }

        if (should_draw) {
            fps_draws++;
            if (screen == SCREEN_SETTINGS) {
                int r = settings_menu_run(g_fb);
                if (r == 0) screen = SCREEN_MENU;
                else if (r == 1) {
                    lang_set(g_cfg.lang);
                    TRACE("settings saved");
                    library_scan();
                    if (library_source_gone()) handle_source_gone();
                    screen = SCREEN_MENU;
                }
            } else if (library_count() == 0) {
                if (input_pressed(BTN_TRIANGLE)) screen = SCREEN_SETTINGS;
                else if (menu_run_empty(g_fb)) {
                    library_scan();
                    if (library_source_gone()) handle_source_gone();
                    last_src_check = SDL_GetTicks();
                }
            } else if (screen == SCREEN_MENU) {
                if (input_pressed(BTN_TRIANGLE)) screen = SCREEN_SETTINGS;
                else if (input_pressed(BTN_SQUARE)) {
                    library_scan();
                    if (library_source_gone()) handle_source_gone();
                    last_src_check = SDL_GetTicks();
                } else {
                    int sel = menu_run(g_fb);
                    if (sel >= 0) {
                        if (library_source_gone()) {
                            handle_source_gone();
                        } else {
                            if (player_start(sel) == 0) screen = SCREEN_PLAYER;
                        }
                    }
                }
            } else if (screen == SCREEN_PLAYER) {
                int pr = player_run(g_fb);
                if (pr == PLAYER_EXIT) {
                    audio_stop();
                    library_scan();
                    if (library_source_gone()) handle_source_gone();
                    last_src_check = SDL_GetTicks();
                    screen = SCREEN_MENU;
                }
            }

            present_frame();
            last_draw_ms = now;
        }

        last_screen = screen;
        SDL_Delay(4);
    }

    TRACE("exiting main loop");

    audio_shutdown();
    free(g_fb);
    SDL_DestroyTexture(g_tex);
    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
