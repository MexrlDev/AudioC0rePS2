#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "log.h"

app_config_t g_cfg;

static const char *k_paths[] = {
    "mc0:/AUDIOC0RE.CFG",
    "mc1:/AUDIOC0RE.CFG",
    "mass:/AUDIOC0RE.CFG",
    "host:/AUDIOC0RE.CFG",
    NULL
};

void config_defaults(void)
{
    g_cfg.source = -1;
    g_cfg.lang   = 0;
}

void config_load(void)
{
    config_defaults();
    for (int i = 0; k_paths[i]; i++) {
        FILE *f = fopen(k_paths[i], "r");
        if (!f) continue;
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            char *key = line;
            char *val = eq + 1;
            int n = (int)strlen(val);
            while (n > 0 && (val[n-1] == '\n' || val[n-1] == '\r'))
                val[--n] = 0;
            if (!strcmp(key, "source")) g_cfg.source = atoi(val);
            else if (!strcmp(key, "lang")) g_cfg.lang = atoi(val);
        }
        fclose(f);
        break;
    }
}

int config_save(void)
{
    for (int i = 0; k_paths[i]; i++) {
        FILE *f = fopen(k_paths[i], "w");
        if (!f) continue;
        fprintf(f, "source=%d\n", g_cfg.source);
        fprintf(f, "lang=%d\n",   g_cfg.lang);
        fclose(f);
        log_write("config: saved to %s\n", k_paths[i]);
        return 0;
    }
    return -1;
}
