#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include "library.h"
#include "metadata.h"
#include "log.h"

static song_info_t g_songs[MAX_SONGS];
static int         g_count = 0;
static int         g_last_error = 0;
static char        g_root[64] = "";
static int         g_cur_src = -1;

static int g_avail_cache[LIB_SRC_COUNT] = {0,0,0,0,0};
static int g_avail_valid = 0;

static const char *k_roots[LIB_SRC_COUNT] = {
    "mass:/music",
    "mc0:/music",
    "mc1:/music",
    "pfs0:/music",
    "host:/music",
};

static const char *k_labels[LIB_SRC_COUNT] = {
    "USB Drive",
    "Memory Card 0",
    "Memory Card 1",
    "Hard Drive",
    "Host (PCSX2)",
};

static int has_ext(const char *n, const char *e)
{
    size_t ln = strlen(n), le = strlen(e);
    if (ln < le) return 0;
    for (size_t i = 0; i < le; i++) {
        char a = n[ln - le + i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != e[i]) return 0;
    }
    return 1;
}

static int cmp_song(const void *a, const void *b)
{
    return strcasecmp(((const song_info_t*)a)->name,
                      ((const song_info_t*)b)->name);
}

/* Stronger check than plain stat: opening the directory forces the
   IOP to touch the actual mount point. If USB was pulled, opendir
   fails with ENODEV/ENOENT even when stat still lies. */
static int root_exists(const char *root)
{
    struct stat st;
    if (stat(root, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return 0;

    DIR *d = opendir(root);
    if (!d) return 0;
    closedir(d);
    return 1;
}

static void refresh_avail(void)
{
    for (int i = 0; i < LIB_SRC_COUNT; i++)
        g_avail_cache[i] = root_exists(k_roots[i]);
    g_avail_valid = 1;
}

int library_source_available(int i)
{
    if (i < 0 || i >= LIB_SRC_COUNT) return 0;
    if (!g_avail_valid) refresh_avail();
    return g_avail_cache[i];
}

int library_source_selectable(int i) { return library_source_available(i); }

int library_available_count(void)
{
    if (!g_avail_valid) refresh_avail();
    int n = 0;
    for (int i = 0; i < LIB_SRC_COUNT; i++) n += g_avail_cache[i];
    return n;
}

static int scan_from_root(const char *root)
{
    DIR *d = opendir(root);
    if (!d) { g_last_error = 3; return 0; }

    g_count = 0;
    strncpy(g_root, root, sizeof(g_root) - 1);
    g_root[sizeof(g_root) - 1] = 0;

    struct dirent *e;
    while ((e = readdir(d)) != NULL && g_count < MAX_SONGS) {
        if (e->d_name[0] == '.') continue;
        if (!has_ext(e->d_name, ".mp3") && !has_ext(e->d_name, ".wav"))
            continue;

        song_info_t *s = &g_songs[g_count];
        memset(s, 0, sizeof(*s));
        snprintf(s->path, sizeof(s->path), "%s/%s", root, e->d_name);
        strncpy(s->name, e->d_name, sizeof(s->name) - 1);
        strncpy(s->title, e->d_name, sizeof(s->title) - 1);
        char *dot = strrchr(s->title, '.');
        if (dot) *dot = 0;
        metadata_read_light(s->path, s);
        log_write("  [track] %s | title='%s' artist='%s'\n",
                  s->name, s->title, s->artist);
        g_count++;
    }
    closedir(d);

    if (g_count == 0) g_last_error = 2;
    else              qsort(g_songs, g_count, sizeof(song_info_t), cmp_song);

    return g_count;
}

int library_scan(void)
{
    g_last_error = 0;
    refresh_avail();

    /* If current source truly gone, drop it so auto-detect runs. */
    if (g_cur_src >= 0 && !g_avail_cache[g_cur_src]) {
        g_cur_src = -1;
    }

    if (g_cur_src >= 0 && g_cur_src < LIB_SRC_COUNT && g_avail_cache[g_cur_src]) {
        log_write("library: using current source '%s'\n", k_roots[g_cur_src]);
        return scan_from_root(k_roots[g_cur_src]);
    }

    static const int auto_order[] = {
        LIB_SRC_USB, LIB_SRC_HOST, LIB_SRC_HDD, -1
    };
    for (int i = 0; auto_order[i] >= 0; i++) {
        int idx = auto_order[i];
        if (g_avail_cache[idx]) {
            log_write("library: auto-picked '%s'\n", k_roots[idx]);
            g_cur_src = idx;
            return scan_from_root(k_roots[idx]);
        }
    }

    g_last_error = 3;
    g_count = 0;
    g_root[0] = 0;
    g_cur_src = -1;
    return 0;
}

int          library_count(void)      { return g_count; }
const char  *library_path(int i)      { return (i >= 0 && i < g_count) ? g_songs[i].path : ""; }
song_info_t *library_get(int i)       { return (i >= 0 && i < g_count) ? &g_songs[i] : NULL; }
int          library_last_error(void) { return g_last_error; }
const char  *library_root(void)       { return g_root; }

int          library_source_count(void)   { return LIB_SRC_COUNT; }
const char  *library_source_label(int i)  { return (i >= 0 && i < LIB_SRC_COUNT) ? k_labels[i] : ""; }
const char  *library_source_root(int i)   { return (i >= 0 && i < LIB_SRC_COUNT) ? k_roots[i] : ""; }
int          library_source_current(void) { return g_cur_src; }

int library_set_source(int idx)
{
    if (idx < 0 || idx >= LIB_SRC_COUNT) return -1;
    if (!root_exists(k_roots[idx])) return -1;
    g_cur_src = idx;
    g_avail_valid = 0;
    return 0;
}

int library_source_gone(void)
{
    if (g_cur_src < 0 || g_cur_src >= LIB_SRC_COUNT) return 0;
    return root_exists(k_roots[g_cur_src]) ? 0 : 1;
}

void library_force_empty(void)
{
    g_count = 0;
    g_last_error = 3;
    g_root[0] = 0;
    g_cur_src = -1;
    g_avail_valid = 0;
}