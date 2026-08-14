// BAD: assuming volatile = atomic.
// Two threads perform a wide read-modify-write on a volatile counter. volatile
// forces each access to memory but does NOT make the load+store atomic: the
// threads interleave in the gap and updates are lost. With 2x50 increments the
// final counter is well below 100, so this program returns nonzero.
#include <pthread.h>
#include <unistd.h>

static volatile int counter;

static void *rmw(void *arg) {
    (void)arg;
    for (int i = 0; i < 50; i++) {
        int tmp = counter;
        usleep(2000);
        counter = tmp + 1;
    }
    return NULL;
}

int main(void) {
    pthread_t a, b;
    pthread_create(&a, NULL, rmw, NULL);
    pthread_create(&b, NULL, rmw, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    return counter == 100 ? 0 : 1;
}
