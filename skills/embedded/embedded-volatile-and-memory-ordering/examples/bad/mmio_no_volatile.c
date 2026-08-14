// BAD: MMIO register read through a NON-volatile pointer.
// The simulated device bank is volatile hardware, but this code drops the
// qualifier with a cast. At -O2 the compiler folds the two reads into one
// load (register caching); on real hardware the second read would return a
// stale value instead of re-reading the device.
#include <stdint.h>

static volatile uint32_t HW_STATUS[4];

uint32_t poll_twice(const uint32_t *r) {
    uint32_t a = r[0];
    uint32_t b = r[0];
    return a + b;
}

int main(void) {
    HW_STATUS[1] = 0x1234;
    uint32_t v = poll_twice((const uint32_t *)&HW_STATUS[1]);
    return v == 0x2468 ? 0 : 1;
}
