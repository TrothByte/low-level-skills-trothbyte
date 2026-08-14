/* BAD: errno is read too late, after an intervening library call that
   overwrites it. Any intervening call may write errno (cppreference errno
   page); here a failed fopen clobbers the check. Bug class A17 / CERT ERR33-C. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static long parse_late_bad(const char *s) {
    errno = 0;
    long v = strtol(s, NULL, 10);        /* succeeds: 123 */

    FILE *probe = fopen("no-such-file-for-errno-eval.txt", "r");
    if (probe) fclose(probe);            /* fails, sets errno = ENOENT */

    if (errno != 0) return -1;           /* BAD: errno no longer belongs to strtol */
    return v;
}

int main(void) {
    long v = parse_late_bad("123");
    if (v != 123) {
        printf("BUG: errno=%d clobbered before the check, success reported as failure\n", errno);
        return 1;
    }
    printf("OK\n");
    return 0;
}
