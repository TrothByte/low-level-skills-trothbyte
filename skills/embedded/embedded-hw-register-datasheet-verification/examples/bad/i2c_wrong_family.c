// intentionally incorrect: SR1 offset taken from an STM32F0 tutorial. F0 has
// no SR1; its ISR lives at 0x18. On the F1 model the register at 0x18 is SR2,
// so polling "SR1 @ 0x18" reads the status register of the wrong meaning.
#include "../st7789_stm32_stubs.h"

#define I2C_SR1_OFFSET_FROM_F0_TUTORIAL 0x18u

_Static_assert(offsetof(I2C_TypeDef, SR1) == I2C_SR1_OFFSET_FROM_F0_TUTORIAL,
               "SR1 is at 0x14 per RM0008; 0x18 is SR2");

int main(void)
{
    return 0;
}
