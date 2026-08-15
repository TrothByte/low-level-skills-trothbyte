// intentionally incorrect
// Writes the state file in place. A SIGTERM between fwrite and fclose leaves a
// truncated file that still passes existence checks, so a later build trusts a
// corrupt deps/state file. The correct pattern is temp file + rename.
#include <stddef.h>
#include <stdio.h>

int save_state(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");        /* kill here -> partial/corrupt file */
    size_t n;
    int ok;

    if (!f)
        return -1;
    n = fwrite(data, 1, size, f);       /* kill here -> truncated file */
    ok = fclose(f) == 0 && n == size;
    return ok ? 0 : -1;
}
