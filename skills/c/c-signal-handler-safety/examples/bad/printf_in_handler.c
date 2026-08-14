/* BAD: printf() is called from a signal handler. printf is NOT on the POSIX
   async-signal-safe list (CERT SIG30-C): it locks stdio internals, and the
   interrupted code may already hold that lock, so the handler deadlocks or
   corrupts stdio buffers. This fixture still runs under a synchronous
   raise() on Windows, which is exactly the false-negative trap: passing this
   test does not make the handler safe. */
#include <signal.h>
#include <stdio.h>

static void on_sigint(int sig) {
    (void)sig;
    printf("interrupted!\n");                /* BAD: not async-signal-safe */
}

int main(void) {
    if (signal(SIGINT, on_sigint) == SIG_ERR) return 2;
    printf("[main] raising SIGINT\n");
    fflush(stdout);
    if (raise(SIGINT) != 0) return 2;
    printf("[main] returned: hazard is latent here (UB/deadlock on POSIX)\n");
    return 1;                                /* fixture reports SIG30-C violation */
}
