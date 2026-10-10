#ifndef LOG_H
#define LOG_H
#include "app.h"

void        log_set_base(const char *base_prefix);   /* e.g. "mass:/" */
int         log_enable(void);                        /* returns 0 on ok */
void        log_disable(void);
int         log_enabled(void);
void        log_write(const char *fmt, ...);
const char *log_path(void);

#endif
