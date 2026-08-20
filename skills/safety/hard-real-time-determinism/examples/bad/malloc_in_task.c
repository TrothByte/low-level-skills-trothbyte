/*
 * malloc_in_task.c - BANNED PATTERN: dynamic allocation inside a task.
 *
 * Hard real-time tasks may not call malloc/calloc/realloc/free: the heap is
 * a shared resource, allocation time is unbounded (first-fit scans, arena
 * locking, coalescing), the heap fragments so later allocations fail at
 * runtime, and ISR context usually has no usable heap at all. The "only at
 * init" argument fails because hidden re-entrancy (printf paths, RTOS
 * internals) allocates in task context.
 *
 * This fixture compiles and runs on the host; the right pattern is static
 * allocation at system init with fixed-size pools.
 *
 * The body intentionally demonstrates the banned constructs; do not copy.
 * Flagged by rt_banned_patterns.py as dynamic allocation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FRAMES 4

static char *rx_buf;

static int rx_task(void)
{
    int i;

    for (i = 0; i < MAX_FRAMES; i++) {
        rx_buf = malloc(128 + i);          /* BANNED: malloc in task path */
        if (rx_buf != NULL) {
            memset(rx_buf, 0, (size_t)(128 + i));
            free(rx_buf);                  /* BANNED: free in task path */
        }
    }
    return 0;
}

static int telemetry_task(void)
{
    char *tmp = calloc(64, 1);             /* BANNED: calloc in task path */
    if (tmp == NULL) {
        return -1;
    }
    tmp = realloc(tmp, 256);               /* BANNED: realloc in task path */
    free(tmp);                             /* BANNED: free in task path */
    return 0;
}

int main(void)
{
    rx_task();
    telemetry_task();
    printf("ran\n");
    return 0;
}
