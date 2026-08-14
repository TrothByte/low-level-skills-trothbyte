// GOOD: the same shared counter, but main's read-modify-write is inside a
// PRIMASK critical section (__disable_irq/__enable_irq). While PRIMASK is set
// the ISR is pended (cannot preempt), so the RMW completes atomically with
// respect to the ISR and no update is lost. The ISR itself does not disable
// interrupts; it cannot be preempted by itself at the same priority.
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
    __disable_irq();            /* PRIMASK = 1: ISR is pended */
    unsigned tmp = shared_count;
    usleep(1000);
    shared_count = tmp + 1u;
    __enable_irq();             /* PRIMASK = 0: pending ISR runs */
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
