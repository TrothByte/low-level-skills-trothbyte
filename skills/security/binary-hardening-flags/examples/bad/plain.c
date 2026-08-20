/* Identical source to examples/good/hardened.c, compiled WITHOUT hardening
 * flags. Demonstrates that the binary properties (canary, FORTIFY, CET note,
 * PIE/RELRO on ELF targets) are NOT present just because the source is the
 * same. Compile and inspect the binary, never assume defaults.
 */
#include <stdio.h>
#include <string.h>

__attribute__((noinline)) static void process(const char *in)
{
    char buf[64];
    strcpy(buf, in);
    printf("processed: %s\n", buf);
}

int main(int argc, char **argv)
{
    process(argc > 1 ? argv[1] : "hello");
    return 0;
}
