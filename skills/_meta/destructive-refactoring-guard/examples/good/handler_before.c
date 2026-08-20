/* src/format_size.c — human-readable byte formatting, v3.1.
 *
 * Formats a byte count as B/KB/MB/GB. v3.1 has a precision bug:
 * integer division truncates fractional units (1536 -> "1 KB",
 * 1048576 -> "1 MB"). The fix in v3.2 changes only the formatting
 * lines; the unit-selection logic is untouched.
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
        snprintf(out, cap, "%lld KB", bytes / 1024);
        return 0;
    }
    if (bytes < 1073741824) {
        snprintf(out, cap, "%lld MB", bytes / 1048576);
        return 0;
    }
    snprintf(out, cap, "%lld GB", bytes / 1073741824);
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
