/* GOOD: open/close handling. open reports failure as -1, never via
   truthiness (0 is a valid descriptor). errno is read immediately after
   the failing call. close failure is checked on the write path. */
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <sys/stat.h>

int main(void) {
    const char *path = "errno-open-demo.tmp";
    int fd = _open(path, _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY,
                   _S_IREAD | _S_IWRITE);
    if (fd == -1) {                        /* GOOD: -1 is the failure sentinel */
        printf("open failed: errno=%d\n", errno);
        return 1;
    }
    if (_close(fd) == -1) {                /* GOOD: close result checked */
        printf("close failed: errno=%d\n", errno);
        return 1;
    }
    (void)_unlink(path);
    printf("OK: open/close handled\n");
    return 0;
}
