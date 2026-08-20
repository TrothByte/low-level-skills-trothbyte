/* Buffer overflow that the stack canary detects when compiled with
 * -fstack-protector-strong, and that compiles silently either way.
 *
 * Build WITH canary:    gcc -O2 -fstack-protector-strong -o canary_on.exe canary_bypass.c
 * Build WITHOUT canary: gcc -O2 -o canary_off.exe canary_bypass.c
 *
 * Expected on this host: canary_on.exe aborts with a stack-smashing exit
 * (nonzero); canary_off.exe behavior is recorded in evals/README.md. The
 * point: identical source, different binary property.
 */
#include <stdio.h>
#include <string.h>

__attribute__((noinline)) static int vuln(const char *in)
{
    char buf[8]; /* small stack array -> canary inserted under -fstack-protector-strong */
    strcpy(buf, in);
    puts(buf);
    return 0;
}

int main(int argc, char **argv)
{
    char payload[24];
    /* 15 'A' + NUL: 16 bytes written. On this host the canary sits 8 bytes
     * above the 8-byte buffer and the saved return address 24 bytes above
     * it, so the overflow clobbers the canary but NOT the return address. */
    memset(payload, 'A', 15);
    payload[15] = '\0';
    vuln(payload);
    puts("RETURNED NORMALLY");
    return 0;
}
