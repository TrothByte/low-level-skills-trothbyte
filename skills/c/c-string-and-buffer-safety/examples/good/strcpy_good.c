// GOOD: bounded copy with explicit truncation check instead of strcpy/strcat.
// Compile: gcc -Wall -Wextra -Werror -O2 -o strcpy_good strcpy_good.c && ./strcpy_good
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *user_input = (argc > 1) ? argv[1] : "alice";
    char name[16];
    int n = snprintf(name, sizeof name, "%s.log", user_input);
    assert(n >= 0);
    if ((size_t)n >= sizeof name) {
        puts("rejected: input too long");      // no overflow, no silent truncation
        return 1;
    }
    assert(name[sizeof name - 1] == '\0');
    printf("name=%s\n", name);
    return 0;
}
