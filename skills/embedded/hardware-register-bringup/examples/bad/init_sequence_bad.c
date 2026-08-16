/*
 * BAD: // intentionally incorrect — configures the peripheral register
 * BEFORE enabling its clock, and proceeds without polling the PLL ready
 * flag. Two init-order violations: (1) writes to a clock-gated peripheral
 * silently do nothing; (2) config on an unstable clock source.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 init_sequence_bad.c -o seqbad
 */
#include <stdint.h>
#include <stdio.h>

static uint8_t sim_clock_enabled;
static uint8_t sim_ready;   /* PLL ready flag — never polled here */

/* // intentionally incorrect: no ready-poll, no ordering */
static void write_config_early(uint8_t v) {
    /* // intentionally incorrect: config before clock enable */
    if (!sim_clock_enabled) {
        printf("WARN: writing config with clock disabled (ignored on silicon)\n");
    }
    (void)v;
}

int main(void) {
    /* // intentionally incorrect: config first, clock never enabled, PLL never waited */
    write_config_early(0xAB);
    sim_clock_enabled = 1;           /* clock "enabled" only after config */
    if (!sim_ready) {
        printf("BUG: proceeding without polling PLL ready flag\n");
    }
    printf("init done\n");
    return 0;
}
