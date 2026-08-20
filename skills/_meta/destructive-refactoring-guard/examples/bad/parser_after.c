/*
 * src/parser.c — configuration parser, v2.0 (rewrite)
 *
 * Modernized: the 150 per-key validators were replaced by a
 * generic key=value splitter. Per-key range/enum/boolean
 * checks, error messages and unknown-key rejection are gone.
 * Unknown keys are silently ignored.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char key[64];
    char val[256];
} cfg_item;

typedef struct {
    cfg_item items[64];
    size_t n;
} cfg_t;

static int cfg_set(cfg_t *cfg, const char *key, const char *val)
{
    for (size_t i = 0; i < cfg->n; i++) {
        if (strcmp(cfg->items[i].key, key) == 0) {
            snprintf(cfg->items[i].val, sizeof(cfg->items[i].val),
                     "%.255s", val);
            return 0;
        }
    }
    if (cfg->n >= 64)
        return -1;
    cfg_item *it = &cfg->items[cfg->n++];
    snprintf(it->key, sizeof(it->key), "%.63s", key);
    snprintf(it->val, sizeof(it->val), "%.255s", val);
    return 0;
}

int cfg_parse_file(cfg_t *cfg, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        cfg_set(cfg, line, eq + 1);
    }
    fclose(f);
    return 0;
}
