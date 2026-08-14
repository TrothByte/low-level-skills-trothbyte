// BAD: shared counter incremented by main and by the ISR with no critical
// section. count++ is load-modify-store; on a single core the ISR can preempt
// main between the load and the store, and both write back the same value, so
// updates are lost. On the host the ISR is a thread performing the same wide
// RMW (which runs only while PRIMASK is clear); main does not mask interrupts,
// the two RMWs interleave, and the final count is below 2 x ITERATIONS.
#include <unistd.h>
#include "../cortex_m_stubs.h"

#define ITERATIONS 40u

static volatile unsigned shared_count;

static void isr_inc(void) {
    unsigned tmp = shared_count;
    usleep(1000);
    shared_count = tmp + 1u;
}

static void *isr_thread(void *arg) {
    (void)arg;
    for (unsigned i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&g_primask);
        isr_inc();
        pthread_mutex_unlock(&g_primask);
    }
    return NULL;
}

static void main_inc(void) {
    /* Missing __disable_irq()/__enable_irq(): the ISR may preempt here. */
    unsigned tmp = shared_count;
    usleep(1000);
    shared_count = tmp + 1u;
}

int main(void) {
    pthread_t t;
    pthread_create(&t, NULL, isr_thread, NULL);
    for (unsigned i = 0; i < ITERATIONS; i++) {
        main_inc();
    }
    pthread_join(t, NULL);
    unsigned expected = 2u * ITERATIONS;
    int ok = (shared_count == expected) && !stub_has_failed();
    return ok ? 0 : 1;
}
