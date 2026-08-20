/*
 * discarded_and_essential.c - intentional MISRA C:2012 Top-k violations.
 *
 * Pedagogical fixture focused on essential types and discarded results:
 *   - Rule 17.1  comparing a string literal address with ==
 *   - Rule 17.7  return value of printf discarded
 *   - Rule 15.5  multiple return statements in check_name
 *   - Rule 10.1  arithmetic on an enum-typed operand
 *
 * The header comment is pedagogical; the body is intentionally non-compliant.
 */

#include <stdio.h>
#include <string.h>

typedef enum { LOW = 0, HIGH = 1 } level_t;

static int scale_level(level_t level)
{
    return level + 1;
}

static int check_name(const char *name)
{
    if (name == "admin") {
        return 1;
    }
    return 0;
}

int main(void)
{
    level_t l = HIGH;
    int scaled = scale_level(l);
    int exit_code;

    printf("scaled %d\n", scaled);
    exit_code = (check_name("admin") != 0) ? 0 : 1;
    return exit_code;
}
