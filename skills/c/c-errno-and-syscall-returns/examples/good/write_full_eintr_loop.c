/* GOOD: full-write loop. write may accept fewer bytes than requested
   (short write); resume from buf + done with the remaining count. Retry
   only on EINTR (POSIX contract; Windows CRT shape is similar). Round-trip
   is verified by reading the file back. */
#include <errno.h>
#include <io.h>
#include <stdio.h>
#include <string.h>

static int write_all_good(int fd, const char *buf, size_t total) {
    size_t done = 0;
    while (done < total) {
        int w = _write(fd, buf + done, (unsigned)(total - done));
        if (w < 0) {
            if (errno == EINTR) continue;    /* retry the interrupted call */
            return -1;
        }
        done += (size_t)w;                   /* short write: continue after it */
    }
    return (int)done;
}

int main(void) {
    FILE *tf = tmpfile();
    if (!tf) return 2;
    int fd = _fileno(tf);
    const char data[] = "0123456789abcdefghij";  /* 20-byte payload */
    const unsigned total = (unsigned)(sizeof(data) - 1);

    if (write_all_good(fd, data, total) != (int)total) {
        printf("BUG: short write dropped data\n");
        return 1;
    }
    if (fflush(tf) != 0) return 2;
    if (_lseek(fd, 0, SEEK_SET) != 0L) return 2;

    char check[32] = {0};
    int n = _read(fd, check, sizeof check);
    if (n != (int)total || memcmp(check, data, total) != 0) {
        printf("BUG: round-trip mismatch (%d bytes)\n", n);
        return 1;
    }
    printf("OK: wrote and read back %d bytes\n", n);
    return 0;
}
