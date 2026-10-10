#ifndef CONFIG_H
#define CONFIG_H
#include "app.h"

typedef struct {
    int source;   /* library source index, -1 = auto-detect */
    int lang;     /* lang index */
} app_config_t;

extern app_config_t g_cfg;

void config_load(void);
int  config_save(void);
void config_defaults(void);

#endif
