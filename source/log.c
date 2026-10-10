#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <kernel.h>
#include <tamtypes.h>
#include "log.h"

static char  g_path[256] = "";
static char  g_base[64]  = "";
static int   g_sema      = -1;
static volatile int g_on = 0;

int log_enabled(void) { return g_on; }

void log_set_base(const char *base_prefix)
{
    if (!base_prefix) return;
    strncpy(g_base, base_prefix, sizeof(g_base) - 1);
    g_base[sizeof(g_base) - 1] = 0;
}

int log_enable(void)
{
    if (g_on) return 0;
    if (!g_base[0]) return -1;

    if (g_sema < 0) {
        ee_sema_t s; s.init_count = 1; s.max_count = 1; s.option = 0;
        g_sema = CreateSema(&s);
    }

    char root[128];
    snprintf(root, sizeof(root), "%sAC-Logs", g_base);
    mkdir(root, 0777);

    int n = 1;
    char dir[160];
    while (n < 999) {
        snprintf(dir, sizeof(dir), "%s/LOG%d", root, n);
        struct stat st;
        if (stat(dir, &st) != 0) break;
        n++;
    }
    mkdir(dir, 0777);

    time_t now = time(NULL);
    if (now > 0) {
        struct tm *tm = localtime(&now);
        if (tm)
            snprintf(g_path, sizeof(g_path),
                     "%s/LOG_%04d%02d%02d_%02d%02d%02d.txt",
                     dir, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                     tm->tm_hour, tm->tm_min, tm->tm_sec);
        else
            snprintf(g_path, sizeof(g_path), "%s/LOG.txt", dir);
    } else {
        snprintf(g_path, sizeof(g_path), "%s/LOG.txt", dir);
    }

    FILE *f = fopen(g_path, "w");
    if (!f) { g_path[0] = 0; return -1; }
    fprintf(f, "AudioC0re - PS2 log\n");
    fprintf(f, "Folder: LOG%d\n", n);
    fprintf(f, "Path: %s\n", g_path);
    fprintf(f, "========================================\n");
    fclose(f);

    g_on = 1;
    log_write("log enabled\n");
    return 0;
}

/* IMPORTANT: turn g_on OFF before touching disk. On real PS2 with USB
   pulled, an fopen here can hang the EE. */
void log_disable(void)
{
    if (!g_on) return;
    g_on = 0;
    g_path[0] = 0;
    g_base[0] = 0;
}

void log_write(const char *fmt, ...)
{
    if (!g_on) return;
    if (!g_path[0]) return;

    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if (n > (int)sizeof(buf) - 1) n = sizeof(buf) - 1;

    if (g_sema >= 0) WaitSema(g_sema);
    FILE *f = fopen(g_path, "a");
    if (f) {
        fwrite(buf, 1, n, f);
        fflush(f);
        fclose(f);
    }
    if (g_sema >= 0) SignalSema(g_sema);
}

const char *log_path(void) { return g_path; }