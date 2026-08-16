// BAD: double-checked locking without C11 atomics — the flag is `volatile int`,
// not `_Atomic`, and `data` is a plain `int`. This is a data race under
// iso-c11-n1570 §6.5p5 (UB). It usually "works" on x86 (TSO + in-order
// execution) but is demonstrably broken under a weak-memory model (AArch64,
// RISC-V) and is not portable.
// intentionally incorrect
#include <stdio.h>
#include <pthread.h>

static volatile int flag = 0;   // wrong: volatile is not atomic ordering
static int data = 0;            // wrong: plain int shared across threads

static void *producer(void *arg) {
    (void)arg;
    data = 42;                  // plain store
    flag = 1;                   // volatile store — no release semantics
    return NULL;
}

static void *consumer(void *arg) {
    (void)arg;
    while (flag == 0) { /* spin */ }   // volatile load — no acquire semantics
    printf("consumer saw data=%d\n", data); // UB: race with producer store
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
