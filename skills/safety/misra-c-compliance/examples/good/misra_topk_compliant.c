/*
 * misra_topk_compliant.c - corrected counterpart of misra_topk_violations.c.
 *
 * Same observable behavior for classify_and_advance / main, rewritten for the
 * MISRA C:2012 Top-k rules:
 *   - Rule 14.4  control expressions are essentially Boolean
 *   - Rule 12.1  precedence made explicit with parentheses
 *   - Rule 15.5  single exit point per function
 *   - Rule 17.7  every non-void function result is captured and used
 *   - Rule 5.3   no identifier shadowing
 *   - Rule 10.1  enum used only in a comparison, never bitwise
 *
 * The header comment is pedagogical.
 */

#include <stdio.h>
#include <string.h>

typedef enum { IDLE = 0, RUNNING = 1, FAILED = 2 } state_t;

static int classify_and_advance(int count, state_t mode, int *out)
{
    int result;
    int local_count;

    (void)count;

    if (mode == RUNNING) {
        result = 1;
    } else {
        result = 0;
    }

    local_count = (mode == RUNNING) ? 4 : 1;
    result += local_count;

    *out = result;
    return result;
}

int init_engine(void)
{
    int printed;
    int cmp;

    printed = printf("engine init\n");
    cmp = strcmp("a", "b");
    return (printed > 0 && cmp == 0) ? 0 : 1;
}

int main(void)
{
    int out;
    int n = classify_and_advance(3, RUNNING, &out);
    int exit_code;

    exit_code = (n != 0) ? 0 : 1;
    return exit_code;
}
