// BAD: device IRQs enabled at priorities that violate the NVIC rules.
// (1) The NVIC priority field on this part has __NVIC_PRIO_BITS = 4 bits, so
// the valid range is 0..15; 16 does not fit and the write is ignored.
// (2) Priority 0 is the highest configurable urgency and is reserved for the
// OS (tick/PendSV); assigning it to a device IRQ lets the device preempt the
// OS tick and bypass the kernel critical sections. The stub rejects both.
#include "../cortex_m_stubs.h"

int main(void) {
    nvic_set_priority(IRQ_UART, 16u); /* out of the 4-bit field */
    nvic_set_priority(IRQ_TIMER, 0u); /* reserved: more urgent than the OS */
    nvic_set_priority(IRQ_SPI, 8u);
    nvic_enable_irq(IRQ_UART);
    nvic_enable_irq(IRQ_TIMER);
    nvic_enable_irq(IRQ_SPI);
    return stub_has_failed() ? 1 : 0;
}
