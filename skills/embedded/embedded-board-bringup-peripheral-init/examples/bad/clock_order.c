/*
 * BAD: // intentionally incorrect — peripheral configured before its clock is
 * enabled. On STM32-class parts writes to a clock-gated peripheral are
 * ignored (or hang the bus). Compiles clean; does nothing on silicon.
 *
 * Target code (needs STM32 headers); host compile below is a stub demo of the
 * ordering rule.
 */
#include <stdint.h>

#define RCC_APB1ENR  (*(volatile uint32_t *)0x40023840UL)
#define TIM2_CR1     (*(volatile uint32_t *)0x40000000UL)
#define TIM2_PSC     (*(volatile uint32_t *)0x40000028UL)

static void init_timer_wrong(void) {
    TIM2_PSC = 71;                 /* config BEFORE clock enable */
    TIM2_CR1 = 1;
    RCC_APB1ENR |= (1u << 0);      /* clock enable comes too late */
}
