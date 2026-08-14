/* GOOD: a signal handler that does only async-signal-safe work.
   It sets a volatile sig_atomic_t flag (the only shared-object type a handler
   may touch, C11 7.14.1.1 / CERT SIG31-C) and writes one line with _write
   (write is on the POSIX async-signal-safe list; _write is the CRT equivalent).
   All real work happens in main() after the handler returns. */
#include <io.h>
#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t g_sigint_seen = 0;

static void on_sigint(int sig) {
    char msg[] = "[handler] SIGINT received\n";
    (void)sig;
    g_sigint_seen = 1;                       /* only volatile sig_atomic_t */
    (void)_write(1, msg, sizeof(msg) - 1);   /* async-signal-safe write */
}

int main(void) {
    if (signal(SIGINT, on_sigint) == SIG_ERR) {
        printf("install failed\n");
        return 2;
    }
    printf("[main] raising SIGINT\n");
    fflush(stdout);                          /* flush before the handler writes */
    if (raise(SIGINT) != 0) {
        printf("raise failed\n");
        return 2;
    }
    if (!g_sigint_seen) {
        printf("BUG: flag was not set by the handler\n");
        return 1;
    }
    printf("[main] handler ran, flag=%d; cleanup happens in the main loop\n",
           (int)g_sigint_seen);
    return 0;
}
