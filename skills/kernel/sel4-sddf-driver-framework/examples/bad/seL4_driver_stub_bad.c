// BAD: driver skeleton with the classic seL4 porting mistakes.
// (1) waits on a notification without ever binding the IRQ to it -> the
//     driver never wakes; (2) touches "device memory" through a global
//     pointer instead of a granted mapping; (3) swallows every seL4 error.
// This compiles on the host but is broken on seL4.
// Compile: gcc -Wall -Wextra -Werror -O2 -c seL4_driver_stub_bad.c
// Marker: intentionally incorrect
#include "../seL4_stubs.h"

#define NTFN_SLOT 5u
#define EP_SLOT 7u

static seL4_CapData_t cnode[16];

/* intentionally incorrect: global "device" address with no mapping cap */
static volatile uint32_t *const g_fake_mmio =
    (volatile uint32_t *const)(uintptr_t)0x3F000000u;

int bad_driver_loop(void)
{
    seL4_Reply_t reply;
    /* intentionally incorrect: seL4_Wait without IRQ binding — the IRQ
       capability was never bound to NTFN_SLOT, so the driver blocks forever
       and no error is ever reported. */
    for (;;) {
        (void)seL4_Wait(cnode[NTFN_SLOT], &reply);

        /* intentionally incorrect: dereferences an un-granted address */
        *g_fake_mmio = 0x2u; /* device write through an un-granted mapping */

        /* intentionally incorrect: the send result is discarded, so a
           missing IPC endpoint capability passes silently */
        seL4_Call(cnode[EP_SLOT], (seL4_MessageInfo_t){0}, &reply);
    }
}
