// GOOD: snprintf return value checked before the truncated value is used.
// Compile: gcc -Wall -Wextra -Werror -O2 -o snprintf_good snprintf_good.c && ./snprintf_good
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *untrusted = (argc > 1) ? argv[1] : "good.example";
    char host[16];
    int n = snprintf(host, sizeof host, "%s", untrusted);
    assert(n >= 0);
    if ((size_t)n >= sizeof host) {
        puts("rejecting host: too long for buffer");   // truncation handled
        return 1;
    }
    assert(strcmp(host, untrusted) == 0);
    printf("host=%s\n", host);
    return 0;
}
