// BAD: snprintf return value ignored -> silent truncation (STR35-C).
// Compile: gcc -Wall -Wextra -O2 -Werror -o snprintf_bad snprintf_bad.c
// No warning by default; the bug is silent.
#include <stdio.h>
#include <string.h>

int main(void) {
    char host[16];
    const char *untrusted = "very.long.example.com.server.example";
    snprintf(host, sizeof host, "%s", untrusted);   // return value NOT checked
    if (strcmp(host, "very.long.example") == 0)     // truncated value used anyway
        puts("host truncated to 15 bytes, code still uses it");
    printf("host=%s\n", host);
    return 0;
}
