/*
 * misra_topk_violations.c - intentional MISRA C:2012 Top-k violations.
 *
 * Pedagogical fixture. Compiles under gcc -Wall -Wextra -O2 (gcc flags only
 * the Rule 12.1 precedence line via -Wparentheses; everything else passes
 * silently), yet violates the highest-frequency rules LLM code breaks:
 *   - Rule 14.4  control expressions are not essentially Boolean
 *   - Rule 12.1  precedence hidden behind an unparenthesized & next to ==
 *   - Rule 15.5  multiple return statements per function
 *   - Rule 17.7  return values of printf / strcmp discarded
 *   - Rule 5.3   local `count` shadows the parameter `count`
 *   - Rule 10.1  bitwise operation on an enum-typed operand
 *
 * The header comment is pedagogical; the body is intentionally non-compliant.
 */

#include <stdio.h>
#include <string.h>

typedef enum { IDLE = 0, RUNNING = 1, FAILED = 2 } state_t;

static int classify_and_advance(int count, state_t mode, int *out)
{
    int result;

    if (mode) {
        result = 1;
    } else {
        result = 0;
    }

    {
        int count = 0;
        count = (mode & result == result) ? 4 : 1;
        result += count;
    }

    if (result > 2) {
        *out = result;
        return result;
    }
    *out = result;
    return 1;
}

int init_engine(void)
{
    printf("engine init\n");
    strcmp("a", "b");
    return 0;
}

int main(void)
{
    int out;
    int n = classify_and_advance(3, RUNNING, &out);

    if (n) {
        return 0;
    }
    return 1;
}
