/* Stack-buffer program intended to be compiled WITH hardening flags.
 * Build (host):   gcc -O2 -fstack-protector-strong -fcf-protection=full \
 *                 -D_FORTIFY_SOURCE=2 -o hardened.exe hardened.c
 * Build (target): gcc -O2 -fstack-protector-strong -fcf-protection=full \
 *                 -D_FORTIFY_SOURCE=2 -pie -fPIE -Wl,-z,relro,-z,now \
 *                 -Wl,-z,noexecstack -o hardened hardened.c
 */
#include <stdio.h>
#include <string.h>

__attribute__((noinline)) static void process(const char *in)
{
    char buf[64]; /* stack array: eligible for canary under -fstack-protector-strong */
    strcpy(buf, in);
    printf("processed: %s\n", buf);
}

int main(int argc, char **argv)
{
    process(argc > 1 ? argv[1] : "hello");
    return 0;
}
