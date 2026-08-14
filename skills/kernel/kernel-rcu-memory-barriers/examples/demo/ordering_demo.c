// User-space demo of kernel-style ordering using the host stubs from
// linux_stubs.h. smp_wmb()/smp_rmb() here are compiler barriers
// (asm volatile("" ::: "memory")); on a real kernel they may additionally be
// hardware fences on weakly-ordered CPUs.
//
// Compile: gcc -Wall -Wextra -Werror -O2 -pthread ordering_demo.c -o ordering_demo
// Run:     ./ordering_demo fenced   (correct pattern; this is the VERIFIED run)
//          ./ordering_demo racy     (barriers removed; data race / UB, demo only)
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "../linux_stubs.h"

#define N 4
#define ROUNDS 200000L

struct shared {
    int data[N];
    int ready;
};

struct shared_racy {
    int data[N];
    volatile int ready;   // volatile only so the spin loop compiles; data[]
};                        // still has no ordering: the pattern is a data race

static pthread_barrier_t bar;

static struct shared fenced;
static struct shared_racy racy;

static void *fenced_publisher(void *arg)
{
    (void)arg;
    for (long r = 0; r < ROUNDS; r++) {
        for (int i = 0; i < N; i++) {
            fenced.data[i] = (int)(r * 100 + i);  // plain stores, ordered by wmb
        }
        smp_wmb();                               // payload must land before flag
        WRITE_ONCE(fenced.ready, 1);
        pthread_barrier_wait(&bar);              // consumer finished reading
        WRITE_ONCE(fenced.ready, 0);
        smp_mb();                                // reset visible before next round
        pthread_barrier_wait(&bar);
    }
    return NULL;
}

static void *fenced_consumer(void *arg)
{
    (void)arg;
    for (long r = 0; r < ROUNDS; r++) {
        while (!READ_ONCE(fenced.ready)) {
        }
        smp_rmb();                               // flag load before payload loads
        for (int i = 0; i < N; i++) {
            if (fenced.data[i] != (int)(r * 100 + i)) {
                fprintf(stderr, "FENCED MISMATCH round %ld data[%d]=%d\n",
                        r, i, fenced.data[i]);
                exit(1);
            }
        }
        pthread_barrier_wait(&bar);
        pthread_barrier_wait(&bar);
    }
    return NULL;
}

static void *racy_publisher(void *arg)
{
    (void)arg;
    for (long r = 0; r < ROUNDS; r++) {
        for (int i = 0; i < N; i++) {
            racy.data[i] = (int)(r * 100 + i);   // no barrier: may land late
        }
        racy.ready = 1;                          // no release
        pthread_barrier_wait(&bar);
        racy.ready = 0;
        pthread_barrier_wait(&bar);
    }
    return NULL;
}

static void *racy_consumer(void *arg)
{
    (void)arg;
    for (long r = 0; r < ROUNDS; r++) {
        while (!racy.ready) {
        }
        for (int i = 0; i < N; i++) {
            if (racy.data[i] != (int)(r * 100 + i)) {
                fprintf(stderr, "RACY MISMATCH round %ld data[%d]=%d\n",
                        r, i, racy.data[i]);
                exit(1);
            }
        }
        pthread_barrier_wait(&bar);
        pthread_barrier_wait(&bar);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int mode_racy = (argc > 1 && argv[1][0] == 'r');

    if (mode_racy) {
        printf("ordering_demo: racy mode (barriers removed -- data race / UB,"
               " result meaningless; on x86 TSO it often still passes)\n");
    } else {
        printf("ordering_demo: fenced mode (smp_wmb/smp_rmb compiler barriers"
               " + smp_mb fence)\n");
    }

    if (pthread_barrier_init(&bar, NULL, 2) != 0) {
        return 2;
    }

    pthread_t t1, t2;
    if (mode_racy) {
        pthread_create(&t1, NULL, racy_publisher, NULL);
        pthread_create(&t2, NULL, racy_consumer, NULL);
    } else {
        pthread_create(&t1, NULL, fenced_publisher, NULL);
        pthread_create(&t2, NULL, fenced_consumer, NULL);
    }
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_barrier_destroy(&bar);

    printf("PASS: %ld rounds, all payloads consistent\n", ROUNDS);
    return 0;
}
