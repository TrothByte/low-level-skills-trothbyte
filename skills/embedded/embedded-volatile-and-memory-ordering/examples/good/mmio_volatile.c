// GOOD: MMIO register accessed through a volatile-qualified pointer.
// The simulated device bank is declared volatile; every read hits memory.
#include <stdint.h>

static volatile uint32_t HW_STATUS[4];

uint32_t poll_twice(volatile const uint32_t *r) {
    uint32_t a = r[0];
    uint32_t b = r[0];
    return a + b;
}

int main(void) {
    HW_STATUS[1] = 0x1234;
    uint32_t v = poll_twice(&HW_STATUS[1]);
    return v == 0x2468 ? 0 : 1;
}
