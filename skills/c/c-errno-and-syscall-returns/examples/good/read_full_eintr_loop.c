/* GOOD: full-read loop. Handles all three read outcomes: n > 0 data,
   n == 0 EOF (normal), n < 0 error (read errno; retry on EINTR).
   Short counts are accumulated. EINTR retry is the POSIX contract; the
   Windows CRT shape is the same but rarely delivers EINTR. */
#include <errno.h>
#include <io.h>
#include <stdio.h>

static int read_all_good(int fd, char *buf, size_t want) {
    size_t done = 0;
    while (done < want) {
        int n = _read(fd, buf + done, (unsigned)(want - done));
        if (n < 0) {
            if (errno == EINTR) continue;    /* retry the interrupted call */
            return -1;                       /* real error: errno is valid here */
        }
        if (n == 0) break;                   /* EOF: not an error */
        done += (size_t)n;                   /* accumulate short reads */
    }
    return (int)done;
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
    int n = read_all_good(fd, small, sizeof small);  /* needs several reads */
    if (n != (int)sizeof small) {
        printf("BUG: read %d of %d bytes\n", n, (int)sizeof small);
        return 1;
    }
    printf("OK: read %d bytes across short reads\n", n);
    return 0;
}
