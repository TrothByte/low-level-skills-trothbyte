/* BAD: AB-BA lock inversion. Thread 1 takes A then B; thread 2 takes B then
 * A. A watchdog thread detects the resulting hang and aborts with a
 * distinct exit code, so the fixture is deterministic and safe to run.
 * The deadlock exists as a *class* regardless of how often a lucky run
 * completes.
 * Compile+run: gcc -Wall -Wextra -Werror -O2 -pthread abba_deadlock_pthread.c
 *   -o /tmp/d2.exe && /tmp/d2.exe   (expect exit 2: DEADLOCK DETECTED)
 * Marker: intentionally incorrect
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t lock_a = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_b = PTHREAD_MUTEX_INITIALIZER;
static volatile int done_work;

static void *t1(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&lock_a);            /* A first */
    usleep(10000);                          /* widen the window */
    pthread_mutex_lock(&lock_b);            /* then B   */
    done_work = 1;
    pthread_mutex_unlock(&lock_b);
    pthread_mutex_unlock(&lock_a);
    return NULL;
}

static void *t2(void *arg)
{
    (void)arg;
    /* intentionally incorrect: opposite acquisition order B then A */
    pthread_mutex_lock(&lock_b);            /* B first  */
    usleep(10000);
    pthread_mutex_lock(&lock_a);            /* then A   */
    done_work = 1;
    pthread_mutex_unlock(&lock_a);
    pthread_mutex_unlock(&lock_b);
    return NULL;
}

static void *watchdog(void *arg)
{
    (void)arg;
    usleep(1000000);
    if (!done_work) {
        fprintf(stderr, "DEADLOCK DETECTED: threads A then B vs B then A "
                        "(AB-BA inversion)\n");
        _exit(2);
    }
    return NULL;
}

int main(void)
{
    pthread_t a, b, w;
    pthread_create(&a, NULL, t1, NULL);
    pthread_create(&b, NULL, t2, NULL);
    pthread_create(&w, NULL, watchdog, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    pthread_join(w, NULL);
    printf("completed without deadlock (a lucky run; the inversion class "
           "is still a deadlock)\n");
    return 0;
}
