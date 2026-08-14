// BAD: the ISR busy-waits for a device status bit that can only be set by a
// lower-priority context (a peripheral handled by another ISR, or main).
// While this ISR spins, nothing at equal or lower priority runs, so on a
// single core the condition can never become true. The stub budget check
// trips once the ISR has charged more than STUB_ISR_BUDGET work units.
#include "../cortex_m_stubs.h"

static volatile unsigned device_status;

static void uart_isr(void) {
    stub_enter_isr();
    while ((device_status & 1u) == 0u) {
        stub_isr_work(1u);
        if (stub_has_failed()) {
            break;
        }
    }
    stub_exit_isr();
}

int main(void) {
    /* Hardware never sets the bit because the ISR holds the core. */
    uart_isr();
    return stub_has_failed() ? 1 : 0;
}
