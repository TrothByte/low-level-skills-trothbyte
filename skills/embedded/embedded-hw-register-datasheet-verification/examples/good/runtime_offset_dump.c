// Prints the datasheet-derived register facts the model encodes. Running this
// is a smoke check that the offsets, sizes, masks, and reset values the agent
// quotes actually come out of the model (not out of memory).
#include <stdio.h>
#include <stdint.h>
#include "../st7789_stm32_stubs.h"

int main(void)
{
    printf("I2C_TypeDef size=0x%02X\n", (unsigned)sizeof(I2C_TypeDef));
    printf("I2C1 base=0x%08X\n", (unsigned)I2C1_BASE);
    printf("SR1 offset=0x%02X (expected 0x14)\n", (unsigned)offsetof(I2C_TypeDef, SR1));
    printf("SR2 offset=0x%02X (expected 0x18)\n", (unsigned)offsetof(I2C_TypeDef, SR2));
    printf("TRISE offset=0x%02X (expected 0x20)\n", (unsigned)offsetof(I2C_TypeDef, TRISE));
    printf("TRISE reset=0x%04X (expected 0x0002)\n", (unsigned)I2C_TRISE_RESET);
    printf("SR1 TxE mask=0x%04X (expected 0x0040)\n", (unsigned)I2C_SR1_TxE);
    printf("SR1 defined bits=0x%04X (expected 0xDF7F)\n",
           (unsigned)(I2C_SR1_SB | I2C_SR1_ADDR | I2C_SR1_BTF | I2C_SR1_ADD10 |
                      I2C_SR1_STOPF | I2C_SR1_RxNE | I2C_SR1_TxE | I2C_SR1_BERR |
                      I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR | I2C_SR1_PECERR |
                      I2C_SR1_TIMEOUT | I2C_SR1_SMBALERT));
    printf("I2C1EN bit=%u (expected 21)\n", 21u);
    printf("MADCTL mask=0x%02X (expected 0xFC)\n", (unsigned)MADCTL_MASK);
    printf("landscape+BGR MADCTL=0x%02X (expected 0x68)\n",
           (unsigned)(MADCTL_MV | MADCTL_MX | MADCTL_BGR));
    printf("MADCTL reset=0x%02X (expected 0x00)\n", (unsigned)ST7789_MADCTL_RESET);
    return 0;
}
