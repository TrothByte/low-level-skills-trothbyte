// GOOD: the ISR does the minimum (record the event, charge a small amount of
// work, request deferred processing with a pending bit) and returns. The heavy
// processing runs in main context after the ISRs are done, so the ISR stays
// within its budget. NVIC pending is used here as the software-interrupt
// mechanism to ask for deferred work.
#include "../cortex_m_stubs.h"

static unsigned events_seen;

static void uart_isr(void) {
    stub_enter_isr();
    events_seen++;               /* only shared state the ISR touches */
    stub_isr_work(1u);
    nvic_set_pending(IRQ_TIMER); /* request deferred processing */
    stub_exit_isr();
}

int main(void) {
    for (unsigned i = 0; i < 8u; i++) {
        uart_isr();
    }
    unsigned processed = 0u;
    while (nvic_get_pending(IRQ_TIMER) != 0u) {
        nvic_clear_pending(IRQ_TIMER);
        processed += events_seen; /* deferred work runs here, not in the ISR */
        events_seen = 0u;
    }
    int ok = (processed >= 8u) && !stub_has_failed();
    return ok ? 0 : 1;
}
