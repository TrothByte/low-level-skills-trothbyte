// Atomic state write: temp file + rename, so an interrupted write never leaves
// a partial main file and a reader never observes mid-write bytes.
#include <stddef.h>
#include <stdio.h>

int write_state_atomic(const char *path, const void *data, size_t size) {
    char tmp[4096];
    int ok;
    FILE *f;

    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp)
        return -1;

    f = fopen(tmp, "wb");
    if (!f)
        return -1;
    ok = fwrite(data, 1, size, f) == size && fflush(f) == 0;
    if (fclose(f) != 0)
        ok = 0;
    if (!ok) {
        remove(tmp);
        return -1;
    }
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
}
