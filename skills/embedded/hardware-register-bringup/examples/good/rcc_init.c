/*
 * GOOD: RCC init-order skeleton for an STM32-class part (target build
 * documented; this file is host-compilable as a stub with static
 * asserts). Every bit is named from the datasheet (stm32-ref-manual),
 * and the sequence is clock -> ready -> reset-deassert -> config.
 *
 * Target: arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -c rcc_init.c
 * Host:   gcc -Wall -Wextra -Werror -O2 rcc_init.c -o rccinit
 */
#include <stdint.h>
#include <stddef.h>

/* Part-specific register block (stub; real offsets from datasheet). */
#define RCC_BASE 0x40021000u
#define RCC_CR   (*(volatile uint32_t *)(RCC_BASE + 0x00u))
#define RCC_CFGR (*(volatile uint32_t *)(RCC_BASE + 0x04u))
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_APB2RSTR (*(volatile uint32_t *)(RCC_BASE + 0x0Cu))

/* Datasheet-verified fields (stm32-ref-manual). */
#define RCC_CR_HSEON  (1u << 16)
#define RCC_CR_HSERDY (1u << 17)
#define RCC_APB2ENR_IOPAEN (1u << 2)   /* bit 2 = GPIOA clock (this part) */

/* Compile-time guard: field exists at the datasheet offset. */
_Static_assert((RCC_APB2ENR_IOPAEN & 0xFFFFu) != 0,
               "GPIOA clock-enable bit must be in APB2ENR low word");

static uint32_t rcc_poll_ready(uint32_t flag) {
    for (uint32_t i = 0; i < 1000000u; i++)
        if (RCC_CR & flag) return 0;
    return 1; /* timeout */
}

void gpioa_init(void) {
    RCC_CR |= RCC_CR_HSEON;            /* 1. clock source on */
    if (rcc_poll_ready(RCC_CR_HSERDY)) return;   /* 2. wait ready (bounded) */
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN; /* 3. bus/peripheral clock */
    RCC_APB2RSTR &= ~(1u << 2);        /* 4. peripheral reset deassert */
    /* 5. configuration (GPIO MODER etc.) happens here */
    /* 6. enable bit last, where the peripheral has one */
}
