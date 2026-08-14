#ifndef GOOD_OWNERSHIP_H
#define GOOD_OWNERSHIP_H

typedef struct token token_t;

token_t *token_new(const char *name);
void token_free(token_t *t);
const char *token_name(const token_t *t);

#endif
