/*
 * GOOD: property-based test harness for a kernel-adjacent packet
 * encoder/decoder (host-runnable). Demonstrates the full PBT loop:
 * universal round-trip property, boundary-aware generator, deterministic
 * seed from argv, and shrinking to a minimal counterexample.
 *
 * A planted boundary bug (len == 17 corrupts the checksum) exists so the
 * harness can demonstrate that it finds AND shrinks the failure. In a
 * real codebase, finding this is the entire point of PBT.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 pbt_checksum.c -o pbt
 * Run:   pbt <seed>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- code under test: checksummed packet format ---- */
static uint16_t checksum16(const uint8_t *p, size_t n) {
    uint16_t s = 0;
    for (size_t i = 0; i < n; i++) s = (uint16_t)(s + p[i]);
    return s;
}

/* encode(payload, len) -> buf[2+len+2]: BE len, payload, checksum */
static void encode(const uint8_t *pay, size_t len, uint8_t *out) {
    out[0] = (uint8_t)(len >> 8);
    out[1] = (uint8_t)(len & 0xff);
    memcpy(out + 2, pay, len);
    uint16_t c = checksum16(out, 2 + len);
    /* PLANTED BUG: when len == 17, checksum written one byte early. */
    if (len == 17) {
        out[2 + len - 1] = (uint8_t)(c & 0xff); /* overwrites last payload byte */
    } else {
        out[2 + len] = (uint8_t)(c & 0xff);
        out[2 + len + 1] = (uint8_t)(c >> 8);
    }
}

/* decode: returns 0 on checksum failure, 1 on success, payload into dst */
static int decode(const uint8_t *buf, uint8_t *dst, size_t *outlen) {
    size_t len = ((size_t)buf[0] << 8) | buf[1];
    if (len > 65535) return 0;
    memcpy(dst, buf + 2, len);
    *outlen = len;
    uint16_t c = checksum16(buf, 2 + len);
    uint16_t stored = (uint16_t)((uint16_t)buf[2 + len] | ((uint16_t)buf[2 + len + 1] << 8));
    return c == stored;
}

/* ---- generator: deterministic PRNG (xorshift64) ---- */
static uint64_t rng_state;
static uint64_t xrand(void) {
    uint64_t x = rng_state;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    return rng_state = x;
}

#define MAXLEN 256
/* property: decode(encode(x)) == x for every payload */
static int prop_roundtrip(const uint8_t *pay, size_t len) {
    uint8_t buf[2 + MAXLEN + 2];
    uint8_t dst[MAXLEN];
    size_t outlen = 0;
    encode(pay, len, buf);
    if (!decode(buf, dst, &outlen)) return 0;
    return outlen == len && memcmp(dst, pay, len) == 0;
}

/* ---- shrinking: reduce len until failure goes away ---- */
static int fails_at_len(size_t len) {
    uint8_t pay[MAXLEN];
    memset(pay, 0xAB, sizeof pay);
    return !prop_roundtrip(pay, len);
}

int main(int argc, char **argv) {
    uint64_t seed = argc > 1 ? strtoull(argv[1], NULL, 10) : 12345;
    rng_state = seed;
    printf("seed=%llu\n", (unsigned long long)seed);

    size_t failing_len = (size_t)-1;
    uint64_t iter;
    for (iter = 0; iter < 5000; iter++) {
        /* boundary-aware generator: mix random + edge lengths */
        size_t len;
        switch (xrand() % 8) {
            case 0: len = 0; break;
            case 1: len = 1; break;
            case 2: len = 2; break;
            case 3: len = 16; break;
            case 4: len = 17; break;   /* the buggy boundary */
            case 5: len = MAXLEN; break;
            default: len = xrand() % (MAXLEN + 1); break;
        }
        uint8_t pay[MAXLEN];
        for (size_t i = 0; i < len; i++) pay[i] = (uint8_t)(xrand() & 0xff);
        if (!prop_roundtrip(pay, len)) { failing_len = len; break; }
    }

    if (failing_len == (size_t)-1) {
        printf("PASS: no counterexample in %llu cases\n", (unsigned long long)iter);
        return 0;
    }

    /* shrink: find minimal failing length */
    size_t lo = 0, hi = failing_len;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (fails_at_len(mid)) hi = mid; else lo = mid + 1;
    }
    printf("COUNTEREXAMPLE len=%zu (raw=%zu), shrunk to minimal: %s\n",
           failing_len, failing_len, fails_at_len(lo) && lo == 17 ? "len=17 confirmed" : "unexpected");
    printf("PLANTED BUG FOUND by property test and shrunk\n");
    return 1; /* the property test correctly fails — that is its job */
}
