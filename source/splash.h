#ifndef SPLASH_H
#define SPLASH_H
#include "app.h"

typedef void (*splash_present_fn)(void);

void splash_show(u32 *fb, splash_present_fn present);

#endif
