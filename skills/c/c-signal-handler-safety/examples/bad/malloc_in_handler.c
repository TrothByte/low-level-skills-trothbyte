/* BAD: malloc()/free() are called from a signal handler. The heap allocator
   uses internal locks; if the signal interrupted another allocation, the
   handler deadlocks on the same lock or leaves the heap inconsistent.
   malloc/free are not async-signal-safe (CERT SIG30-C). This fixture runs
   under a synchronous raise() on Windows, masking the bug — a false negative
   that "works in tests". */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static void on_sigint(int sig) {
    void *p;
    (void)sig;
    p = malloc(64);                          /* BAD: not async-signal-safe */
    if (p != NULL) {
        free(p);                             /* BAD: same heap lock */
    }
}

int main(void) {
    if (signal(SIGINT, on_sigint) == SIG_ERR) return 2;
    printf("[main] raising SIGINT\n");
    fflush(stdout);
    if (raise(SIGINT) != 0) return 2;
    printf("[main] returned: heap calls succeeded here (UB/deadlock on POSIX)\n");
    return 1;                                /* fixture reports SIG30-C violation */
}
