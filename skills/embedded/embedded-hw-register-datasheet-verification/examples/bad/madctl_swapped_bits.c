// intentionally incorrect: MX and MV are swapped (memory: "MV is 0x40") and a
// reserved bit D0 is forced on. The claimed 0x69 value contradicts the
// datasheet bit table (0x68) and trips the assert.
#include "../st7789_stm32_stubs.h"

#define MADCTL_WRONG_MX 0x20u
#define MADCTL_WRONG_MV 0x40u
#define MADCTL_WRONG_RSV 0x01u

static uint8_t madctl_landscape_bgr(void)
{
    return (uint8_t)(MADCTL_WRONG_MV | MADCTL_WRONG_MX | MADCTL_BGR | MADCTL_WRONG_RSV);
}

_Static_assert((MADCTL_WRONG_MV | MADCTL_WRONG_MX | MADCTL_BGR | MADCTL_WRONG_RSV) == 0x68u,
               "claimed MADCTL value does not match the datasheet bit table");

int main(void)
{
    return 0;
}
