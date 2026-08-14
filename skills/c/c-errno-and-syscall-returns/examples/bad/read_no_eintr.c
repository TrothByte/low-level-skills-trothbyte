/* BAD: a single-shot read with no EINTR retry and no short-count accumulation.
   On POSIX, read may return fewer bytes than requested and may fail with EINTR
   before transferring anything; both cases require a loop. Here a 20-byte
   payload read through an 8-byte buffer silently truncates. Bug class A18 /
   CERT ERR30-C. EINTR semantics are POSIX; the Windows CRT shape is similar but
   rarely delivers EINTR. */
#include <errno.h>
#include <io.h>
#include <stdio.h>

static int read_all_bad(int fd, char *buf, unsigned want) {
    int n = _read(fd, buf, want);        /* BAD: one call, no EINTR retry, no loop */
    if (n < 0) return -1;                /* an interrupted read is abandoned */
    return n;                            /* BAD: short count treated as "all" by caller */
}

int main(void) {
    FILE *tf = tmpfile();
    if (!tf) return 2;
    int fd = _fileno(tf);
    const char data[] = "0123456789abcdefghij";  /* 20-byte payload */
    const unsigned total = (unsigned)(sizeof(data) - 1);
    if (_write(fd, data, total) != (int)total) return 2;
    if (fflush(tf) != 0) return 2;
    if (_lseek(fd, 0, SEEK_SET) != 0L) return 2;

    char small[8] = {0};
    int n = read_all_bad(fd, small, sizeof small);
    if (n != (int)total) {
        printf("BUG: read %d of %u bytes, the rest is lost\n", n, total);
        return 1;
    }
    printf("OK\n");
    return 0;
}
