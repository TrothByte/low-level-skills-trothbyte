// GOOD: correct seL4 driver skeleton. The component holds capabilities for
// its MMIO region and its IRQ; it allocates a notification, binds the IRQ to
// it, checks every error, and only then waits. MMIO is reached through the
// mapping granted to the component, never through a guessed global address.
// Compile: gcc -Wall -Wextra -Werror -O2 -c seL4_driver_stub.c
#include "../seL4_stubs.h"
#include <assert.h>

#define IRQ_TIMER 27u
#define NTFN_SLOT 5u
#define IRQ_SLOT 6u
#define EP_SLOT 7u   /* IPC endpoint capability */

/* component-local capability slots (indexes into the component CNode) */
static seL4_CapData_t cnode[16];

static seL4_Error bind_timer_irq(void)
{
    seL4_Error err;
    /* 1. allocate notification object into slot NTFN_SLOT */
    /* 2. bind the IRQ handler capability to that notification */
    err = seL4_IRQHandler_SetNotification(cnode[IRQ_SLOT], cnode[NTFN_SLOT]);
    if (err != seL4_NoError) {
        return err; /* missing IRQ capability must surface, not be swallowed */
    }
    return seL4_NoError;
}

int driver_loop(void)
{
    seL4_Reply_t reply;
    seL4_Error err = bind_timer_irq();
    if (err != seL4_NoError) {
        return -1; /* config error: do not run with a dead IRQ path */
    }
    for (;;) {
        seL4_Word_t badge = seL4_Wait(cnode[NTFN_SLOT], &reply);
        /* IRQ arrived: read the device through the granted mapping */
        uint32_t status = g_device_reg;
        if (status & 1u) {
            (void)badge;
            seL4_Call(cnode[EP_SLOT], (seL4_MessageInfo_t){0}, &reply);
        }
    }
}
