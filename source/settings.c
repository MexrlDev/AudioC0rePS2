#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "settings.h"
#include "library.h"
#include "input.h"
#include "ui.h"
#include "lang.h"
#include "config.h"
#include "log.h"

enum { ST_MAIN, ST_SOURCE, ST_LANG, ST_CONFIRM_RESET };

static int  st  = ST_MAIN;
static int  cur = 0;
static char toast[64];
static u32  toast_until = 0;

#define ROW_H 44
#define TOP_Y 128

static void toast_set(const char *s, u32 ms)
{
    strncpy(toast, s, sizeof(toast) - 1);
    toast[sizeof(toast) - 1] = 0;
    toast_until = SDL_GetTicks() + ms;
}

static void draw_header(u32 *fb, const char *title)
{
    ui_fill(fb, 0, 0, SCR_W, 60, RGB(24, 24, 32));
    ui_fill(fb, 0, 60, SCR_W, 3, RGB(200, 30, 30));
    ui_str(fb, 20, 22, title, RGB(255, 255, 255), 3);
}

static void draw_row(u32 *fb, int y, const char *label, const char *value,
                     int selected, int wrap_h)
{
    if (selected) {
        ui_fill(fb, 8, y - 4, SCR_W - 16, wrap_h, RGB(60, 20, 20));
        ui_fill(fb, 8, y - 4, 4, wrap_h, RGB(220, 40, 40));
    }
    u32 lc = selected ? RGB(255,255,255) : RGB(220,220,230);
    ui_str(fb, 34, y + 6, label, lc, 2);
    if (value && value[0])
        ui_str_right(fb, SCR_W - 34, y + 6, value, RGB(160, 200, 240), 2);
}

static void draw_toast(u32 *fb)
{
    if (!toast[0] || SDL_GetTicks() >= toast_until) return;
    int w = ui_str_w(toast, 2) + 24;
    int x = (SCR_W - w) / 2;
    ui_fill(fb, x, SCR_H - 76, w, 30, RGB(30, 60, 30));
    ui_frame(fb, x, SCR_H - 76, w, 30, RGB(80, 220, 80), 2);
    ui_str(fb, x + 12, SCR_H - 68, toast, RGB(220, 255, 220), 2);
}

int settings_menu_run(u32 *fb)
{
    /* ---------- Reset confirmation ---------- */
    if (st == ST_CONFIRM_RESET) {
        ui_clear(fb, RGB(12, 12, 18));
        ui_frame(fb, 20, 40, SCR_W - 40, SCR_H - 80, RGB(200, 30, 30), 3);
        ui_str_center(fb, 130, lang_str(L_CONFIRM_RESET), RGB(255, 220, 80), 3);
        ui_str_center(fb, 280, lang_str(L_SET_HELP_BACK), RGB(200, 200, 220), 2);

        if (input_pressed(BTN_CROSS)) {
            config_defaults();
            lang_set(g_cfg.lang);
            cur = 0;
            st = ST_MAIN;
            toast_set(lang_str(L_RESET_DONE), 1500);
        }
        if (input_pressed(BTN_CIRCLE)) st = ST_MAIN;
        return 2;
    }

    /* ---------- Source picker ---------- */
    if (st == ST_SOURCE) {
        int n = library_source_count();
        if (input_pressed(BTN_DOWN) && cur < n - 1) cur++;
        if (input_pressed(BTN_UP)   && cur > 0)     cur--;

        ui_clear(fb, RGB(12, 12, 18));
        draw_header(fb, lang_str(L_SET_SOURCE));
        ui_str_center(fb, 78, lang_str(L_SET_HELP_SEL), RGB(120,120,140), 1);

        for (int i = 0; i < n; i++) {
            int y = TOP_Y + i * ROW_H;
            int avail = library_source_available(i);
            int is_cur = (library_source_current() == i);

            if (i == cur)
                ui_fill(fb, 8, y - 4, SCR_W - 16, ROW_H - 6, RGB(60, 20, 20));

            u32 cbox = avail ? RGB(80, 220, 80) : RGB(120, 60, 60);
            u32 ctxt = avail ? RGB(255, 255, 255) : RGB(140, 140, 140);
            ui_str(fb, 24, y + 4, avail ? "[X]" : "[ ]", cbox, 2);

            char lbl[64];
            snprintf(lbl, sizeof(lbl), "%s%s",
                     library_source_label(i), is_cur ? "  *" : "");
            ui_str(fb, 80, y + 4, lbl, ctxt, 2);
            ui_str(fb, 80, y + 24, library_source_root(i), RGB(140, 140, 160), 1);
        }

        if (input_pressed(BTN_CIRCLE)) { st = ST_MAIN; return 2; }
        if (input_pressed(BTN_CROSS)) {
            if (library_source_available(cur)) {
                library_set_source(cur);
                st = ST_MAIN;
            }
        }
        return 2;
    }

    /* ---------- Language picker ---------- */
    if (st == ST_LANG) {
        int n = lang_count();
        if (input_pressed(BTN_DOWN) && cur < n - 1) cur++;
        if (input_pressed(BTN_UP)   && cur > 0)     cur--;

        ui_clear(fb, RGB(12, 12, 18));
        draw_header(fb, lang_str(L_SET_LANG));
        ui_str_center(fb, 78, lang_str(L_SET_HELP_SEL), RGB(120,120,140), 1);

        for (int i = 0; i < n; i++) {
            int y = TOP_Y + i * ROW_H;
            if (i == cur)
                ui_fill(fb, 8, y - 4, SCR_W - 16, ROW_H - 6, RGB(60, 20, 20));
            u32 c = (i == cur) ? RGB(255,255,255) : RGB(220,220,230);
            if (i == lang_current()) c = RGB(240, 220, 80);
            ui_str(fb, 44, y + 8, lang_name(i), c, 2);
            if (i == lang_current())
                ui_str_right(fb, SCR_W - 40, y + 8, "*", RGB(240, 220, 80), 2);
        }

        if (input_pressed(BTN_CIRCLE)) { st = ST_MAIN; return 2; }
        if (input_pressed(BTN_CROSS)) { lang_set(cur); st = ST_MAIN; }
        return 2;
    }

    /* ---------- Main settings list ---------- */
    enum { ROW_SOURCE, ROW_LANG, ROW_RESET, ROW_SAVE, ROW_CANCEL, ROW_COUNT };
    int n = ROW_COUNT;
    if (input_pressed(BTN_DOWN) && cur < n - 1) cur++;
    if (input_pressed(BTN_UP)   && cur > 0)     cur--;

    ui_clear(fb, RGB(12, 12, 18));
    draw_header(fb, lang_str(L_SETTINGS));

    char src_lbl[64] = "(none)";
    int src_i = library_source_current();
    if (src_i >= 0)
        snprintf(src_lbl, sizeof(src_lbl), "%s", library_source_label(src_i));

    char lang_lbl[32];
    snprintf(lang_lbl, sizeof(lang_lbl), "%s", lang_name(lang_current()));

    for (int i = 0; i < n; i++) {
        int y = TOP_Y + i * ROW_H;
        const char *label = "";
        const char *value = NULL;
        switch (i) {
        case ROW_SOURCE: label = lang_str(L_SET_SOURCE); value = src_lbl;  break;
        case ROW_LANG:   label = lang_str(L_SET_LANG);   value = lang_lbl; break;
        case ROW_RESET:  label = lang_str(L_SET_RESET);  value = "";       break;
        case ROW_SAVE:   label = lang_str(L_SET_SAVE);   value = "";       break;
        case ROW_CANCEL: label = lang_str(L_SET_CANCEL); value = "";       break;
        }
        draw_row(fb, y, label, value, i == cur, ROW_H - 6);
    }

    ui_str(fb, 20, SCR_H - 30, lang_str(L_SET_HELP_BACK),
           RGB(120, 120, 140), 1);

    draw_toast(fb);

    if (input_pressed(BTN_CIRCLE)) return 0;

    if (input_pressed(BTN_START)) {
        g_cfg.source = library_source_current();
        g_cfg.lang   = lang_current();
        if (config_save() == 0) { toast_set(lang_str(L_SAVED), 1200); return 1; }
        toast_set(lang_str(L_SAVE_FAILED), 1500);
        return 2;
    }

    if (input_pressed(BTN_CROSS)) {
        switch (cur) {
        case ROW_SOURCE:
            cur = library_source_current();
            if (cur < 0) cur = 0;
            st = ST_SOURCE; break;
        case ROW_LANG:
            cur = lang_current();
            st = ST_LANG; break;
        case ROW_RESET:
            st = ST_CONFIRM_RESET; break;
        case ROW_SAVE:
            g_cfg.source = library_source_current();
            g_cfg.lang   = lang_current();
            if (config_save() == 0) { toast_set(lang_str(L_SAVED), 1200); return 1; }
            toast_set(lang_str(L_SAVE_FAILED), 1500);
            break;
        case ROW_CANCEL:
            return 0;
        }
    }

    return 2;
}
