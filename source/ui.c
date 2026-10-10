#include "ui.h"
#include "font8x8_basic.h"
#include "font_ext.h"

static u32 utf8_next(const char **sp)
{
    const u8 *s = (const u8 *)*sp;
    u32 c = *s++;
    if (c < 0x80) { *sp = (const char *)s; return c; }
    int n;
    if ((c & 0xE0) == 0xC0) { c &= 0x1F; n = 1; }
    else if ((c & 0xF0) == 0xE0) { c &= 0x0F; n = 2; }
    else if ((c & 0xF8) == 0xF0) { c &= 0x07; n = 3; }
    else { *sp = (const char *)s; return '?'; }
    for (int i = 0; i < n; i++) {
        if ((*s & 0xC0) != 0x80) { *sp = (const char *)s; return '?'; }
        c = (c << 6) | (*s++ & 0x3F);
    }
    *sp = (const char *)s;
    return c;
}

/* Zero-width / invisible codepoints — skip with no advance. */
static int is_zero_width(u32 cp)
{
    if (cp < 0x20) return 1;                       /* C0 controls */
    if (cp == 0x7F) return 1;                      /* DEL */
    if (cp == 0x00AD) return 1;                    /* soft hyphen */
    if (cp == 0x200B) return 1;                    /* zero-width space */
    if (cp == 0x200C) return 1;                    /* zero-width non-joiner */
    if (cp == 0x200D) return 1;                    /* zero-width joiner */
    if (cp == 0x200E) return 1;                    /* LTR mark */
    if (cp == 0x200F) return 1;                    /* RTL mark */
    if (cp >= 0x202A && cp <= 0x202E) return 1;    /* bidi embedding/override */
    if (cp >= 0x2060 && cp <= 0x2064) return 1;    /* word joiner + invisible ops */
    if (cp >= 0x2066 && cp <= 0x2069) return 1;    /* bidi isolates */
    if (cp == 0xFEFF) return 1;                    /* BOM / ZWNBSP */
    if (cp == 0x180E) return 1;                    /* Mongolian vowel sep */
    return 0;
}

/* Wide / special spaces — advance one cell but draw nothing. */
static int is_wide_space(u32 cp)
{
    if (cp == 0x00A0) return 1;                    /* NBSP */
    if (cp >= 0x2000 && cp <= 0x200A) return 1;    /* en/em/thin/hair spaces */
    if (cp == 0x2028) return 1;                    /* line separator */
    if (cp == 0x2029) return 1;                    /* paragraph separator */
    if (cp == 0x202F) return 1;                    /* narrow no-break */
    if (cp == 0x205F) return 1;                    /* medium math space */
    if (cp == 0x3000) return 1;                    /* ideographic space */
    if (cp == 0x1680) return 1;                    /* ogham space */
    return 0;
}

static const u8 *font_lookup(u32 cp)
{
    if (cp >= 32 && cp <= 126) return (const u8 *)font8x8_basic[cp];
    for (int i = 0; i < font_ext_count; i++)
        if (font_ext[i].cp == cp) return font_ext[i].g;
    return NULL;
}

void ui_clear(u32 *fb, u32 c) { u32 n = SCR_W * SCR_H; for (u32 i = 0; i < n; i++) fb[i] = c; }

void ui_fill(u32 *fb, int x, int y, int w, int h, u32 c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        u32 *row = fb + (y + j) * SCR_W + x;
        for (int i = 0; i < w; i++) row[i] = c;
    }
}

void ui_frame(u32 *fb, int x, int y, int w, int h, u32 c, int t) {
    ui_fill(fb, x, y, w, t, c); ui_fill(fb, x, y+h-t, w, t, c);
    ui_fill(fb, x, y, t, h, c); ui_fill(fb, x+w-t, y, t, h, c);
}

static void draw_glyph(u32 *fb, int x, int y, const u8 *g, u32 col, int s) {
    for (int row = 0; row < 8; row++) {
        u8 bits = g[row];
        for (int bit = 0; bit < 8; bit++) {
            if (!(bits & (1u << bit))) continue;
            int px0 = x + bit * s, py0 = y + row * s;
            for (int dy = 0; dy < s; dy++) {
                int py = py0 + dy; if (py < 0 || py >= SCR_H) continue;
                u32 *rp = fb + py * SCR_W;
                for (int dx = 0; dx < s; dx++) {
                    int px = px0 + dx;
                    if (px >= 0 && px < SCR_W) rp[px] = col;
                }
            }
        }
    }
}

void ui_str(u32 *fb, int x, int y, const char *s, u32 col, int scl) {
    if (scl < 1) scl = 1;
    while (*s) {
        u32 cp = utf8_next(&s);
        if (is_zero_width(cp)) continue;
        if (is_wide_space(cp)) { x += 8 * scl; continue; }
        const u8 *g = font_lookup(cp);
        if (g) draw_glyph(fb, x, y, g, col, scl);
        else {
            static const u8 box[8] = {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF};
            draw_glyph(fb, x, y, box, col, scl);
        }
        x += 8 * scl;
    }
}

static int utf8_len(const char *s) {
    int n = 0;
    while (*s) {
        u32 cp = utf8_next(&s);
        if (is_zero_width(cp)) continue;
        n++;
    }
    return n;
}

int ui_str_w(const char *s, int scl) { return utf8_len(s) * 8 * scl; }

void ui_str_center(u32 *fb, int y, const char *s, u32 col, int scl) {
    ui_str(fb, (SCR_W - ui_str_w(s, scl)) / 2, y, s, col, scl);
}

void ui_str_right(u32 *fb, int xr, int y, const char *s, u32 col, int scl) {
    ui_str(fb, xr - ui_str_w(s, scl), y, s, col, scl);
}

void ui_blit_bgra(u32 *fb, int x, int y, const u32 *src, int sw, int sh) {
    for (int j = 0; j < sh; j++) {
        int py = y + j; if (py < 0 || py >= SCR_H) continue;
        const u32 *sp = src + (u32)j * sw;
        u32 *dp = fb + py * SCR_W;
        for (int i = 0; i < sw; i++) {
            int px = x + i; if (px < 0 || px >= SCR_W) continue;
            u32 v = sp[i]; u32 a = (v >> 24) & 0xFF;
            if (a == 0) continue;
            if (a == 255) { dp[px] = v | 0xFF000000u; continue; }
            u32 d = dp[px];
            u32 dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
            u32 sr = (v >> 16) & 0xFF, sg = (v >> 8) & 0xFF, sb = v & 0xFF;
            u32 r = (sr * a + dr * (255 - a)) / 255;
            u32 g = (sg * a + dg * (255 - a)) / 255;
            u32 b = (sb * a + db * (255 - a)) / 255;
            dp[px] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

void ui_blit_bgra_scaled(u32 *fb, int x, int y, const u32 *src, int sw, int sh, int dw, int dh) {
    for (int dy = 0; dy < dh; dy++) {
        int py = y + dy; if (py < 0 || py >= SCR_H) continue;
        int sy = dy * sh / dh; if (sy >= sh) sy = sh - 1;
        const u32 *sp = src + (u32)sy * sw;
        u32 *dp = fb + py * SCR_W;
        for (int dx = 0; dx < dw; dx++) {
            int px = x + dx; if (px < 0 || px >= SCR_W) continue;
            int sx = dx * sw / dw; if (sx >= sw) sx = sw - 1;
            u32 v = sp[sx]; u32 a = (v >> 24) & 0xFF;
            if (a == 0) continue;
            if (a == 255) { dp[px] = v | 0xFF000000u; continue; }
            u32 d = dp[px];
            u32 dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
            u32 sr = (v >> 16) & 0xFF, sg = (v >> 8) & 0xFF, sb = v & 0xFF;
            u32 r = (sr * a + dr * (255 - a)) / 255;
            u32 g = (sg * a + dg * (255 - a)) / 255;
            u32 b = (sb * a + db * (255 - a)) / 255;
            dp[px] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}