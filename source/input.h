#ifndef INPUT_H
#define INPUT_H
#include "app.h"

#define BTN_UP       (1u<<0)
#define BTN_DOWN     (1u<<1)
#define BTN_LEFT     (1u<<2)
#define BTN_RIGHT    (1u<<3)
#define BTN_CROSS    (1u<<4)
#define BTN_CIRCLE   (1u<<5)
#define BTN_SQUARE   (1u<<6)
#define BTN_TRIANGLE (1u<<7)
#define BTN_L1       (1u<<8)
#define BTN_R1       (1u<<9)
#define BTN_L2       (1u<<10)
#define BTN_R2       (1u<<11)
#define BTN_START    (1u<<12)
#define BTN_SELECT   (1u<<13)

void input_init(void);
void input_poll(void);
int  input_pressed(u32 mask);
int  input_held(u32 mask);
#endif
