// intentionally incorrect: I2C1 clock enable claimed on RCC_APB1ENR bit 1
// (that bit gates TIM2). I2C1EN is bit 21. "Clock enabled" by this code
// actually starts timer 2 and leaves I2C1 clocked off.
#include "../st7789_stm32_stubs.h"

#define I2C1EN_CLAIMED (1u << 1)

_Static_assert(I2C1EN_CLAIMED == RCC_APB1ENR_I2C1EN,
               "I2C1EN is bit 21 per RM0008 RCC_APB1ENR");

int main(void)
{
    return 0;
}
