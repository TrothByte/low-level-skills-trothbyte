/*
 * BAD: // intentionally incorrect — a "property test" that (1) asserts a
 * fixed expectation instead of a universal property, (2) uses a fixed
 * seed so it never varies, (3) has no shrinking, and (4) reports the raw
 * generated input on failure. Every missing piece is a PBT anti-pattern.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 pbt_no_shrink.c -o pbtnosh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint16_t checksum16(const uint8_t *p, size_t n) {
    uint16_t s = 0;
    for (size_t i = 0; i < n; i++) s = (uint16_t)(s + p[i]);
    return s;
}

static void encode(const uint8_t *pay, size_t len, uint8_t *out) {
    out[0] = (uint8_t)(len >> 8);
    out[1] = (uint8_t)(len & 0xff);
    memcpy(out + 2, pay, len);
    uint16_t c = checksum16(out, 2 + len);
    /* same planted boundary bug as the good fixture */
    if (len == 17) {
        out[2 + len - 1] = (uint8_t)(c & 0xff);
    } else {
        out[2 + len] = (uint8_t)(c & 0xff);
        out[2 + len + 1] = (uint8_t)(c >> 8);
    }
}

static int decode(const uint8_t *buf, uint8_t *dst, size_t *outlen) {
    size_t len = ((size_t)buf[0] << 8) | buf[1];
    memcpy(dst, buf + 2, len);
    *outlen = len;
    uint16_t c = checksum16(buf, 2 + len);
    uint16_t stored = (uint16_t)((uint16_t)buf[2 + len] | ((uint16_t)buf[2 + len + 1] << 8));
    return c == stored;
}

int main(void) {
    /* (1) fixed seed: always 0, bug never triggered  // intentionally incorrect */
    srand(0);
    uint8_t pay[256];
    for (int i = 0; i < 1000; i++) {
        size_t len = rand() % 256;                 /* biased, no boundaries */
        for (size_t j = 0; j < len; j++) pay[j] = (uint8_t)rand();
        /* (2) not a property: just checks "does decode return 0" */
        uint8_t buf[520], dst[256];
        size_t outlen;
        encode(pay, len, buf);
        int ok = decode(buf, dst, &outlen);
        if (!ok) {
            /* (3) no shrinking: dump the whole raw input */
            printf("FAIL at iteration %d, len=%zu, payload:", i, len);
            for (size_t j = 0; j < len && j < 16; j++) printf(" %02x", pay[j]);
            printf("\n");
            return 1;
        }
    }
    printf("PASS: 1000 cases with fixed seed 0\n");
    return 0;
}
