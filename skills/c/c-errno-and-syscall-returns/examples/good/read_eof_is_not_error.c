/* GOOD: read() returning 0 means EOF, a normal state, not an error.
   The loop terminates exactly at EOF; errno is not consulted for a read
   that returns 0. Only n < 0 is an error. */
#include <errno.h>
#include <io.h>
#include <stdio.h>

int main(void) {
    FILE *tf = tmpfile();
    if (!tf) return 2;
    int fd = _fileno(tf);
    if (_write(fd, "abc", 3) != 3) return 2;
    if (fflush(tf) != 0) return 2;
    if (_lseek(fd, 0, SEEK_SET) != 0L) return 2;

    char buf[4] = {0};
    int n = _read(fd, buf, sizeof buf);     /* 3 bytes: data */
    if (n != 3) return 2;
    int again = _read(fd, buf, sizeof buf); /* 0: EOF, not an error */
    if (again != 0) return 2;               /* EOF is the expected normal exit */
    printf("OK: read 3 bytes, then EOF cleanly (return 0)\n");
    return 0;
}
