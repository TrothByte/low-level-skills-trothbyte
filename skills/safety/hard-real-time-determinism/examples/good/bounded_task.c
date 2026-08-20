/*
 * bounded_task.c - GOOD PATTERN: hard real-time task model, host-runnable.
 *
 * Demonstrates the discipline the skill enforces:
 *   - static allocation only: no malloc/calloc/realloc/free/new/delete
 *   - bounded loops: named-constant trip counts, a counting `while`, and a
 *     blocking RTOS-shaped superloop that waits each period
 *   - monotonic timing: clock_gettime(CLOCK_MONOTONIC) for every deadline
 *     check, never gettimeofday or the wall clock
 *   - shared resources behind a priority-inheriting mutex model
 *   - measured maximum per-frame time compared against a fixed budget
 *
 * Deadline model: each frame must finish within FRAME_BUDGET_NS. Because the
 * frame timer uses the monotonic clock, NTP/timezone adjustments cannot move
 * the deadline.
 *
 * Compile and run (host, MinGW provides CLOCK_MONOTONIC):
 *   gcc -Wall -Wextra -Werror -O2 bounded_task.c -o bounded_task.exe
 *   ./bounded_task.exe
 *
 * On targets without clock_gettime(CLOCK_MONOTONIC), substitute the target
 * hardware tick timer and convert ticks to nanoseconds; the pattern is
 * identical.
 */

#include <stdio.h>
#include <time.h>

#define FRAMES              100000
#define FRAME_BUDGET_NS     1000000L   /* 1 ms worst-case budget per frame */
#define SCRATCH_BYTES       64
#define MAX_QUEUE_ITEMS     4

typedef struct {
    int holder;
    int owner_priority;
} mutex_t;

static unsigned char scratch[SCRATCH_BYTES];
static int queue[MAX_QUEUE_ITEMS];
static int queue_len;
static mutex_t shared = { -1, 0 };

static long diff_ns(const struct timespec *from, const struct timespec *to)
{
    return (long)(to->tv_sec - from->tv_sec) * 1000000000L
         + (long)(to->tv_nsec - from->tv_nsec);
}

static void mutex_lock_pi(mutex_t *m, int priority)
{
    /* model of a priority-inheriting mutex: the holder inherits the waiter's
     * priority, so blocking is bounded by the critical section length */
    (void)priority;
    m->holder = 1;
}

static void mutex_unlock(mutex_t *m)
{
    m->holder = -1;
}

static void process_frame(void)
{
    int i;
    unsigned int items = (unsigned int)queue_len;

    mutex_lock_pi(&shared, 10);

    for (i = 0; i < (int)(sizeof(scratch) / sizeof(scratch[0])); i++) {
        scratch[i] = (unsigned char)(scratch[i] + 1U);
    }

    while (items != 0) {            /* bounded counting loop: items decreases */
        items--;
        scratch[0] = (unsigned char)(scratch[0] ^ 0xA5U);
    }

    for (i = 0; i < queue_len; i++) {
        queue[i] = queue[i] + 1;
    }

    mutex_unlock(&shared);
}

static int event_wait(void)
{
    /* modeled blocking wait; in an RTOS this is a queue/semaphore take that
     * suspends the task, so the superloop consumes no CPU while idle */
    return 0;
}

void control_task(void *arg)
{
    (void)arg;
    for (;;) {                      /* RTOS task superloop: blocks each period */
        if (event_wait() == 0) {
            process_frame();
        }
    }
}

int main(void)
{
    struct timespec t_start;
    struct timespec t_end;
    long max_frame_ns = 0L;
    long elapsed;
    int iter;
    int deadline_missed = 0;

    queue_len = MAX_QUEUE_ITEMS;

    for (iter = 0; iter < FRAMES; iter++) {
        if (clock_gettime(CLOCK_MONOTONIC, &t_start) != 0) {
            return 1;
        }
        process_frame();
        if (clock_gettime(CLOCK_MONOTONIC, &t_end) != 0) {
            return 1;
        }
        elapsed = diff_ns(&t_start, &t_end);
        if (elapsed > max_frame_ns) {
            max_frame_ns = elapsed;
        }
        if (elapsed > FRAME_BUDGET_NS) {
            deadline_missed = 1;
        }
    }

    printf("frames=%d max_frame_ns=%ld budget_ns=%ld deadline_missed=%d\n",
           FRAMES, max_frame_ns, (long)FRAME_BUDGET_NS, deadline_missed);
    return deadline_missed;
}
