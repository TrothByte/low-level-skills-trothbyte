#include <stdint.h>
#include <stdio.h>

static uint32_t fnv1a(const uint8_t *buf, size_t len)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= buf[i];
        h *= 16777619u;
    }
    return h;
}

static uint32_t crc32(const uint8_t *buf, size_t len)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        c ^= buf[i];
        for (int bit = 0; bit < 8; bit++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

int main(void)
{
    const uint8_t data[] = {0x01, 0x80, 0x03, 0xFF, 0x05};
    printf("fnv1a: %08x\n", fnv1a(data, sizeof data));
    printf("crc32: %08x\n", crc32(data, sizeof data));
    return 0;
}
