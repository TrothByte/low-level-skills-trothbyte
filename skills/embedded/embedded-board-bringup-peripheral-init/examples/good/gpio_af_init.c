/*
 * GOOD: GPIO alternate-function setup (target pattern).
 * For USART1 TX on PA9 (STM32F4): MODER = AF mode (2), AFRL/AFRH selects AF7.
 * The AF number comes from the datasheet's AF table, not from memory.
 */
#include <stdint.h>

#define GPIOA_MODER (*(volatile uint32_t *)0x40020000UL)
#define GPIOA_AFRH  (*(volatile uint32_t *)0x40020024UL)

static void usart1_tx_pin_init(void) {
    /* PA9: MODER[19:18] = 2 (AF mode) */
    GPIOA_MODER = (GPIOA_MODER & ~(3u << 18)) | (2u << 18);
    /* PA9 is in the high nibble: AFRL covers PA0-7, AFRH covers PA8-15 */
    GPIOA_AFRH = (GPIOA_AFRH & ~(0xFu << 4)) | (7u << 4); /* AF7 */
}
