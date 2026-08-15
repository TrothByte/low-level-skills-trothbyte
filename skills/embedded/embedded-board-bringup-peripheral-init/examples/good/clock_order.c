/*
 * GOOD: clock-tree-first init ordering (target pattern).
 * Order: clock source -> bus clock enable -> peripheral config. The bus clock
 * gate must be on before the peripheral registers are written. Target code;
 * the ordering rule is demonstrated on host by examples/good/init_order_check.c.
 */
#include <stdint.h>

#define RCC_CR       (*(volatile uint32_t *)0x40023800UL)
#define RCC_CFGR     (*(volatile uint32_t *)0x40023808UL)
#define RCC_APB1ENR  (*(volatile uint32_t *)0x40023840UL)
#define TIM2_CR1     (*(volatile uint32_t *)0x40000000UL)
#define TIM2_PSC     (*(volatile uint32_t *)0x40000028UL)
#define TIM2_ARR     (*(volatile uint32_t *)0x4000002CUL)

static void init_timer2_correct(void) {
    RCC_CR |= (1u << 0);               /* 1. HSI on */
    while (!(RCC_CR & (1u << 1))) { }  /* wait HSERDY */
    RCC_CFGR = 0;                      /* 2. keep HSI as system clock */
    RCC_APB1ENR |= (1u << 0);          /* 3. enable TIM2 on APB1 */
    TIM2_PSC = 71;                     /* 4. now configure the peripheral */
    TIM2_ARR = 999;
    TIM2_CR1 |= 1;
}
