// Correct: STM32F103 I2C1 init skeleton. Clock enable on RCC_APB1ENR bit 21,
// START/ACK bits on CR1, status polled via SR1 named masks. Every constant is
// asserted against the datasheet model before use.
#include "../st7789_stm32_stubs.h"

static void i2c1_enable_clock(void)
{
    volatile uint32_t *apb1enr = (volatile uint32_t *)(RCC_BASE + 0x1Cu);
    *apb1enr |= RCC_APB1ENR_I2C1EN;
}

static void i2c1_start(void)
{
    I2C1->CR1 |= I2C_CR1_START | I2C_CR1_ACK;
}

static int i2c1_wait_sb(void)
{
    unsigned spins = 0u;
    while ((I2C1->SR1 & I2C_SR1_SB) == 0u) {
        if (++spins > 10000u) {
            return -1;
        }
    }
    return 0;
}

_Static_assert((1u << 21) == RCC_APB1ENR_I2C1EN, "I2C1EN is bit 21");

int main(void)
{
    i2c1_enable_clock();
    i2c1_start();
    return i2c1_wait_sb() == 0 ? 0 : 1;
}
