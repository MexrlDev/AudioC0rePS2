#include <stdio.h>
#include <SDL2/SDL.h>
#include "player.h"
#include "audio.h"
#include "library.h"
#include "metadata.h"
#include "input.h"
#include "ui.h"
#include "lang.h"

#define COVER_W 240
#define COVER_H 240
#define REPEAT_DELAY_MS  400
#define REPEAT_RATE_MS   100
#define SRC_CHECK_MS     250

static int      s_idx = -1;
static u32     *s_cover = NULL;
static char     s_status[48];
static u32      s_status_until = 0;
static int      s_loop = 0;
static int      s_auto = 0;
static int      s_ended_handled = 0;
static u32      s_last_src_check = 0;

static u32 rpt_right_t = 0, rpt_left_t = 0;
static u32 rpt_up_t    = 0, rpt_down_t = 0;

static int is_rtl_text(const char *s)
{
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c >= 0xD8 && c <= 0xDB) return 1;
        s++;
    }
    return 0;
}

static void draw_fit(u32 *fb, int x, int y, const char *s, u32 col,
                     int pref_scale, int max_px)
{
    if (!s || !s[0]) return;
    if (ui_str_w(s, pref_scale) <= max_px) { ui_str(fb, x, y, s, col, pref_scale); return; }
    for (int sc = pref_scale - 1; sc >= 1; sc--)
        if (ui_str_w(s, sc) <= max_px) { ui_str(fb, x, y, s, col, sc); return; }
    int mc = max_px / 8; if (mc < 4) mc = 4;
    int n = (int)strlen(s);
    char buf[128];
    if (n > mc) { int k = mc - 3; if (k < 1) k = 1;
        memcpy(buf, s, k); buf[k]='.'; buf[k+1]='.'; buf[k+2]='.'; buf[k+3]=0; }
    else { memcpy(buf, s, n); buf[n]=0; }
    ui_str(fb, x, y, buf, col, 1);
}

static void draw_marquee(u32 *fb, int x, int y, int w,
                         const char *s, u32 col, int scale, u32 t)
{
    if (!s || !s[0]) return;
    int tw = ui_str_w(s, scale);
    if (tw <= w) { ui_str(fb, x, y, s, col, scale); return; }
    int cw = 8 * scale, gap = 32 * scale, loop = tw + gap;
    int off = (int)((t / 25) % (u32)loop);
    int rtl = is_rtl_text(s);
    for (int copy = 0; copy < 2; copy++) {
        int base = rtl ? (x + off - copy * loop)
                       : (x - off + copy * loop);
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

static void set_status(const char *s, u32 ms)
{
    strncpy(s_status, s, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = 0;
    s_status_until = SDL_GetTicks() + ms;
}

int player_queue_on(void) { return s_auto; }

int player_start(int idx)
{
    s_idx = idx;
    if (s_cover) { free(s_cover); s_cover = NULL; }
    s_cover = metadata_read_cover(library_path(idx), COVER_W, COVER_H);
    audio_open(library_path(idx));
    s_status[0] = 0;
    s_status_until = 0;
    s_ended_handled = 0;
    s_last_src_check = 0;
    rpt_right_t = rpt_left_t = rpt_up_t = rpt_down_t = 0;
    return 0;
}

static void fmt_time(char *out, u32 ms)
{
    u32 s = ms / 1000, m = s / 60; s %= 60;
    int p = 0;
    if (m >= 100) p += sprintf(out + p, "%u", m);
    else          p += sprintf(out + p, "%02u", m);
    out[p++] = ':';
    p += sprintf(out + p, "%02u", s);
    out[p] = 0;
}

static void adjust_volume(int delta)
{
    if (audio_get_mute()) audio_set_mute(0);
    int v = audio_get_volume() + delta;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    audio_set_volume(v);
    char buf[32]; snprintf(buf, sizeof(buf), "%s %d%%", lang_str(L_VOLUME), v);
    set_status(buf, 900);
}

static int do_skip(int sign, u32 step_ms, const char *tag)
{
    u32 pos = audio_position_ms(), dur = audio_duration_ms();
    if (sign > 0) {
        if (dur == 0) return 0;
        if (pos + step_ms >= dur) { set_status(lang_str(L_END_SONG), 900); return 0; }
        audio_seek_ms(pos + step_ms);
    } else {
        if (pos == 0) { set_status(lang_str(L_START_SONG), 900); return 0; }
        if (pos > step_ms) audio_seek_ms(pos - step_ms);
        else               audio_seek_ms(0);
    }
    char buf[32]; snprintf(buf, sizeof(buf), "%s%us", tag, (unsigned)(step_ms / 1000));
    set_status(buf, 900);
    return 1;
}

int player_run(u32 *fb)
{
    if (s_idx < 0) return PLAYER_EXIT;

    int st = audio_state();

    if (st == AUDIO_LOADING) {
        u32 now_t = SDL_GetTicks();
        if (s_last_src_check == 0 || (now_t - s_last_src_check) >= SRC_CHECK_MS) {
            s_last_src_check = now_t;
            if (library_source_gone()) {
                audio_cancel_load();
                return PLAYER_EXIT;
            }
        }

        ui_clear(fb, RGB(12, 12, 16));
        {
            static const char *dots[3] = { "Loading.", "Loading..", "Loading..." };
            int d = (int)((SDL_GetTicks() / 400) % 3);
            ui_str_center(fb, SCR_H/2 - 20, dots[d], RGB(220, 220, 230), 3);
        }
        const char *nm = library_path(s_idx);
        const char *base = strrchr(nm, '/'); if (base) base++; else base = nm;
        draw_fit(fb, 20, SCR_H/2 + 20, base, RGB(140, 140, 160), 2, SCR_W - 40);
        ui_str_center(fb, SCR_H - 40, "O: Cancel", RGB(120, 120, 140), 1);
        if (input_pressed(BTN_CIRCLE)) {
            audio_cancel_load();
            return PLAYER_EXIT;
        }
        return PLAYER_STAY;
    }
    if (st == AUDIO_ERROR) {
        ui_clear(fb, RGB(12, 12, 16));
        ui_str_center(fb, SCR_H/2 - 20, lang_str(L_LOAD_FAILED), RGB(240, 120, 120), 3);
        const char *nm = library_path(s_idx);
        const char *base = strrchr(nm, '/'); if (base) base++; else base = nm;
        draw_fit(fb, 20, SCR_H/2 + 20, base, RGB(180, 180, 200), 2, SCR_W - 40);
        ui_str_center(fb, SCR_H - 40, "X/O: Back", RGB(120, 120, 140), 1);
        if (input_pressed(BTN_CROSS) || input_pressed(BTN_CIRCLE))
            return PLAYER_EXIT;
        return PLAYER_STAY;
    }

    u32 now = SDL_GetTicks();

    if (s_ended_handled && !audio_at_end()) s_ended_handled = 0;

    if (input_pressed(BTN_CIRCLE)) return PLAYER_EXIT;

    if (input_pressed(BTN_CROSS)) {
        if (audio_is_playing()) { audio_pause(); set_status(lang_str(L_PAUSED), 1200); }
        else                    { audio_resume(); set_status(lang_str(L_PLAYING), 1200); }
    }

    if (input_pressed(BTN_SQUARE)) {
        if (audio_get_mute()) { audio_set_mute(0); set_status(lang_str(L_UNMUTED), 900); }
        else                  { audio_set_mute(1); set_status(lang_str(L_MUTED), 900); }
    }

    if (input_pressed(BTN_TRIANGLE)) {
        s_loop = !s_loop;
        set_status(s_loop ? lang_str(L_LOOP_ON) : lang_str(L_LOOP_OFF), 900);
    }

    if (input_pressed(BTN_SELECT)) {
        s_auto = !s_auto;
        set_status(s_auto ? lang_str(L_AUTO_ON) : lang_str(L_AUTO_OFF), 1200);
    }

    if (input_pressed(BTN_R1)) {
        int n = library_count();
        if (n > 1) {
            if (library_source_gone()) return PLAYER_EXIT;
            s_idx = (s_idx + 1) % n;
            player_start(s_idx);
            return PLAYER_STAY;
        }
    }
    if (input_pressed(BTN_L1)) {
        int n = library_count();
        if (n > 1) {
            if (library_source_gone()) return PLAYER_EXIT;
            s_idx = (s_idx - 1 + n) % n;
            player_start(s_idx);
            return PLAYER_STAY;
        }
    }

    if (input_pressed(BTN_RIGHT)) {
        do_skip(+1, 5000, lang_str(L_SEEK_PLUS));
        rpt_right_t = now + REPEAT_DELAY_MS;
    } else if (input_held(BTN_RIGHT) && rpt_right_t && now >= rpt_right_t) {
        if (do_skip(+1, 10000, lang_str(L_SEEK_PLUS))) rpt_right_t = now + 250;
        else                                           rpt_right_t = now + 900;
    } else if (!input_held(BTN_RIGHT)) rpt_right_t = 0;

    if (input_pressed(BTN_LEFT)) {
        do_skip(-1, 5000, lang_str(L_SEEK_MINUS));
        rpt_left_t = now + REPEAT_DELAY_MS;
    } else if (input_held(BTN_LEFT) && rpt_left_t && now >= rpt_left_t) {
        if (do_skip(-1, 10000, lang_str(L_SEEK_MINUS))) rpt_left_t = now + 250;
        else                                            rpt_left_t = now + 900;
    } else if (!input_held(BTN_LEFT)) rpt_left_t = 0;

    if (input_pressed(BTN_UP)) { adjust_volume(+2); rpt_up_t = now + REPEAT_DELAY_MS; }
    else if (input_held(BTN_UP) && rpt_up_t && now >= rpt_up_t) {
        adjust_volume(+2); rpt_up_t = now + REPEAT_RATE_MS;
    } else if (!input_held(BTN_UP)) rpt_up_t = 0;

    if (input_pressed(BTN_DOWN)) { adjust_volume(-2); rpt_down_t = now + REPEAT_DELAY_MS; }
    else if (input_held(BTN_DOWN) && rpt_down_t && now >= rpt_down_t) {
        adjust_volume(-2); rpt_down_t = now + REPEAT_RATE_MS;
    } else if (!input_held(BTN_DOWN)) rpt_down_t = 0;

    if (audio_at_end() && !s_ended_handled) {
        s_ended_handled = 1;

        if (s_loop) {
            audio_seek_ms(0);
            audio_resume();
            set_status(lang_str(L_LOOP_ON), 900);
            return PLAYER_STAY;
        }
        if (s_auto) {
            int n = library_count();
            if (n > 0) {
                if (library_source_gone()) return PLAYER_EXIT;
                s_idx = (s_idx + 1) % n;
                player_start(s_idx);
                return PLAYER_STAY;
            }
        }
        return PLAYER_EXIT;
    }

    ui_clear(fb, RGB(12, 12, 16));
    ui_fill(fb, 0, 0, SCR_W, 44, RGB(24, 24, 32));
    ui_fill(fb, 0, 44, SCR_W, 3, RGB(200, 30, 30));
    ui_str(fb, 20, 12, lang_str(L_NOW_PLAYING), RGB(220, 220, 230), 2);

    song_info_t *s = library_get(s_idx);
    if (!s) return PLAYER_EXIT;

    int ix = SCR_W - 20;
    if (audio_get_mute()) { ui_str_right(fb, ix, 10, lang_str(L_MUTED), RGB(240, 100, 100), 1); ix -= 60; }
    if (s_loop)           { ui_str_right(fb, ix, 10, lang_str(L_LOOP_ON), RGB(240, 200, 60), 1); ix -= 60; }
    if (s_auto)           { ui_str_right(fb, ix, 10, lang_str(L_AUTO_ON), RGB(80, 200, 240), 1); }

    int cx = SCR_W - COVER_W - 35, cy = 80;
    if (s_cover) {
        ui_blit_bgra(fb, cx, cy, s_cover, COVER_W, COVER_H);
        ui_frame(fb, cx - 2, cy - 2, COVER_W + 4, COVER_H + 4, RGB(80, 80, 90), 1);
    } else {
        ui_fill(fb, cx, cy, COVER_W, COVER_H, RGB(28, 28, 36));
        ui_frame(fb, cx, cy, COVER_W, COVER_H, RGB(60, 60, 70), 1);
        const char *nc = lang_str(L_NO_COVER);
        int nw = ui_str_w(nc, 2);
        ui_str(fb, cx + (COVER_W - nw)/2, cy + COVER_H/2 - 8, nc, RGB(120, 120, 140), 2);
    }

    int tx = 20, max_tw = cx - 40;
    if (max_tw < 100) max_tw = 100;
    u32 now2 = SDL_GetTicks();
    draw_marquee(fb, tx, 80, max_tw, s->title,  RGB(255, 255, 255), 3, now2);
    draw_fit   (fb, tx, 130, s->artist, RGB(180, 180, 200), 2, max_tw);
    draw_fit   (fb, tx, 165, s->album,  RGB(130, 130, 150), 2, max_tw);

    const char *stext; u32 scol;
    if (now2 < s_status_until && s_status[0]) { stext = s_status; scol = RGB(240, 200, 60); }
    else if (audio_is_playing()) { stext = lang_str(L_PLAYING); scol = RGB(80, 200, 80); }
    else { stext = lang_str(L_PAUSED); scol = RGB(240, 180, 60); }
    ui_str(fb, tx, 205, stext, scol, 2);

    int bx = 20, by = 365, bw = SCR_W - 40, bh = 12;
    u32 pos = audio_position_ms(), dur = audio_duration_ms();
    ui_fill(fb, bx, by, bw, bh, RGB(50, 50, 60));
    if (dur > 0) {
        int fill = (int)((u64)bw * pos / dur);
        if (fill > bw) fill = bw;
        ui_fill(fb, bx, by, fill, bh, RGB(220, 40, 40));
    }
    ui_frame(fb, bx, by, bw, bh, RGB(90, 90, 100), 1);

    char t1[16], t2[16];
    fmt_time(t1, pos); fmt_time(t2, dur);
    ui_str(fb, bx, by - 26, t1, RGB(220, 220, 220), 2);
    ui_str_right(fb, bx + bw, by - 26, t2, RGB(220, 220, 220), 2);

    char vbuf[24];
    if (audio_get_mute()) snprintf(vbuf, sizeof(vbuf), "%s", lang_str(L_MUTED));
    else                  snprintf(vbuf, sizeof(vbuf), "%s %d%%", lang_str(L_VOLUME), audio_get_volume());
    ui_str(fb, bx, by + 20, vbuf, RGB(180, 180, 200), 2);

    ui_str(fb, 20, SCR_H - 44, lang_str(L_PLAYER_HELP1), RGB(120, 120, 140), 1);
    ui_str(fb, 20, SCR_H - 30, lang_str(L_PLAYER_HELP2), RGB(120, 120, 140), 1);

    return PLAYER_STAY;
}
