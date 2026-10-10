/* AudioC0re - PS2 — audio engine
 * Single-threaded, file loaded into RAM, resampled to 48 kHz stereo.
 * Audio thread runs ABOVE the UI thread so it is never starved.*/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <kernel.h>
#include <tamtypes.h>
#include <sifrpc.h>
#include <audsrv.h>
#include "audio.h"
#include "minimp3.h"
#include "log.h"

#define OUT_RATE    48000
#define OUT_FRAMES  2048
#define OUT_BYTES   (OUT_FRAMES * 4)
#define STAGE       512
#define MAX_FILE    (24 * 1024 * 1024)
#define READ_CHUNK  4096
#define SILENCE_CHUNK 2048

#define DRAIN_STEP_MS ((OUT_FRAMES * 1000u) / OUT_RATE)
#define DRAIN_MIN_MS  60

#define ALOG(fmt, ...) do { \
    printf("[AudioC0re] audio: " fmt "\n", ##__VA_ARGS__); \
    log_write("audio: " fmt "\n", ##__VA_ARGS__); \
    fflush(stdout); \
} while (0)

typedef enum { SK_NONE, SK_WAV, SK_MP3 } SrcKind;

static struct {
    SrcKind kind;
    u8     *buf;
    u32     size;
    int     rate;
    int     ch;
    const u8 *pcm;
    u32  frames, pos;
    mp3dec_t    mp3d;
    const u8   *mp3_start;
    u32         mp3_total;
    const u8   *mp3p;
    u32         mp3_left;
    s16         mp3_pend[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int         mp3_pend_n, mp3_pend_pos;
} S;

static volatile int  g_run;
static volatile int  g_started;
static volatile int  g_alive;
static volatile int  g_paused;
static volatile int  g_muted;
static volatile int  g_eof;
static volatile int  g_draining;
static volatile int  g_really_done;
static volatile u32  g_drain_ms_left;
static volatile int  g_dev_lost;
static volatile int  g_seek_req;
static volatile u32  g_seek_ms;
static volatile int  g_park_req;
static volatile int  g_parked;

static volatile int  g_load_req;
static volatile int  g_load_abort;
static volatile int  g_load_gen = 0;
static char          g_load_path[256];
static volatile int  g_state = AUDIO_IDLE;

static int           g_vol = 100;

static char g_stack[64 * 1024] __attribute__((aligned(64)));
static int  g_tid = -1;
static int  g_fmt_set = 0;

static volatile u32 g_frames_played;
static volatile u32 g_dur_ms;

static s16 g_stg[STAGE * 2] __attribute__((aligned(64)));
static int g_stg_n, g_stg_pos;
static int g_pl, g_pr, g_nl, g_nr;
static u32 g_phase, g_step;

static s16 g_out[OUT_FRAMES * 2] __attribute__((aligned(64)));
static s16 g_silence[SILENCE_CHUNK / 2] __attribute__((aligned(64)));

/* Last time silence was pushed. Used to pace silence to the DSP rate. */
static u32 g_last_silence_tick = 0;

static void src_pull(s16 *dst, int frames);
static void rs_reset(void);
static void free_src(void);
static int  ensure_thread(void);
static int  load_file_internal(const char *path, int my_gen);
static int  install(u8 *buf, u32 size, const char *name, int my_gen);

static void reset_end_state(void)
{
    g_eof           = 0;
    g_draining      = 0;
    g_really_done   = 0;
    g_drain_ms_left = 0;
}

/* Push silence to the ring so the DSP never underruns during a load.
   Paced by wall clock: each chunk is ~10.7 ms of audio, so we push
   roughly one chunk per 10 ms of elapsed real time. This matches the
   DSP's consumption rate exactly. */
static void top_up_silence(void)
{
    if (!g_run) return;

    u32 now = SDL_GetTicks();
    if (g_last_silence_tick == 0) { g_last_silence_tick = now; return; }

    u32 elapsed = now - g_last_silence_tick;
    int chunks = (int)(elapsed / 10);
    if (chunks <= 0) return;
    if (chunks > 40) chunks = 40;   /* cap: at most 80 KB per burst */

    for (int i = 0; i < chunks && !g_load_abort && g_run; i++)
        audsrv_play_audio((const char *)g_silence, sizeof(g_silence));

    g_last_silence_tick = now;
}

static void pull1(int *l, int *r)
{
    if (g_stg_pos >= g_stg_n) {
        src_pull(g_stg, STAGE);
        g_stg_n = STAGE;
        g_stg_pos = 0;
    }
    *l = g_stg[2 * g_stg_pos];
    *r = g_stg[2 * g_stg_pos + 1];
    g_stg_pos++;
}

static void rs_reset(void)
{
    g_stg_n = g_stg_pos = 0;
    g_phase = 0;
    if (S.rate > 0 && S.rate != OUT_RATE)
        g_step = (u32)(((u64)S.rate << 16) / OUT_RATE);
    else
        g_step = 65536;
    pull1(&g_pl, &g_pr);
    pull1(&g_nl, &g_nr);
}

static void src_pull_wav(s16 *dst, int frames)
{
    for (int i = 0; i < frames; i++) {
        if (S.pos >= S.frames) {
            dst[2*i]   = 0;
            dst[2*i+1] = 0;
            g_eof = 1;
            continue;
        }
        const u8 *p = S.pcm + (size_t)S.pos * S.ch * 2;
        s16 l = (s16)(p[0] | (p[1] << 8));
        s16 r = (S.ch == 2) ? (s16)(p[2] | (p[3] << 8)) : l;
        dst[2*i]   = l;
        dst[2*i+1] = r;
        S.pos++;
    }
}

static void src_pull_mp3(s16 *dst, int frames)
{
    int i = 0;
    while (i < frames) {
        while (i < frames && S.mp3_pend_pos < S.mp3_pend_n) {
            dst[2*i]   = S.mp3_pend[2 * S.mp3_pend_pos];
            dst[2*i+1] = S.mp3_pend[2 * S.mp3_pend_pos + 1];
            i++;
            S.mp3_pend_pos++;
        }
        if (i >= frames) return;

        if (S.mp3_left < 4) { g_eof = 1; break; }

        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&S.mp3d, S.mp3p, S.mp3_left,
                                          S.mp3_pend, &info);
        if (info.frame_bytes <= 0) {
            S.mp3p++;
            S.mp3_left--;
            continue;
        }
        S.mp3p    += info.frame_bytes;
        S.mp3_left -= info.frame_bytes;

        if (samples <= 0) { S.mp3_pend_n = S.mp3_pend_pos = 0; continue; }

        if (info.channels == 1) {
            for (int k = samples - 1; k >= 0; k--) {
                S.mp3_pend[k*2+1] = S.mp3_pend[k];
                S.mp3_pend[k*2]   = S.mp3_pend[k];
            }
        }
        S.mp3_pend_n   = samples;
        S.mp3_pend_pos = 0;
    }
    if (i < frames) memset(dst + i*2, 0, (size_t)(frames - i) * 4);
}

static void src_pull(s16 *dst, int frames)
{
    if (S.kind == SK_NONE || g_eof) {
        memset(dst, 0, (size_t)frames * 4);
        return;
    }
    if (S.kind == SK_WAV) { src_pull_wav(dst, frames); return; }
    src_pull_mp3(dst, frames);
}

static void render(s16 *out, int frames)
{
    if (S.kind == SK_NONE || g_eof || g_paused) {
        memset(out, 0, (size_t)frames * 4);
        return;
    }
    int v = g_vol;
    for (int i = 0; i < frames; i++) {
        s16 l = (s16)(g_pl + (((g_nl - g_pl) * (int)g_phase) >> 16));
        s16 r = (s16)(g_pr + (((g_nr - g_pr) * (int)g_phase) >> 16));
        if (g_muted) {
            out[2*i]   = 0;
            out[2*i+1] = 0;
        } else if (v >= 100) {
            out[2*i]   = l;
            out[2*i+1] = r;
        } else {
            out[2*i]   = (s16)(((int)l * v) / 100);
            out[2*i+1] = (s16)(((int)r * v) / 100);
        }
        g_phase += g_step;
        while (g_phase >= 65536u) {
            g_phase -= 65536u;
            g_pl = g_nl; g_pr = g_nr;
            pull1(&g_nl, &g_nr);
        }
    }
    g_frames_played += (u32)frames;
}

static void do_seek(u32 ms)
{
    if (S.kind == SK_NONE || g_dur_ms == 0) return;
    if (ms > g_dur_ms) ms = g_dur_ms;

    if (S.kind == SK_WAV) {
        u32 fr = (u32)((u64)ms * S.rate / 1000);
        if (fr >= S.frames) fr = S.frames ? S.frames - 1 : 0;
        S.pos = fr;
    } else {
        u32 off = (u32)((u64)S.mp3_total * ms / g_dur_ms);
        if (off > S.mp3_total - 4) off = S.mp3_total - 4;
        const u8 *p = S.mp3_start + off;
        u32 left = S.mp3_total - off;
        while (left > 4) {
            if (p[0] == 0xFF && (p[1] & 0xE0) == 0xE0) break;
            p++; left--;
        }
        mp3dec_init(&S.mp3d);
        S.mp3p       = p;
        S.mp3_left   = left;
        S.mp3_pend_n = S.mp3_pend_pos = 0;
    }

    reset_end_state();
    g_frames_played = (u32)((u64)ms * OUT_RATE / 1000);
    g_stg_n = g_stg_pos = 0;
    rs_reset();
}

static void audio_thread(void *arg)
{
    (void)arg;
    while (g_run) {
        if (g_park_req) {
            g_parked = 1;
            while (g_park_req && g_run) usleep(2000);
            g_parked = 0;
            continue;
        }
        if (g_load_req) {
            g_load_req = 0;
            g_load_abort = 0;
            int my_gen = g_load_gen;
            g_state = AUDIO_LOADING;
            g_last_silence_tick = 0;
            free_src();
            int r = load_file_internal(g_load_path, my_gen);
            if (my_gen != g_load_gen) {
                ALOG("stale load %d discarded (current gen %d)", my_gen, g_load_gen);
            } else if (g_load_abort) {
                ALOG("load aborted");
                g_state = AUDIO_IDLE;
            } else if (r != 0) {
                ALOG("load error %d", r);
                g_state = AUDIO_ERROR;
            } else {
                g_state = AUDIO_READY;
            }
            continue;
        }
        if (g_seek_req) {
            g_seek_req = 0;
            do_seek(g_seek_ms);
        }

        if (g_eof && !g_draining && !g_really_done) {
            g_draining = 1;
            int q = audsrv_queued();
            if (q < 0) q = 0;
            u32 ms = (u32)((u64)q * 1000u / (OUT_RATE * 4));
            if (ms < DRAIN_MIN_MS) ms = DRAIN_MIN_MS;
            g_drain_ms_left = ms;
        }

        render(g_out, OUT_FRAMES);

        if (audsrv_wait_audio(OUT_BYTES) < 0) { usleep(20000); continue; }
        if (audsrv_play_audio((char *)g_out, OUT_BYTES) < 0) {
            g_dev_lost = 1;
            usleep(20000);
        }

        if (g_draining && !g_paused) {
            if (g_drain_ms_left <= DRAIN_STEP_MS) {
                g_drain_ms_left = 0;
                g_draining = 0;
                g_really_done = 1;
            } else {
                g_drain_ms_left -= DRAIN_STEP_MS;
            }
        }
    }
    g_alive = 0;
}

static int ensure_thread(void)
{
    if (!g_fmt_set) {
        struct audsrv_fmt_t f = { OUT_RATE, 16, 2 };
        if (audsrv_set_format(&f) != 0) { ALOG("set_format failed"); return -1; }
        audsrv_set_volume(MAX_VOLUME);
        g_fmt_set = 1;
    }
    if (g_started) return 0;

    ee_thread_status_t st;
    ReferThreadStatus(GetThreadId(), &st);
    int mp = st.current_priority;
    if (mp < 8) { ChangeThreadPriority(GetThreadId(), 16); mp = 16; }

    ee_thread_t t;
    memset(&t, 0, sizeof t);
    t.func             = audio_thread;
    t.stack            = g_stack;
    t.stack_size       = sizeof(g_stack);
    t.gp_reg           = &_gp;
    t.initial_priority = mp - 4;

    g_run      = 1;
    g_alive    = 1;
    g_park_req = 0;
    g_tid      = CreateThread(&t);
    if (g_tid < 0) { g_run = 0; g_alive = 0; ALOG("CreateThread failed"); return -2; }
    g_started = 1;
    StartThread(g_tid, NULL);
    ALOG("thread started (main prio=%d, audio prio=%d)", mp, mp - 4);
    return 0;
}

static u32 rd32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
static u32 rd16(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8); }

static void free_src(void)
{
    if (S.buf) { free(S.buf); S.buf = NULL; }
    memset(&S, 0, sizeof S);
    S.kind = SK_NONE;
}

static int park(void)
{
    if (!g_started) return 1;
    g_parked = 0;
    g_park_req = 1;
    for (int i = 0; i < 2500; i++) {
        if (g_parked) return 1;
        usleep(2000);
    }
    g_park_req = 0;
    return 0;
}
static void unpark(void) { g_park_req = 0; }

static int install(u8 *buf, u32 size, const char *name, int my_gen)
{
    if (g_load_abort || my_gen != g_load_gen) {
        ALOG("install discarded (abort=%d gen=%d/%d)",
             g_load_abort, my_gen, g_load_gen);
        free(buf);
        return -9;
    }

    SrcKind kind = SK_NONE;
    int rate = 0, ch = 0;
    u32 dpos = 0, dlen = 0;
    const u8 *mp3_start = NULL;
    u32 mp3_total = 0;

    if (size > 44 && !memcmp(buf, "RIFF", 4) && !memcmp(buf + 8, "WAVE", 4)) {
        u32 fmt = 0, bits = 0, p = 12;
        while (p + 8 <= size) {
            u32 len = rd32(buf + p + 4);
            if (!memcmp(buf + p, "fmt ", 4) && p + 24 <= size) {
                fmt  = rd16(buf + p + 8);
                ch   = (int)rd16(buf + p + 10);
                rate = (int)rd32(buf + p + 12);
                bits = rd16(buf + p + 22);
            } else if (!memcmp(buf + p, "data", 4)) {
                dpos = p + 8;
                dlen = len;
                break;
            }
            p += 8 + len + (len & 1);
        }
        if (fmt != 1 || bits != 16 || (ch != 1 && ch != 2) ||
            !dpos || rate < 4000 || rate > 96000) {
            ALOG("%s must be 16-bit PCM WAV", name);
            free(buf);
            return -5;
        }
        if (dpos + dlen > size) dlen = size - dpos;
        if (dlen < (u32)ch * 2 * 2048) {
            ALOG("%s too short", name);
            free(buf);
            return -6;
        }
        kind = SK_WAV;
    } else {
        u32 off = 0;
        if (size > 10 && !memcmp(buf, "ID3", 3)) {
            off = 10 + (((u32)buf[6] << 21) | ((u32)buf[7] << 14) |
                        ((u32)buf[8] << 7) | (u32)buf[9]);
        }
        if (off >= size) { ALOG("%s: bad ID3", name); free(buf); return -7; }
        mp3_start = buf + off;
        mp3_total = size - off;
        kind = SK_MP3;
    }

    if (ensure_thread() != 0) { free(buf); return -3; }

    free_src();

    S.kind = kind;
    S.buf  = buf;
    S.size = size;

    if (kind == SK_WAV) {
        S.rate   = rate;
        S.ch     = ch;
        S.pcm    = buf + dpos;
        S.frames = dlen / (ch * 2);
        S.pos    = 0;
        g_dur_ms = (u32)((u64)S.frames * 1000 / rate);
    } else {
        mp3dec_init(&S.mp3d);
        S.mp3_start  = mp3_start;
        S.mp3_total  = mp3_total;
        S.mp3p       = mp3_start;
        S.mp3_left   = mp3_total;
        S.mp3_pend_n = S.mp3_pend_pos = 0;

        mp3dec_frame_info_t info;
        s16 tmp[MINIMP3_MAX_SAMPLES_PER_FRAME];
        int n = mp3dec_decode_frame(&S.mp3d, S.mp3p, S.mp3_left, tmp, &info);
        if (n <= 0 || info.hz == 0) {
            ALOG("%s is not a valid MP3", name);
            free_src();
            return -8;
        }
        S.rate = info.hz;
        S.ch   = info.channels;

        mp3dec_init(&S.mp3d);
        S.mp3p       = mp3_start;
        S.mp3_left   = mp3_total;
        S.mp3_pend_n = S.mp3_pend_pos = 0;

        if (info.bitrate_kbps > 0)
            g_dur_ms = (u32)((u64)mp3_total * 8 / info.bitrate_kbps);
        else
            g_dur_ms = 0;
    }

    reset_end_state();
    g_paused = 0;
    g_muted  = 0;
    g_frames_played = 0;
    rs_reset();

    ALOG("playing %s (%d Hz, %dch, %s, dur=%ums)",
         name, S.rate, S.ch, kind == SK_WAV ? "wav" : "mp3",
         (unsigned)g_dur_ms);
    return 0;
}

static int load_file_internal(const char *path, int my_gen)
{
    FILE *f = fopen(path, "rb");
    if (!f) { ALOG("cannot open %s", path); return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0 || sz > MAX_FILE) { fclose(f); ALOG("bad size %ld", sz); return -2; }
    fseek(f, 0, SEEK_SET);
    u8 *buf = (u8 *)malloc(sz);
    if (!buf) { fclose(f); ALOG("malloc %ld failed", sz); return -3; }

    /* Prime the ring with silence so the DSP doesn't loop the tail of
       the previous song while we read. Pushed just before the read
       loop, paced by wall clock afterward. */
    g_last_silence_tick = 0;
    for (int i = 0; i < 8; i++)
        audsrv_play_audio((const char *)g_silence, sizeof(g_silence));

    size_t got = 0;
    while (got < (size_t)sz) {
        if (g_load_abort || my_gen != g_load_gen) {
            free(buf); fclose(f);
            ALOG("load interrupted (abort=%d gen=%d/%d)",
                 g_load_abort, my_gen, g_load_gen);
            return -9;
        }

        size_t want = (size_t)sz - got;
        if (want > READ_CHUNK) want = READ_CHUNK;
        size_t r = fread(buf + got, 1, want, f);
        if (r == 0) break;
        got += r;

        /* Keep the DSP fed with silence while the next read happens. */
        top_up_silence();
    }
    fclose(f);
    if (got != (size_t)sz) {
        free(buf);
        ALOG("short read %u/%u", (unsigned)got, (unsigned)sz);
        return -4;
    }
    return install(buf, (u32)sz, path, my_gen);
}

int audio_init(void)
{
    int ret = audsrv_init();
    ALOG("audsrv_init=%d", ret);
    if (ret != 0 && ret != -1) return -1;
    g_fmt_set  = 0;
    g_started  = 0;
    g_alive    = 0;
    g_dev_lost = 0;
    g_state    = AUDIO_IDLE;
    reset_end_state();
    return 0;
}

void audio_shutdown(void)
{
    g_run = 0;
    g_park_req = 0;

    for (int i = 0; i < 1500 && g_alive; i++) usleep(2000);

    if (g_alive) {
        ALOG("audio thread stuck, forcing termination");
        if (g_tid >= 0) { TerminateThread(g_tid); DeleteThread(g_tid); g_tid = -1; }
        g_started = 0;
        return;
    }

    if (g_tid >= 0) { DeleteThread(g_tid); g_tid = -1; }
    g_started = 0;
    free_src();
    audsrv_stop_audio();
    usleep(50000);
    audsrv_stop_audio();
    audsrv_quit();
    g_fmt_set = 0;
}

int audio_open(const char *path)
{
    if (ensure_thread() != 0) return -1;

    g_load_gen++;
    g_load_abort = 1;

    strncpy(g_load_path, path, sizeof(g_load_path) - 1);
    g_load_path[sizeof(g_load_path) - 1] = 0;
    g_state    = AUDIO_LOADING;
    g_load_req = 1;
    return 0;
}

void audio_cancel_load(void)
{
    if (g_state == AUDIO_LOADING) g_load_abort = 1;
}

int audio_state(void) { return g_state; }

/* Fast-path stop. If a load is in progress, do NOT block on the audio
   thread — it may be stuck inside fread on a dead USB. Just signal
   the abort, reset our side, and return immediately. The thread will
   clean up as soon as it notices the flag. If no load is in progress,
   park the thread and free the buffer safely. */
void audio_stop(void)
{
    g_load_abort = 1;
    g_paused     = 1;
    g_eof        = 1;

    if (g_state == AUDIO_LOADING) {
        g_load_req = 0;
        g_state    = AUDIO_IDLE;
        g_frames_played = 0;
        g_dur_ms        = 0;
        g_stg_n = g_stg_pos = 0;
        return;
    }

    if (park()) {
        free_src();
    }
    unpark();
    g_state = AUDIO_IDLE;
    reset_end_state();
    g_frames_played = 0;
    g_dur_ms = 0;
    g_stg_n = g_stg_pos = 0;
}

void audio_play(void)   { g_paused = 0; }
void audio_pause(void)  { g_paused = 1; }
void audio_resume(void) { g_paused = 0; }

int audio_is_playing(void)
{
    if (S.kind == SK_NONE) return 0;
    if (g_paused || g_really_done || g_dev_lost) return 0;
    return 1;
}

int audio_at_end(void)
{
    return g_really_done && !g_paused;
}

u32 audio_position_ms(void)
{
    if (g_frames_played == 0) return 0;
    return (u32)((u64)g_frames_played * 1000 / OUT_RATE);
}

u32 audio_duration_ms(void) { return g_dur_ms; }

void audio_seek_ms(u32 ms) { g_seek_ms = ms; g_seek_req = 1; }

void audio_set_volume(int v)
{
    if (v < 0)   v = 0;
    if (v > 100) v = 100;
    g_vol = v;
}
int audio_get_volume(void) { return g_vol; }

void audio_set_mute(int m) { g_muted = m ? 1 : 0; }
int  audio_get_mute(void)  { return g_muted; }

int  audio_device_lost(void) { return g_dev_lost; }

void audio_request_seek(u32 ms) { audio_seek_ms(ms); }