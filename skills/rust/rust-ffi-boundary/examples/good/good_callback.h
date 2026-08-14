#ifndef GOOD_CALLBACK_H
#define GOOD_CALLBACK_H

#include <stdint.h>

typedef void (*visit_fn)(const char *key, int64_t value, void *user);

/* Iterates a table, calling cb(key, value, user) for each entry. */
int table_visit(const void *table, visit_fn cb, void *user);

#endif
