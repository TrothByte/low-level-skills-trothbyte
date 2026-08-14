// GOOD: ISR-to-main flag shared with an ISR, declared volatile.
// A single-producer/single-consumer flag on one core is the canonical use of
// volatile: the polling loop must re-read memory every iteration.
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

static int wait_for_flag(volatile const int *f) {
    unsigned spins = 0;
    while (*f == 0) {
        if (++spins > 2000000000u) return 0;
    }
    return 1;
}

int main(void) {
    pthread_t t;
    pthread_create(&t, NULL, isr_sim, NULL);
    int ok = wait_for_flag(&isr_flag);
    pthread_join(t, NULL);
    return ok ? 0 : 1;
}
