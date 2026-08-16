/* GOOD: two threads acquire locks in a single global order (A then B).
 * No cycle exists in the dependency graph, so no interleaving can
 * deadlock; the program always completes with consistent results.
 * Compile+run: gcc -Wall -Wextra -Werror -O2 -pthread lock_order_pthread.c
 *   -o /tmp/d1.exe && /tmp/d1.exe
 */
#include <pthread.h>
#include <stdio.h>

#define THREADS 8
#define ITERS   20000

static pthread_mutex_t lock_a = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_b = PTHREAD_MUTEX_INITIALIZER;
static unsigned long shared_a, shared_b;

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        pthread_mutex_lock(&lock_a);        /* global order: A before B */
        shared_a++;
        pthread_mutex_lock(&lock_b);
        shared_b++;
        pthread_mutex_unlock(&lock_b);
        pthread_mutex_unlock(&lock_a);
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
    unsigned long expect = (unsigned long)THREADS * ITERS;
    if (shared_a != expect || shared_b != expect) {
        printf("BUG: inconsistent totals a=%lu b=%lu\n", shared_a, shared_b);
        return 1;
    }
    printf("GOOD: consistent lock order, a=%lu b=%lu\n", shared_a, shared_b);
    return 0;
}
