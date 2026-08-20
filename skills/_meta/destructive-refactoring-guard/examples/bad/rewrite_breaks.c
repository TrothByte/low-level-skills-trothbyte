/* rewrite_breaks.c — hex decoder + djb2 checksum utility, v4.0 "rewrite".
 *
 * This is the anti-pattern this skill exists to stop: instead of fixing
 * the odd-length bug, the whole parser was rewritten "cleanly" on top of
 * sscanf(). The rewrite silently drops three behaviors that the previous
 * code had:
 *   1. odd-length inputs are truncated ("abc" -> AB, losing C);
 *   2. whitespace padding around the value is not normalized;
 *   3. invalid trailing characters are accepted, not rejected.
 * The self-test below feeds the same cases the old code handled and shows
 * the rewrite failing all three — while still compiling clean.
 */
#include <stdio.h>
#include <string.h>

static int hex_decode(const char *s, unsigned char *out, size_t *out_len)
{
    size_t n = strlen(s) / 2;   /* odd-length inputs are silently truncated */
    for (size_t i = 0; i < n; i++) {
        unsigned int v;
        if (sscanf(s + 2 * i, "%2x", &v) != 1)
            return -1;
        out[i] = (unsigned char)v;
    }
    *out_len = n;
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
        printf("rewrite_breaks: %d case(s) wrong\n", fails);
        return 1;
    }
    printf("rewrite_breaks: PASS all cases\n");
    return 0;
}
