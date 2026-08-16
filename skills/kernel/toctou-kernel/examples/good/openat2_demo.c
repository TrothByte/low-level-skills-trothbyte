// GOOD: Linux openat2 with RESOLVE_* flags — path resolution and the
// permission/beneath constraints are atomic. Compile and run on a Linux
// host with a recent kernel (openat2 is a syscall since 5.6). This host is
// Windows; the fixture is provided as a compilable-and-runnable stub for the
// documented target command.
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

struct open_how {
    unsigned long flags;
    unsigned int mode;
    unsigned int resolve;
};

#ifndef __NR_openat2
#define __NR_openat2 437
#endif

#define RESOLVE_BENEATH     0x08
#define RESOLVE_NO_MAGICLINKS 0x02

int main(void) {
    // GOOD: resolution happens once, atomically, and refuses to escape the
    // directory or follow magic links — the check and the open are the SAME
    // operation, so there is no TOCTOU window for path replacement.
    struct open_how how = {
        .flags = O_RDONLY,
        .mode = 0,
        .resolve = RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS,
    };
    int fd = (int)syscall(__NR_openat2, AT_FDCWD, "subdir/file.txt", &how,
                          sizeof(how));
    if (fd < 0) {
        printf("openat2 failed: errno=%d\n", errno);
        return 1;
    }
    printf("openat2 succeeded (fd=%d)\n", fd);
    close(fd);
    return 0;
}
