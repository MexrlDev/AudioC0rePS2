#ifndef LIBRARY_H
#define LIBRARY_H
#include "app.h"

#define LIB_SRC_USB   0
#define LIB_SRC_MC0   1
#define LIB_SRC_MC1   2
#define LIB_SRC_HDD   3
#define LIB_SRC_HOST  4
#define LIB_SRC_COUNT 5

int          library_scan(void);
int          library_count(void);
const char  *library_path(int i);
song_info_t *library_get(int i);
int          library_last_error(void);
const char  *library_root(void);

int          library_source_count(void);
const char  *library_source_label(int idx);
const char  *library_source_root(int idx);
int          library_source_available(int idx);
int          library_source_selectable(int idx);
int          library_source_current(void);
int          library_set_source(int idx);
int          library_available_count(void);

int          library_source_gone(void);

/* Force the library to the "no source" state. Called by main.c when
   the stronger opendir-based check detects the source truly gone. */
void         library_force_empty(void);

#endif