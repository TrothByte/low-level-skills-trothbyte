// intentionally incorrect: TxE claimed at bit 7 (0x80). Per RM0008 bit 7 of
// SR1 is BERR; TxE is bit 6 (0x40). A TxE poll with 0x80 reads BERR instead,
// so the loop "waits" on an error flag.
#include "../st7789_stm32_stubs.h"

#define I2C_SR1_TXE_CLAIMED 0x80u

_Static_assert(I2C_SR1_TXE_CLAIMED == I2C_SR1_TxE,
               "TxE is bit 6 (0x40); 0x80 is BERR");

int main(void)
{
    return 0;
}
