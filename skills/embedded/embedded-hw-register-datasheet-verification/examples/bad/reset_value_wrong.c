// intentionally incorrect: TRISE reset value claimed as 0x0000. The datasheet
// reset table lists 0x0002 (rise time, in I2C clock cycles). Code that checks
// "TRISE still 0 after reset" is comparing against a fabricated value.
#include "../st7789_stm32_stubs.h"

#define I2C_TRISE_RESET_CLAIMED 0x0000u

_Static_assert(I2C_TRISE_RESET_CLAIMED == I2C_TRISE_RESET,
               "TRISE reset is 0x0002 per the RM0008 reset table");

int main(void)
{
    return 0;
}
