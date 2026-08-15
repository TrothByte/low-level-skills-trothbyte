// Correct: MADCTL composed from the datasheet's named bit masks. Landscape +
// BGR on this panel = MV|MX|BGR. Reserved D1:D0 stay 0.
#include "../st7789_stm32_stubs.h"

static uint8_t madctl_landscape_bgr(void)
{
    return (uint8_t)(MADCTL_MV | MADCTL_MX | MADCTL_BGR);
}

_Static_assert((MADCTL_MV | MADCTL_MX | MADCTL_BGR) == 0x68u,
               "landscape+BGR = 0x68 from the datasheet bit table");
_Static_assert(((MADCTL_MV | MADCTL_MX | MADCTL_BGR) & ~MADCTL_MASK) == 0u,
               "no reserved bit is set");

int main(void)
{
    return madctl_landscape_bgr() == 0x68u ? 0 : 1;
}
