// GOOD: publish-consume with C11 atomics — release store + acquire load on the
// same location. Correct on x86, AArch64, and RISC-V. Compile:
//   gcc -O2 -Wall -Wextra -pthread wakeup_flag.c -o /tmp/wakeup && /tmp/wakeup
#include <stdatomic.h>
#include <stdio.h>
#include <pthread.h>

static _Atomic int flag = 0;
static int data = 0;

static void *producer(void *arg) {
    (void)arg;
    data = 42;                                   // plain write, published by...
    atomic_store_explicit(&flag, 1, memory_order_release); // ...this release
    return NULL;
}

static void *consumer(void *arg) {
    (void)arg;
    while (atomic_load_explicit(&flag, memory_order_acquire) == 0) {
        /* spin until the release-store becomes visible */
    }
    printf("consumer saw data=%d\n", data);      // guaranteed 42 (happens-before)
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, producer, NULL);
    pthread_create(&t2, NULL, consumer, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}
