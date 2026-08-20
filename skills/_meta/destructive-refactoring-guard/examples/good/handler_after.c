/* src/format_size.c — human-readable byte formatting, v3.2.
 *
 * Surgical fix for v3.1's integer-division precision bug: 1536 was
 * printed as "1 KB", 1048576 as "1 MB". The formatting lines now use
 * floating point with one decimal; the unit-selection logic and the
 * byte case are byte-for-byte identical to v3.1.
 */
#include <stdio.h>
#include <string.h>

static int format_size(long long bytes, char *out, size_t cap)
{
    if (cap < 24)
        return -1;
    if (bytes < 1024) {
        snprintf(out, cap, "%lld B", bytes);
        return 0;
    }
    if (bytes < 1048576) {
        double kb = (double)bytes / 1024.0;
        snprintf(out, cap, "%.1f KB", kb);
        return 0;
    }
    if (bytes < 1073741824) {
        double mb = (double)bytes / 1048576.0;
        snprintf(out, cap, "%.1f MB", mb);
        return 0;
    }
    double gb = (double)bytes / 1073741824.0;
    snprintf(out, cap, "%.1f GB", gb);
    return 0;
}

struct case_t {
    long long bytes;
    const char *expect;
};

static const struct case_t cases[] = {
    {512,            "512 B"},
    {1536,           "1.5 KB"},
    {1048576,        "1.0 MB"},
    {3145728,        "3.0 MB"},
    {1073741824LL,   "1.0 GB"},
};

int main(void)
{
    int fails = 0;
    size_t n = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0; i < n; i++) {
        char buf[32];
        if (format_size(cases[i].bytes, buf, sizeof buf) != 0 ||
            strcmp(buf, cases[i].expect) != 0) {
            printf("FAIL format_size(%lld) = \"%s\", expected \"%s\"\n",
                   cases[i].bytes, buf, cases[i].expect);
            fails++;
        } else {
            printf("PASS format_size(%lld) = \"%s\"\n", cases[i].bytes, buf);
        }
    }
    if (fails) {
        printf("format_size: %d case(s) wrong\n", fails);
        return 1;
    }
    printf("format_size: all %zu cases correct\n", n);
    return 0;
}
