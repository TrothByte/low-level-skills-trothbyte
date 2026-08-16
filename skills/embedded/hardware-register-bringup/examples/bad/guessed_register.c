/*
 * BAD: // intentionally incorrect — a guessed register bit with no
 * datasheet check: "RCC->APB2ENR |= (1 << 4)" assumed to enable USART1.
 * On many parts bit 4 gates a different (or nonexistent) peripheral —
 * the code compiles via CMSIS headers and silently enables the wrong
 * clock, and the peripheral "does nothing". Register fields come from
 * the reference manual for the EXACT part number.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 guessed_register.c -o guessbad
 */
#include <stdint.h>
#include <stdio.h>

#define RCC_APB2ENR (*(volatile uint32_t *)0x40021018u)

void usart1_init(void) {
    /* // intentionally incorrect: bit guessed from memory, not datasheet */
    RCC_APB2ENR |= (1u << 4);   /* "USART1 clock" — unverified on this part */
    /* // intentionally incorrect: config proceeds without ready poll */
    (void)RCC_APB2ENR;
}

int main(void) {
    usart1_init();
    printf("BUG: guessed clock-enable bit (no datasheet check)\n");
    return 0;
}
