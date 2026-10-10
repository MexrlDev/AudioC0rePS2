#include "metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"

/* Open a file with retries. Needed right after a USB re-mount, when
   opendir() sees entries but fopen() hasn't caught up yet. */
static FILE *open_retry(const char *path)
{
    for (int i = 0; i < 6; i++) {
        FILE *f = fopen(path, "rb");
        if (f) return f;
        usleep(100000);   /* 100 ms between tries */
    }
    return NULL;
}

static u32 syncsafe(const u8 *b)
{
    return ((u32)(b[0] & 0x7F) << 21)
         | ((u32)(b[1] & 0x7F) << 14)
         | ((u32)(b[2] & 0x7F) << 7)
         |  (u32)(b[3] & 0x7F);
}
static u32 be32(const u8 *b)
{
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];
}

static int utf8_encode(u8 *out, u32 cp)
{
    if (cp < 0x80) { out[0] = (u8)cp; return 1; }
    if (cp < 0x800) {
        out[0] = 0xC0 | (cp >> 6);
        out[1] = 0x80 | (cp & 0x3F);
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = 0xE0 | (cp >> 12);
        out[1] = 0x80 | ((cp >> 6) & 0x3F);
        out[2] = 0x80 | (cp & 0x3F);
        return 3;
    }
    out[0] = 0xF0 | (cp >> 18);
    out[1] = 0x80 | ((cp >> 12) & 0x3F);
    out[2] = 0x80 | ((cp >> 6) & 0x3F);
    out[3] = 0x80 | (cp & 0x3F);
    return 4;
}

static void copy_text(char *dst, int dst_max, const u8 *src, int src_len, int enc)
{
    if (enc == 3) {
        int j = 0;
        for (int i = 0; i < src_len && j < dst_max - 1; i++) {
            u8 c = src[i];
            if (c == 0) break;
            dst[j++] = (char)c;
        }
        dst[j] = 0;
        while (j > 0 && dst[j-1] == ' ') dst[--j] = 0;
        return;
    }

    if (enc == 1 || enc == 2) {
        int i = 0;
        int little = 1;
        if (enc == 1 && src_len >= 2) {
            if (src[0] == 0xFE && src[1] == 0xFF) { little = 0; i = 2; }
            else if (src[0] == 0xFF && src[1] == 0xFE) { little = 1; i = 2; }
        }
        int j = 0;
        while (i + 1 < src_len && j < dst_max - 4) {
            u16 w = little
                  ? (u16)(src[i] | (src[i+1] << 8))
                  : (u16)((src[i] << 8) | src[i+1]);
            i += 2;
            if (w == 0) break;
            u32 cp = w;
            if (w >= 0xD800 && w <= 0xDBFF && i + 1 < src_len) {
                u16 lo = little
                       ? (u16)(src[i] | (src[i+1] << 8))
                       : (u16)((src[i] << 8) | src[i+1]);
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    i += 2;
                    cp = 0x10000 + (((u32)w - 0xD800) << 10) + ((u32)lo - 0xDC00);
                }
            }
            j += utf8_encode((u8 *)(dst + j), cp);
        }
        dst[j] = 0;
        while (j > 0 && dst[j-1] == ' ') dst[--j] = 0;
        return;
    }

    int j = 0;
    for (int i = 0; i < src_len && j < dst_max - 4; i++) {
        u8 c = src[i];
        if (c == 0) break;
        if (c < 0x80) {
            dst[j++] = (char)c;
        } else {
            j += utf8_encode((u8 *)(dst + j), c);
        }
    }
    dst[j] = 0;
    while (j > 0 && dst[j-1] == ' ') dst[--j] = 0;
}

static int read_id3v1(FILE *f, song_info_t *out)
{
    if (fseek(f, -128, SEEK_END) != 0) return 0;
    u8 tag[128];
    if (fread(tag, 1, 128, f) != 128) return 0;
    if (memcmp(tag, "TAG", 3) != 0) return 0;

    if (!out->title[0] || !strcmp(out->title, out->name)) {
        memset(out->title, 0, sizeof(out->title));
        copy_text(out->title, sizeof(out->title), tag + 3, 30, 0);
    }
    memset(out->artist, 0, sizeof(out->artist));
    copy_text(out->artist, sizeof(out->artist), tag + 33, 30, 0);
    memset(out->album, 0, sizeof(out->album));
    copy_text(out->album, sizeof(out->album), tag + 63, 30, 0);
    return 1;
}

typedef struct { const u8 *data; int size; int pos; int version; int id3_size; } id3_t;

static int id3_next(id3_t *t, char id[5], const u8 **body, int *body_len)
{
    if (t->pos + 10 > t->size) return 0;
    const u8 *h = t->data + t->pos;
    if (h[0] == 0) return 0;
    memcpy(id, h, 4); id[4] = 0;
    u32 sz = (t->version >= 4) ? syncsafe(h + 4) : be32(h + 4);
    if (sz == 0 || t->pos + 10 + (int)sz > t->size) return 0;
    *body = h + 10;
    *body_len = (int)sz;
    t->pos += 10 + (int)sz;
    return 1;
}

typedef struct { u8 *ptr; int len; } apic_t;

static void id3_scan(const char *path, song_info_t *out, apic_t *apic_out, int full)
{
    FILE *f = open_retry(path);
    if (!f) return;

    u8 hdr[10];
    if (fread(hdr, 1, 10, f) == 10 && memcmp(hdr, "ID3", 3) == 0) {
        int version = hdr[3];
        int flags   = hdr[5];
        int tag_sz  = (int)syncsafe(hdr + 6);
        if (version != 3 && version != 4) { fclose(f); return; }
        int cap = tag_sz;
        if (cap > 4 * 1024 * 1024) cap = 4 * 1024 * 1024;
        u8 *buf = (u8 *)malloc(cap);
        if (!buf) { fclose(f); return; }
        if (fread(buf, 1, cap, f) != (size_t)cap) { free(buf); fclose(f); return; }
        id3_t t = { buf, cap, 0, version, tag_sz };
        if (flags & 0x40) {
            u32 ext = (version >= 4) ? syncsafe(buf) : be32(buf);
            t.pos += ext;
        }
        char id[5]; const u8 *body; int len;
        while (id3_next(&t, id, &body, &len)) {
            if (len < 1) continue;
            if (!strcmp(id, "TIT2") && !out->title[0]) {
                copy_text(out->title, sizeof(out->title), body + 1, len - 1, body[0]);
            } else if (!strcmp(id, "TPE1")) {
                copy_text(out->artist, sizeof(out->artist), body + 1, len - 1, body[0]);
            } else if (!strcmp(id, "TALB")) {
                copy_text(out->album, sizeof(out->album), body + 1, len - 1, body[0]);
            } else if (full && !strcmp(id, "APIC") && apic_out && !apic_out->ptr) {
                int enc = body[0];
                int p = 1;
                while (p < len && body[p]) p++;
                p++;
                p++;
                if (enc == 1 || enc == 2) {
                    while (p + 1 < len && (body[p] || body[p+1])) p += 2;
                    p += 2;
                } else {
                    while (p < len && body[p]) p++;
                    p++;
                }
                if (p < len) {
                    int dlen = len - p;
                    u8 *d = (u8 *)malloc(dlen);
                    if (d) { memcpy(d, body + p, dlen); apic_out->ptr = d; apic_out->len = dlen; }
                }
            }
        }
        free(buf);
    }
    fclose(f);
    if (!out->artist[0] || !out->album[0]) {
        f = open_retry(path);
        if (f) { read_id3v1(f, out); fclose(f); }
    }
}

static void read_wav_info(const char *path, song_info_t *out)
{
    FILE *f = open_retry(path);
    if (!f) return;
    u8 hdr[12];
    if (fread(hdr, 1, 12, f) != 12 ||
        memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
        fclose(f); return;
    }
    for (;;) {
        u8 chdr[8];
        if (fread(chdr, 1, 8, f) != 8) break;
        u32 sz = chdr[4] | ((u32)chdr[5]<<8) | ((u32)chdr[6]<<16) | ((u32)chdr[7]<<24);
        if (!memcmp(chdr, "LIST", 4)) {
            u8 *buf = (u8*)malloc(sz);
            if (!buf) break;
            if (fread(buf, 1, sz, f) != sz) { free(buf); break; }
            if (sz >= 4 && !memcmp(buf, "INFO", 4)) {
                int p = 4;
                while (p + 8 <= (int)sz) {
                    u8 *id = buf + p;
                    u32 s  = buf[p+4] | ((u32)buf[p+5]<<8) |
                             ((u32)buf[p+6]<<16) | ((u32)buf[p+7]<<24);
                    p += 8;
                    if (p + (int)s > (int)sz) break;
                    const char *v = (const char *)(buf + p);
                    int vl = (int)s;
                    if (!memcmp(id, "INAM", 4))
                        copy_text(out->title, sizeof(out->title), (const u8*)v, vl, 0);
                    else if (!memcmp(id, "IART", 4))
                        copy_text(out->artist, sizeof(out->artist), (const u8*)v, vl, 0);
                    else if (!memcmp(id, "IPRD", 4))
                        copy_text(out->album, sizeof(out->album), (const u8*)v, vl, 0);
                    p += (s + 1) & ~1u;
                }
            }
            free(buf);
            break;
        } else {
            if (fseek(f, sz + (sz & 1), SEEK_CUR) != 0) break;
        }
    }
    fclose(f);
}

void metadata_read_light(const char *path, song_info_t *out)
{
    char fallback[128];
    strncpy(fallback, out->title, sizeof(fallback) - 1);
    fallback[sizeof(fallback) - 1] = 0;
    out->title[0]  = 0;
    out->artist[0] = 0;
    out->album[0]  = 0;

    int is_wav = (strlen(path) >= 4);
    if (is_wav) {
        const char *e = path + strlen(path) - 4;
        is_wav = (e[0] == '.' && (e[1] == 'w' || e[1] == 'W'));
    }
    if (is_wav) read_wav_info(path, out);
    else        id3_scan(path, out, NULL, 0);
    if (!out->title[0]) strncpy(out->title, fallback, sizeof(out->title) - 1);
}

u32 *metadata_read_cover(const char *path, int out_w, int out_h)
{
    song_info_t dummy; memset(&dummy, 0, sizeof(dummy));
    apic_t apic = { NULL, 0 };

    const char *e = strrchr(path, '.');
    if (e && (e[1] == 'm' || e[1] == 'M'))
        id3_scan(path, &dummy, &apic, 1);
    if (!apic.ptr) return NULL;

    int w, h, ch;
    u8 *pixels = stbi_load_from_memory(apic.ptr, apic.len, &w, &h, &ch, 4);
    free(apic.ptr);
    if (!pixels) return NULL;

    u32 *out = (u32 *)malloc(out_w * out_h * 4);
    if (!out) { stbi_image_free(pixels); return NULL; }

    int min_dim = (w < h) ? w : h;
    int crop_x  = (w - min_dim) / 2;
    int crop_y  = (h - min_dim) / 2;

    for (int y = 0; y < out_h; y++) {
        int sy = crop_y + y * min_dim / out_h;
        u32 *dp = out + (u32)y * out_w;
        for (int x = 0; x < out_w; x++) {
            int sx = crop_x + x * min_dim / out_w;
            u8 *sp = pixels + ((u32)sy * w + sx) * 4;
            u32 a = sp[3], r = sp[0], g = sp[1], b = sp[2];
            dp[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    stbi_image_free(pixels);
    return out;
}