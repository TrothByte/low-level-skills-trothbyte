/*
 * recursive_task.c - BANNED PATTERN: recursion in a hard real-time task.
 *
 * This fixture compiles and runs on the host, but recursion makes stack
 * usage and execution time a function of the input depth: there is no
 * static bound, so the task has no provable WCET. Raising the stack size
 * does not fix the unboundedness; the recursion must become iteration.
 *
 * The correct pattern (see examples/good/bounded_task.c) iterates over the
 * chain with a bounded loop and a named-constant trip count.
 *
 * The body intentionally demonstrates the banned construct; do not copy.
 * Flagged by rt_banned_patterns.py as self-recursion.
 */

#include <stdio.h>

typedef struct frame {
    int value;
    const struct frame *next;
} frame_t;

static frame_t chain[8];

static int sum_chain(const frame_t *f)
{
    if (f == NULL) {
        return 0;
    }
    return f->value + sum_chain(f->next);   /* RECURSION: unbounded stack + WCET */
}

int main(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        chain[i].value = i + 1;
        chain[i].next = (i < 7) ? &chain[i + 1] : NULL;
    }
    printf("sum=%d (recursive)\n", sum_chain(&chain[0]));
    return 0;
}
