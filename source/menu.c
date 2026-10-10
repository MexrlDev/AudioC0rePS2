#include <stdio.h>
#include <SDL2/SDL.h>
#include "menu.h"
#include "library.h"
#include "input.h"
#include "ui.h"
#include "lang.h"
#include "log.h"

#define REPEAT_DELAY_MS 400
#define REPEAT_RATE_MS  80

static int cursor = 0, scroll = 0;
static u32 rpt_up_t = 0, rpt_down_t = 0;
static u32 g_toast_until = 0;
static char g_toast[48];

static void set_toast(const char *s, u32 ms)
{
    strncpy(g_toast, s, sizeof(g_toast) - 1);
    g_toast[sizeof(g_toast) - 1] = 0;
    g_toast_until = SDL_GetTicks() + ms;
}

static int is_rtl_text(const char *s)
{
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c >= 0xD8 && c <= 0xDB) return 1;
        s++;
    }
    return 0;
}

static void draw_marquee(u32 *fb, int x, int y, int w,
                         const char *s, u32 col, int scale, u32 t)
{
    if (!s || !s[0]) return;
    int tw = ui_str_w(s, scale);
    if (tw <= w) { ui_str(fb, x, y, s, col, scale); return; }

    int cw = 8 * scale, gap = 24 * scale, loop = tw + gap;
    int off = (int)((t / 30) % (u32)loop);
    int rtl = is_rtl_text(s);

    for (int copy = 0; copy < 2; copy++) {
        int base = rtl ? (x + off - copy * loop) : (x - off + copy * loop);
        const char *p = s; int i = 0;
        while (*p) {
            int px = base + i * cw;
            if (!rtl && px >= x + w) break;
            if (rtl && px < x - cw) break;
            if (px >= x && px + cw <= x + w) {
                char b[2] = { *p, 0 };
                ui_str(fb, px, y, b, col, scale);
            }
            p++; i++;
        }
    }
}

void menu_toggle_log(void)
{
    if (log_enabled()) { log_disable(); set_toast(lang_str(L_LOG_OFF), 1500); }
    else {
        if (log_enable() == 0) set_toast(lang_str(L_LOG_ON), 1500);
        else                   set_toast("LOG ERR", 1500);
    }
}

void menu_draw(u32 *fb)
{
    ui_clear(fb, RGB(12, 12, 16));
    int n = library_count();

    if (cursor >= n) cursor = n > 0 ? n - 1 : 0;
    if (cursor < 0)  cursor = 0;

    ui_fill(fb, 0, 0, SCR_W, 60, RGB(24, 24, 32));
    ui_fill(fb, 0, 60, SCR_W, 3, RGB(200, 30, 30));
    ui_str(fb, 20, 22, "AudioC0re", RGB(255, 255, 255), 3);
    ui_str(fb, 20 + ui_str_w("AudioC0re ", 3), 22, "PS2", RGB(200, 30, 30), 3);

    char cnt[64];
    snprintf(cnt, sizeof(cnt), "%d %s", n, lang_str(L_TRACKS));
    ui_str_right(fb, SCR_W - 20, 26, cnt, RGB(160, 160, 180), 2);

    const char *root = library_root();
    if (root && root[0]) {
        char buf[96];
        snprintf(buf, sizeof(buf), lang_str(L_SOURCE_FMT), root);
        ui_str(fb, 20, 70, buf, RGB(120, 120, 140), 1);
    }

    const int row_h = 24, top_y = 92, bottom = SCR_H - 44;
    int vis_rows = (bottom - top_y) / row_h;
    if (cursor < scroll) scroll = cursor;
    if (cursor >= scroll + vis_rows) scroll = cursor - vis_rows + 1;

    const int title_x = 24, artist_w = 160;
    int title_w = SCR_W - title_x - 24 - artist_w - 8;
    u32 t = SDL_GetTicks();

    for (int i = 0; i < vis_rows; i++) {
        int idx = scroll + i; if (idx >= n) break;
        int y = top_y + i * row_h;
        song_info_t *s = library_get(idx);
        if (!s) break;
        if (idx == cursor) {
            ui_fill(fb, 8, y - 2, SCR_W - 16, row_h, RGB(60, 20, 20));
            ui_fill(fb, 8, y - 2, 4, row_h, RGB(220, 40, 40));
        }
        u32 tcol = (idx == cursor) ? RGB(255,255,255) : RGB(220,220,230);
        u32 acol = (idx == cursor) ? RGB(200,200,210) : RGB(140,140,160);
        draw_marquee(fb, title_x, y, title_w, s->title, tcol, 2, t);
        if (s->artist[0]) ui_str_right(fb, SCR_W - 24, y + 4, s->artist, acol, 1);
    }

    ui_str(fb, 20, SCR_H - 30, lang_str(L_MENU_HELP), RGB(120, 120, 140), 1);
}

int menu_run(u32 *fb)
{
    int n = library_count();
    if (n == 0) { menu_draw(fb); return -1; }

    u32 now = SDL_GetTicks();

    if (input_pressed(BTN_UP)) {
        cursor = (cursor - 1 + n) % n;
        rpt_up_t = now + REPEAT_DELAY_MS;
    } else if (input_held(BTN_UP) && rpt_up_t && now >= rpt_up_t) {
        cursor = (cursor - 1 + n) % n;
        rpt_up_t = now + REPEAT_RATE_MS;
    } else if (!input_held(BTN_UP)) rpt_up_t = 0;

    if (input_pressed(BTN_DOWN)) {
        cursor = (cursor + 1) % n;
        rpt_down_t = now + REPEAT_DELAY_MS;
    } else if (input_held(BTN_DOWN) && rpt_down_t && now >= rpt_down_t) {
        cursor = (cursor + 1) % n;
        rpt_down_t = now + REPEAT_RATE_MS;
    } else if (!input_held(BTN_DOWN)) rpt_down_t = 0;

    if (input_pressed(BTN_R1)) { cursor += 8; if (cursor >= n) cursor %= n; }
    if (input_pressed(BTN_L1)) { cursor -= 8; if (cursor < 0)  cursor += n; }

    if (input_pressed(BTN_SELECT)) menu_toggle_log();

    menu_draw(fb);

    if (g_toast[0] && SDL_GetTicks() < g_toast_until) {
        int w = ui_str_w(g_toast, 2) + 24;
        int x = (SCR_W - w) / 2;
        ui_fill(fb, x, SCR_H - 76, w, 30, RGB(30, 60, 30));
        ui_frame(fb, x, SCR_H - 76, w, 30, RGB(80, 220, 80), 2);
        ui_str(fb, x + 12, SCR_H - 68, g_toast, RGB(220, 255, 220), 2);
    }

    if (input_pressed(BTN_CROSS)) return cursor;
    return -1;
}

int menu_run_empty(u32 *fb)
{
    int err = library_last_error();
    const char *title = lang_str(L_NO_MUSIC);
    const char *body = "";

    switch (err) {
    case 2: title = lang_str(L_FOLDER_EMPTY);
            body = "Copy MP3/WAV files into the folder, then press X to rescan."; break;
    case 3: title = lang_str(L_NO_SOURCE);
            body = "Insert USB stick / memory card, or enable HostFS in PCSX2. Press X to retry."; break;
    }

    ui_clear(fb, RGB(12, 12, 18));
    ui_frame(fb, 20, 40, SCR_W - 40, SCR_H - 80, RGB(200, 30, 30), 3);
    ui_str_center(fb, 130, title, RGB(255, 220, 80), 3);

    int len = (int)strlen(body);
    int max_chars = (SCR_W - 100) / 16;
    char line[128]; int n = 0, ly = 220;
    while (n < len) {
        int take = (len - n > max_chars) ? max_chars : (len - n);
        if (len - n > max_chars) {
            int brk = take;
            while (brk > 0 && body[n + brk] != ' ') brk--;
            if (brk == 0) brk = take;
            take = brk;
        }
        memcpy(line, body + n, take); line[take] = 0;
        ui_str_center(fb, ly, line, RGB(220, 220, 230), 2);
        ly += 26; n += take;
        if (body[n] == ' ') n++;
        if (ly > SCR_H - 140) break;
    }

    ui_str_center(fb, SCR_H - 90, lang_str(L_MENU_HELP), RGB(160, 160, 180), 1);

    if (input_pressed(BTN_CROSS)) return 1;
    return 0;
}