#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>

#define NTHREADS 4
#define ITERS    200000000ULL

static volatile int active = 0;        /* threads inside the region */
static volatile int max_active = 0;    /* max overlapping started threads */
static volatile int working = 0;       /* threads actually doing CPU work */
static volatile int max_working = 0;   /* max overlapping working threads */
static uint64_t result_holder[NTHREADS];

static void bump(volatile int *cnt, volatile int *max)
{
    int n = __sync_add_and_fetch(cnt, 1);
    int m;
    do {
        m = *max;
    } while (m < n && !__sync_bool_compare_and_swap(max, m, n));
}

static uint64_t burn(uint64_t seed, uint64_t iters)
{
    uint64_t x = seed;
    for (uint64_t i = 0; i < iters; i++) {
        x = x * 6364136223846793005ULL + i;
        x ^= x >> 33;
    }
    return x;
}

static void *worker(void *arg)
{
    intptr_t i = (intptr_t)arg;
    bump(&active, &max_active);
    bump(&working, &max_working);
    result_holder[i] = burn(0x1234 + (uint64_t)i, ITERS);
    __sync_sub_and_fetch(&working, 1);
    __sync_sub_and_fetch(&active, 1);
    return NULL;
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(void)
{
    pthread_t t[NTHREADS];
    double t0 = now_sec();
    for (int i = 0; i < NTHREADS; i++) {
        pthread_create(&t[i], NULL, worker, (void *)(intptr_t)i);
    }
    for (int i = 0; i < NTHREADS; i++) {
        pthread_join(t[i], NULL);
    }
    double wall = now_sec() - t0;
    DWORD cores = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    printf("real_parallel: threads=%d cores=%lu wall=%.3fs "
           "max_started=%d max_working=%d\n",
           NTHREADS, (unsigned long)cores, wall, max_active, max_working);
    for (int i = 0; i < NTHREADS; i++) {
        printf("  result[%d] = %016llx\n", i, (unsigned long long)result_holder[i]);
    }
    return 0;
}
