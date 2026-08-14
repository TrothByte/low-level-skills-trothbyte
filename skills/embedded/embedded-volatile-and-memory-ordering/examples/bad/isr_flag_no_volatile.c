// BAD: ISR flag declared/accessed WITHOUT volatile.
// The polling loop reads through a non-volatile pointer; at -O2 the load is
// hoisted out of the loop (or the loop collapses to a single check), so the
// main loop never observes the ISR store and times out (returns nonzero).
// The ISR is simulated with a helper thread (POSIX threads).
#include <pthread.h>
#include <unistd.h>

static volatile int isr_flag;

static void *isr_sim(void *arg) {
    (void)arg;
    usleep(5000);
    isr_flag = 1;
    return NULL;
}

static int wait_for_flag(const int *f) {
    unsigned spins = 0;
    while (*f == 0) {
        if (++spins > 2000000000u) return 0;
    }
    return 1;
}

int main(void) {
    pthread_t t;
    pthread_create(&t, NULL, isr_sim, NULL);
    int ok = wait_for_flag((const int *)&isr_flag);
    pthread_join(t, NULL);
    return ok ? 0 : 1;
}
