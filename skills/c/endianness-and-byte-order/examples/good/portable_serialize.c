/*
 * portable_serialize.c — shift-based big-endian (network byte order)
 * serializer/deserializer with a golden-vector test, plus a memcpy-based
 * type-punning demo (the only standard-conforming way to inspect bytes of
 * an integer). Compile: gcc -Wall -Wextra -Werror -O2 portable_serialize.c
 *
 * Expected output:
 *   golden vector u32 0x12345678 -> 12 34 56 78 ... PASS
 *   golden vector u16 0x1234     -> 12 34 ...... PASS
 *   roundtrip 200 values .......... PASS
 *   memcpy punning: bytes of 0x11223344 -> 44 33 22 11 (host = little-endian)
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Serialize a uint32_t to a big-endian byte buffer (portable: no host
 * layout, no alignment, no aliasing involved). */
static void put_u32_be(uint8_t buf[4], uint32_t v) {
    buf[0] = (uint8_t)(v >> 24);
    buf[1] = (uint8_t)(v >> 16);
    buf[2] = (uint8_t)(v >> 8);
    buf[3] = (uint8_t)(v);
}

/* Deserialize a uint32_t from a big-endian byte buffer. Shift-based;
 * the cast to uint32_t on the left operand avoids signed overflow. */
static uint32_t get_u32_be(const uint8_t buf[4]) {
    return ((uint32_t)buf[0] << 24)
         | ((uint32_t)buf[1] << 16)
         | ((uint32_t)buf[2] << 8)
         | ((uint32_t)buf[3]);
}

static uint16_t get_u16_be(const uint8_t buf[2]) {
    return (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
}

static void put_u16_be(uint8_t buf[2], uint16_t v) {
    buf[0] = (uint8_t)(v >> 8);
    buf[1] = (uint8_t)(v);
}

/* Golden vector: 0x12345678 must serialize to 12 34 56 78 on every
 * architecture, little- or big-endian. */
static int test_golden_u32(void) {
    uint8_t buf[4];
    put_u32_be(buf, 0x12345678u);
    printf("  golden vector u32 0x12345678 -> %02x %02x %02x %02x ",
           buf[0], buf[1], buf[2], buf[3]);
    if (buf[0] == 0x12 && buf[1] == 0x34 && buf[2] == 0x56 && buf[3] == 0x78
        && get_u32_be(buf) == 0x12345678u) {
        printf("... PASS\n");
        return 0;
    }
    printf("... FAIL\n");
    return 1;
}

static int test_golden_u16(void) {
    uint8_t buf[2];
    put_u16_be(buf, 0x1234u);
    printf("  golden vector u16 0x1234     -> %02x %02x ", buf[0], buf[1]);
    if (buf[0] == 0x12 && buf[1] == 0x34 && get_u16_be(buf) == 0x1234u) {
        printf("...... PASS\n");
        return 0;
    }
    printf("...... FAIL\n");
    return 1;
}

/* Round-trip all-ones, all-zeroes and boundary values. */
static int test_roundtrip(void) {
    const uint32_t vals[4] = { 0x00000000u, 0xFFFFFFFFu, 0x12345678u,
                               0xDEADBEEFu };
    for (size_t i = 0; i < 4; i++) {
        uint8_t buf[4];
        put_u32_be(buf, vals[i]);
        if (get_u32_be(buf) != vals[i])
            return 1;
    }
    for (uint32_t v = 0; v < 200; v++) {   /* dense low range */
        uint8_t buf[4];
        put_u32_be(buf, v);
        if (get_u32_be(buf) != v)
            return 1;
    }
    printf("  roundtrip 200+ values ........ PASS\n");
    return 0;
}

/* memcpy is the standard-conforming way to type-pun: reading the raw
 * bytes of a uint32_t into an array of unsigned char. Union punning and
 * pointer casts are UB for this. Result is host-dependent on purpose. */
static int test_memcpy_punning(void) {
    uint32_t x = 0x11223344u;
    unsigned char bytes[4];
    memcpy(bytes, &x, sizeof x);
    printf("  memcpy punning: bytes of 0x11223344 -> %02x %02x %02x %02x "
           "(host byte order)\n", bytes[0], bytes[1], bytes[2], bytes[3]);
    return 0;
}

int main(void) {
    int failed = 0;
    printf("portable_serialize: golden vectors + roundtrip\n");
    failed |= test_golden_u32();
    failed |= test_golden_u16();
    failed |= test_roundtrip();
    test_memcpy_punning();
    if (failed) {
        printf("RESULT: FAIL\n");
        return 1;
    }
    printf("RESULT: PASS\n");
    return 0;
}
