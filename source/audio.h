#ifndef AUDIO_H
#define AUDIO_H
#include "app.h"

#define AUDIO_IDLE     0
#define AUDIO_LOADING  1
#define AUDIO_READY    2
#define AUDIO_ERROR    3

int  audio_init(void);
void audio_shutdown(void);
int  audio_open(const char *path);
void audio_cancel_load(void);
int  audio_state(void);
void audio_stop(void);
void audio_play(void);
void audio_pause(void);
void audio_resume(void);
int  audio_is_playing(void);
int  audio_at_end(void);
u32  audio_position_ms(void);
u32  audio_duration_ms(void);
void audio_seek_ms(u32 ms);
void audio_set_volume(int v);
int  audio_get_volume(void);
void audio_set_mute(int m);
int  audio_get_mute(void);
void audio_request_seek(u32 ms);
int  audio_device_lost(void);

#endif