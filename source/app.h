/* AudioC0re - PS2 — common types & defines
 * By MexrlDev */
#ifndef APP_H
#define APP_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;
typedef signed   char  s8;
typedef signed   short s16;
typedef signed   int   s32;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SCR_W 640
#define SCR_H 448

#define RGB(r,g,b) (0xFF000000u | ((u32)(r)<<16) | ((u32)(g)<<8) | (u32)(b))
#define RGBA_A(c)  ((u8)(((c)>>24) & 0xFF))

#define MUSIC_DIR  "mass:/music"
#define MAX_SONGS  512

/* Module registry */
typedef struct song_info {
    char  path[256];
    char  name[128];      /* basename */
    char  title[128];
    char  artist[128];
    char  album[128];
} song_info_t;

/* Global app state (defined in main.c) */
extern u32 *g_fb;

#endif
