/* BAD: errno is checked after a successful call, with a stale value from
   an earlier failure. Library functions never reset errno to 0 (N1570 7.5),
   so this parse reports a false range error. Bug class A17 / CERT ERR30-C. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static long parse_bad(const char *s) {
    long v = strtol(s, NULL, 10);        /* BAD: errno not cleared before the call */
    if (errno == ERANGE) return -1;      /* BAD: stale ERANGE from a prior call */
    return v;
}

int main(void) {
    errno = ERANGE;                      /* left over from an earlier failed call */
    long v = parse_bad("123");
    if (v != 123) {
        printf("BUG: valid input misreported as a range error (errno=%d)\n", errno);
        return 1;
    }
    printf("OK\n");
    return 0;
}
