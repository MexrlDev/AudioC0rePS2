#include <SDL2/SDL.h>
#include "splash.h"
#include "ui.h"
#include "input.h"

extern const u32 logo_data[];
extern const int logo_width;
extern const int logo_height;

#define DISP_W 240
#define DISP_H 240

#define FADE_IN_MS  1200
#define HOLD_MS      500
#define FADE_OUT_MS 1200
#define TOTAL_MS    (FADE_IN_MS + HOLD_MS + FADE_OUT_MS)

static u32 lerp(u32 a, u32 b, int t)
{
    return (u32)(a + ((int)(b - a) * t) / 255);
}

void splash_show(u32 *fb, splash_present_fn present)
{
    int lx = (SCR_W - DISP_W) / 2;
    int ly = (SCR_H - DISP_H) / 2 - 20;
    const char *line = "By MexrlDev";
    int lw = ui_str_w(line, 2);
    int txt_x = (SCR_W - lw) / 2;
    int txt_y = ly + DISP_H + 12;

    u32 start = SDL_GetTicks();

    while (1) {
        u32 elapsed = SDL_GetTicks() - start;
        if (elapsed >= TOTAL_MS) break;

        input_poll();
        if (input_pressed(BTN_CROSS) || input_pressed(BTN_CIRCLE) ||
            input_pressed(BTN_SQUARE) || input_pressed(BTN_TRIANGLE) ||
            input_pressed(BTN_START)) {
            break;
        }

        int alpha = 255;
        if (elapsed < FADE_IN_MS) {
            alpha = (int)((u64)elapsed * 255 / FADE_IN_MS);
        } else if (elapsed < FADE_IN_MS + HOLD_MS) {
            alpha = 255;
        } else {
            u32 fo = elapsed - FADE_IN_MS - HOLD_MS;
            if (fo >= FADE_OUT_MS) alpha = 0;
            else alpha = 255 - (int)((u64)fo * 255 / FADE_OUT_MS);
        }

        u32 bg;
        if (elapsed < TOTAL_MS - FADE_OUT_MS) {
            bg = 0;
        } else {
            u32 t = elapsed - (TOTAL_MS - FADE_OUT_MS);
            u32 tt = t * 255 / FADE_OUT_MS;
            bg = lerp(0, 15, tt);
        }
        ui_clear(fb, RGB(bg, bg, bg));

        if (alpha > 0) {
            for (int dy = 0; dy < DISP_H; dy++) {
                int py = ly + dy;
                if (py < 0 || py >= SCR_H) continue;
                int sy = dy * logo_height / DISP_H;
                const u32 *srow = logo_data + (u32)sy * logo_width;
                u32 *drow = fb + (u32)py * SCR_W;
                for (int dx = 0; dx < DISP_W; dx++) {
                    int px = lx + dx;
                    if (px < 0 || px >= SCR_W) continue;
                    int sx = dx * logo_width / DISP_W;
                    u32 v = srow[sx];
                    u32 sa = (v >> 24) & 0xFF;
                    if (!sa) continue;
                    int eff = (int)((u64)sa * alpha / 255);
                    if (!eff) continue;
                    int inv = 255 - eff;
                    u32 sr =  v        & 0xFF;
                    u32 sg = (v >>  8) & 0xFF;
                    u32 sb = (v >> 16) & 0xFF;
                    u32 d  = drow[px];
                    u32 dr = (d >> 16) & 0xFF;
                    u32 dg = (d >>  8) & 0xFF;
                    u32 db =  d        & 0xFF;
                    u32 r = (sr * eff + dr * inv) / 255;
                    u32 g = (sg * eff + dg * inv) / 255;
                    u32 b = (sb * eff + db * inv) / 255;
                    drow[px] = 0xFF000000u | (r << 16) | (g << 8) | b;
                }
            }

            u32 base_r = 40, base_g = 110, base_b = 225;
            u32 r = base_r * alpha / 255;
            u32 g = base_g * alpha / 255;
            u32 b = base_b * alpha / 255;
            ui_str(fb, txt_x, txt_y, line,
                   0xFF000000u | (r << 16) | (g << 8) | b, 2);
        }

        if (present) present();
        SDL_Delay(16);
    }

    ui_clear(fb, RGB(0, 0, 0));
    if (present) present();
    u32 hold = SDL_GetTicks();
    while (SDL_GetTicks() - hold < 300) {
        input_poll();
        if (input_pressed(BTN_CROSS) || input_pressed(BTN_START)) break;
        SDL_Delay(16);
    }
}
