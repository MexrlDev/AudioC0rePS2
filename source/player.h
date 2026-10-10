#ifndef PLAYER_H
#define PLAYER_H
#include "app.h"

#define PLAYER_EXIT   0
#define PLAYER_NEXT   1
#define PLAYER_PREV  -1
#define PLAYER_STAY   2

int player_start(int song_index);
int player_run(u32 *fb);
int player_queue_on(void);

#endif
