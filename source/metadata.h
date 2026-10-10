#ifndef METADATA_H
#define METADATA_H
#include "app.h"

/* Cheap header-only metadata read: title/artist/album (no cover). */
void  metadata_read_light(const char *path, song_info_t *out);

/* Full read at play time. Returns a malloc'd BGRA buffer of
   out_w * out_h pixels (caller frees), or NULL if no cover. */
u32 *metadata_read_cover(const char *path, int out_w, int out_h);

#endif
