// GOOD: snprintf truncates and always NUL-terminates; truncation is detected.
// Compile: gcc -Wall -Wextra -Werror -O2 -o strncpy_good strncpy_good.c && ./strncpy_good
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *src = (argc > 1) ? argv[1] : "0123456789";
    char buf[8];
    int n = snprintf(buf, sizeof buf, "%s", src);
    assert(n >= 0);
    if ((size_t)n >= sizeof buf) {
        puts("truncated: value longer than buffer");
    }
    assert(buf[sizeof buf - 1] == '\0');   // termination guaranteed by snprintf
    return 0;
}
