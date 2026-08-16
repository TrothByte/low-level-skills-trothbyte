/* BAD: "kmalloc(GFP_KERNEL)" style allocation while holding a spinlock.
 * The modeled sleep-capable call happens under the lock; on a real kernel
 * CONFIG_DEBUG_ATOMIC_SLEEP emits "sleeping function called from invalid
 * context" and the sleep is a deadlock vector for the whole CPU. Here the
 * blocking syscall under the lock models the pattern.
 * Compile+run: gcc -Wall -Wextra -Werror -O2 -pthread sleep_under_lock.c
 *   -o /tmp/d4.exe && /tmp/d4.exe
 * Marker: intentionally incorrect
 */
#include <pthread.h>
#include <stdio.h>
#include <time.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned long counter;

static void sleep_capable_alloc(void)
{
    /* models kmalloc(GFP_KERNEL): may block/schedule */
    struct timespec ts = {0, 1000000L};   /* 1 ms sleep */
    nanosleep(&ts, NULL);
}

static void *worker(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&lock);
    /* intentionally incorrect: sleep-capable call while holding a
       spinlock-class lock; on a real kernel this trips DEBUG_ATOMIC_SLEEP
       and can deadlock the CPU. */
    sleep_capable_alloc();
    counter++;
    pthread_mutex_unlock(&lock);
    return NULL;
}

int main(void)
{
    pthread_t t[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&t[i], NULL, worker, NULL);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(t[i], NULL);
    }
    printf("BAD: sleep-capable call executed while holding the lock "
           "(counter=%lu); target kernel: 'sleeping function called from "
           "invalid context' splat, pre-allocate or use GFP_ATOMIC\n",
           counter);
    return 0;
}
