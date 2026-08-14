/* GOOD: errno discipline for a call whose only failure signal is errno.
   Clear errno before the call, check the failure sentinel, then read errno
   immediately on the failure path only (N1570 7.5; CERT ERR30-C). */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static long parse_good(const char *s) {
    char *end = NULL;
    errno = 0;                               /* clear before the call */
    long v = strtol(s, &end, 10);
    (void)end;
    if (errno == ERANGE) return -1;          /* errno read immediately, only here */
    return v;                                /* success path ignores errno */
}

int main(void) {
    errno = ERANGE;                          /* stale value must not matter */
    long v = parse_good("123");
    if (v != 123) {
        printf("BUG: parsed %ld\n", v);
        return 1;
    }
    printf("OK: parsed 123\n");
    return 0;
}
