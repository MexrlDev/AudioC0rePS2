#ifndef MENU_H
#define MENU_H
#include "app.h"

#define SCREEN_MENU     0
#define SCREEN_PLAYER   1
#define SCREEN_SETTINGS 2
#define SCREEN_GONE     3

int  menu_run(u32 *fb);
void menu_draw(u32 *fb);
int  menu_run_empty(u32 *fb);
void menu_toggle_log(void);

#endif