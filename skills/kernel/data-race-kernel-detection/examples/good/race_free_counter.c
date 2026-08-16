/* GOOD: a shared counter protected by a mutex. No conflicting accesses on
 * any interleaving: the count is deterministic and equal to the number of
 * increments. This is the "do not flag" fixture.
 * Compile+run: gcc -Wall -Wextra -Werror -O2 -pthread race_free_counter.c
 *   -o /tmp/c1.exe && /tmp/c1.exe
 */
#include <pthread.h>
#include <stdio.h>

#define THREADS 4
#define ITERS   100000

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned long count;

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        pthread_mutex_lock(&lock);
        count++;
        pthread_mutex_unlock(&lock);
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
    if (count != expected) {
        printf("BUG: count=%lu expected=%lu\n", count, expected);
        return 1;
    }
    printf("GOOD: count=%lu == expected=%lu, no lost updates\n", count, expected);
    return 0;
}
