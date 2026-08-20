/* incremental_fix.c — hex decoder + djb2 checksum utility, v3.2.
 *
 * A real bug was reported: odd-length hex strings were decoded by
 * right-padding ("ABC" -> AB 0C), corrupting checksums. v3.2 fixes ONLY
 * the odd-length branch (left-pad with a leading zero nibble, so "ABC"
 * -> 0A BC). Every other behavior — case-insensitivity, whitespace
 * skipping, invalid-character rejection, checksum — is unchanged from
 * v3.1. This is the surgical-fix shape this skill defends.
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_decode(const char *s, unsigned char *out, size_t *out_len)
{
    const char *p = s;
    while (isspace((unsigned char)*p))
        p++;
    const char *q = p;
    while (isxdigit((unsigned char)*q))
        q++;
    size_t len = (size_t)(q - p);
    if (len == 0)
        return -1;
    const char *r = q;
    while (isspace((unsigned char)*r))
        r++;
    if (*r != '\0')
        return -1;
    size_t i = 0;
    if (len % 2 == 1) {
        int lo = hex_nibble(p[0]);
        if (lo < 0)
            return -1;
        out[i++] = (unsigned char)lo;
        p++;
    }
    while (p < q) {
        int a = hex_nibble(p[0]);
        int b = hex_nibble(p[1]);
        if (a < 0 || b < 0)
            return -1;
        out[i++] = (unsigned char)((a << 4) | b);
        p += 2;
    }
    *out_len = i;
    return 0;
}

static unsigned long checksum(const unsigned char *b, size_t n)
{
    unsigned long h = 5381;
    for (size_t i = 0; i < n; i++)
        h = (h * 33) ^ b[i];
    return h;
}

static int run_case(const char *in, const unsigned char *want,
                    size_t want_n, unsigned long want_sum)
{
    unsigned char got[8] = {0};
    size_t got_n = 0;
    if (hex_decode(in, got, &got_n) != 0 || got_n != want_n ||
        memcmp(got, want, want_n) != 0) {
        printf("FAIL decode(\"%s\") = %zu bytes\n", in, got_n);
        return 1;
    }
    unsigned long s = checksum(got, got_n);
    if (s != want_sum) {
        printf("FAIL checksum(\"%s\") = %08lx, want %08lx\n", in, s, want_sum);
        return 1;
    }
    printf("PASS \"%s\" -> %zu bytes, checksum %08lx\n", in, got_n, s);
    return 0;
}

static int run_reject(const char *in)
{
    unsigned char got[8] = {0};
    size_t got_n = 0;
    if (hex_decode(in, got, &got_n) == 0) {
        printf("FAIL \"%s\": invalid input accepted (%zu bytes)\n", in, got_n);
        return 1;
    }
    printf("PASS \"%s\" -> rejected\n", in);
    return 0;
}

int main(void)
{
    static const unsigned char abc1[2] = {0xab, 0xc1};
    static const unsigned char odd[2] = {0x0a, 0xbc};
    static const unsigned char a0b[2] = {0x0a, 0x0b};
    int fails = 0;
    fails += run_case("abc1", abc1, 2, 0x59560fUL);
    fails += run_case("ABC1", abc1, 2, 0x59560fUL);
    fails += run_case("abc", odd, 2, 0x596b33UL);
    fails += run_case(" A0B ", a0b, 2, 0x596b84UL);
    fails += run_reject("ag");
    if (fails) {
        printf("incremental_fix: %d case(s) wrong\n", fails);
        return 1;
    }
    printf("incremental_fix: PASS all cases\n");
    return 0;
}
