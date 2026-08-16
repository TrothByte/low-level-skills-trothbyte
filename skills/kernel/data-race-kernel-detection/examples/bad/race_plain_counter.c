/* BAD: a shared counter updated by four threads with a non-atomic
 * read-modify-write. `count = count + 1` is a load/inc/store that is NOT
 * atomic: the plain accesses conflict and form a data race (UB). The
 * counter is volatile to stop the compiler from fusing the RMW into a
 * single (atomic-on-x86) instruction — with the fused `incq` the race
 * stays invisible on x86, which is exactly the "passes on my machine"
 * trap. This fixture makes the lost updates observable on x86.
 * Compile+run: gcc -Wall -Wextra -Werror -O2 -pthread race_plain_counter.c
 *   -o /tmp/c2.exe && /tmp/c2.exe
 * Marker: intentionally incorrect
 */
#include <pthread.h>
#include <stdio.h>

#define THREADS 4
#define ITERS   100000

static volatile unsigned long count; /* intentionally incorrect: shared */

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        count = count + 1;    /* intentionally incorrect: non-atomic RMW,
                                 data race (UB) */
    }
    return NULL;
}

int main(void)
{
    pthread_t t[THREADS];
    for (int i = 0; i < THREADS; i++) {
        pthread_create(&t[i], NULL, worker, NULL);
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(t[i], NULL);
    }
    unsigned long expected = (unsigned long)THREADS * ITERS;
    printf("count=%lu expected=%lu\n", count, expected);
    if (count != expected) {
        printf("DEMONSTRATED: lost updates observed -> data race is real\n");
        return 2;
    }
    printf("This run happened to show no lost updates on x86 TSO; the data "
           "race is still present (UB, may reproduce on other CPUs)\n");
    return 0;
}
