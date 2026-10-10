#ifndef LANG_H
#define LANG_H
#include "app.h"

typedef enum {
    L_NOW_PLAYING, L_TRACKS, L_SOURCE_FMT,
    L_PLAYING, L_PAUSED, L_MUTED, L_UNMUTED,
    L_LOOP_ON, L_LOOP_OFF, L_AUTO_ON, L_AUTO_OFF,
    L_SEEK_PLUS, L_SEEK_MINUS, L_VOLUME,
    L_NEXT_TRACK, L_PREV_TRACK,
    L_NO_NEXT, L_NO_PREV, L_END_SONG, L_START_SONG,
    L_LOADING, L_LOAD_FAILED,
    L_LOG_ON, L_LOG_OFF, L_NO_COVER,
    L_NO_MUSIC, L_NO_SOURCE, L_FOLDER_EMPTY,
    L_SAVE_SRC, L_YES, L_NO, L_CANCEL,
    L_MENU_HELP, L_PLAYER_HELP1, L_PLAYER_HELP2,
    L_SETTINGS, L_SET_SOURCE, L_SET_LANG, L_SET_RESET,
    L_SET_SAVE, L_SET_CANCEL,
    L_SET_HELP_BACK, L_SET_HELP_SEL,
    L_RESET_DONE, L_SAVED, L_SAVE_FAILED, L_CONFIRM_RESET,
    L_SRC_REMOVED_FMT, L_PRESS_X_RESCAN,
    L_COUNT
} lang_id_t;

void        lang_init(void);
const char *lang_str(lang_id_t id);
int         lang_current(void);
int         lang_count(void);
const char *lang_name(int idx);
void        lang_set(int idx);

#endif
