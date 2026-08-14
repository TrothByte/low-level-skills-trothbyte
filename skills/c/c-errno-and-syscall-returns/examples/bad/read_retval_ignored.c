/* BAD: the return value of _read is ignored, so EOF becomes invisible.
   A read at end-of-file returns 0, not an error, and the buffer is untouched;
   code that ignores the return value reprocesses stale data forever. Bug
   class A18 / CERT ERR33-C; see CVE-2024-32650 (rustls complete_io). */
#include <errno.h>
#include <io.h>
#include <stdio.h>

static int read_next_batch_bad(int fd, char *buf, unsigned cap) {
    _read(fd, buf, cap);                 /* BAD: return value ignored */
    return 1;                            /* BAD: always claims data was produced */
}

int main(void) {
    FILE *tf = tmpfile();
    if (!tf) return 2;
    int fd = _fileno(tf);
    const char line[] = "hello\n";
    if (_write(fd, line, 6) != 6) return 2;
    if (fflush(tf) != 0) return 2;
    if (_lseek(fd, 0, SEEK_SET) != 0L) return 2;

    char batch[16] = {0};
    int batches = 0;
    while (read_next_batch_bad(fd, batch, sizeof batch)) {
        batches++;
        if (batches > 1) {
            printf("BUG: EOF was not detected; stale data reprocessed\n");
            return 1;
        }
        printf("batch %d: %s", batches, batch);
    }
    printf("OK\n");
    return 0;
}
