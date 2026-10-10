#ifndef SETTINGS_H
#define SETTINGS_H
#include "app.h"

/* Returns:
   0 = back without save
   1 = saved, back to menu
   2 = still open (keep drawing)
*/
int settings_menu_run(u32 *fb);

#endif
