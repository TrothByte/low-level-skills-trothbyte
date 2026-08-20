/*
 * essential_types_and_returns.c - corrected counterpart of
 * discarded_and_essential.c.
 *
 * Rewritten for the MISRA C:2012 Top-k rules:
 *   - Rule 17.1  string comparison done with strcmp, not ==
 *   - Rule 17.7  every non-void function result is captured and used
 *   - Rule 15.5  single exit point per function
 *   - Rule 10.1  enum explicitly cast to its essential integer type before
 *                arithmetic
 *
 * The header comment is pedagogical.
 */

#include <stdio.h>
#include <string.h>

typedef enum { LOW = 0, HIGH = 1 } level_t;

static int scale_level(level_t level)
{
    return (int)level + 1;
}

static int check_name(const char *name)
{
    int result;

    result = strcmp(name, "admin");
    return (result == 0) ? 1 : 0;
}

int main(void)
{
    level_t l = HIGH;
    int scaled = scale_level(l);
    int matched = check_name("admin");
    int exit_code;

    exit_code = ((scaled != 0) && (matched != 0)) ? 0 : 1;
    return exit_code;
}
