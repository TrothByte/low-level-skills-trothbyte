/*
 * BAD: // intentionally incorrect — a misaligned/aliased read of a byte
 * buffer as uint32_t. This violates C11 6.5p7 strict aliasing and may be
 * misaligned; UBSan flags it. Kernel-correct code uses memcpy /
 * get_unaligned. The "works in a quick test" illusion is exactly the
 * trap.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 -fsanitize=undefined kernel_ub_bad.c -o ubbad
 */
#include <stdint.h>
#include <stdio.h>

/* // intentionally incorrect: aliased + potentially misaligned read */
static uint32_t read_u32_bad(const uint8_t *p) {
    return *(const uint32_t *)(const void *)p;
}

int main(void) {
    uint8_t buf[8] = {0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0};
    uint32_t v = read_u32_bad(buf);   /* aliases a uint8_t object */
    printf("value=%#x\n", v);
    if (v == 0x12345678) {
        printf("BUG hidden: -O0 looks right; UB flagged by UBSan\n");
    }
    return 0;
}
