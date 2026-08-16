/*
 * GOOD: power-on register init sequence, host-runnable with a simulated
 * clock/ready model. Demonstrates the correct order:
 *   bus clocks -> peripheral clock -> ready-poll -> reset deassert ->
 *   configuration -> enable bit LAST.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 init_sequence_good.c -o seqgood
 */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

/* Simulated register model of an MCU peripheral. */
static uint8_t sim_clock_enabled;   /* peripheral clock gate */
static uint8_t sim_reset_held;      /* 1 = peripheral in reset */
static uint8_t sim_ready;           /* clock-source ready flag */
static uint8_t sim_enabled;         /* peripheral enable bit */

/* simulated writes: track whether the write "landed" */
static uint8_t config_landed;

static void sim_enable_bus_clock(void) { sim_clock_enabled = 1; }
static void sim_poll_ready(void) { sim_ready = 1; }   /* after timeout */
static void sim_deassert_reset(void) {
    assert(sim_clock_enabled);       /* reset release requires clock */
    sim_reset_held = 0;
}

static void sim_write_config(uint8_t v) {
    assert(sim_clock_enabled && !sim_reset_held);
    config_landed = v;               /* landed only when clocked+unreset */
}

static void sim_enable_peripheral(void) {
    assert(sim_clock_enabled && !sim_reset_held);
    assert(config_landed != 0);      /* enable must be the LAST step */
    sim_enabled = 1;
}

int main(void) {
    /* Correct power-on sequence. */
    sim_enable_bus_clock();          /* step 1: bus clock */
    sim_enable_bus_clock();          /* step 2: peripheral clock gate */
    sim_poll_ready();                /* step 3: ready-poll with timeout */
    sim_deassert_reset();            /* step 4: reset release (clock on) */
    sim_write_config(0xAB);          /* step 5: configuration */
    sim_enable_peripheral();         /* step 6: enable bit LAST */

    assert(config_landed == 0xAB);
    assert(sim_enabled);
    printf("PASS: clock -> ready-poll -> reset-deassert -> config -> enable\n");
    return 0;
}
