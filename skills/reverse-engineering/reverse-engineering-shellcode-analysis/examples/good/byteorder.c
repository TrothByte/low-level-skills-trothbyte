#include <stdint.h>
#include <stdio.h>

int main(void)
{
    uint32_t value = 0x7f000001u;
    unsigned char *p = (unsigned char *)&value;
    printf("0x7f000001 stored little-endian : %02x %02x %02x %02x\n",
           p[0], p[1], p[2], p[3]);

    uint32_t pushed = 0x0100007fu;
    unsigned char *q = (unsigned char *)&pushed;
    printf("0x0100007f stored little-endian : %02x %02x %02x %02x  (s_addr 127.0.0.1)\n",
           q[0], q[1], q[2], q[3]);

    uint32_t sock = 0x5c110002u;
    unsigned char *r = (unsigned char *)&sock;
    printf("0x5c110002 stored little-endian : %02x %02x %02x %02x\n",
           r[0], r[1], r[2], r[3]);
    printf("  -> sin_family(u16 LE) = 0x%04x = AF_INET, sin_port bytes = %02x %02x = %u\n",
           (unsigned)(r[0] | (r[1] << 8)), r[2], r[3],
           (unsigned)((r[2] << 8) | r[3]));
    return 0;
}
