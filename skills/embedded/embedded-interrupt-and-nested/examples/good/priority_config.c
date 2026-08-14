// GOOD: NVIC priority configured inside the valid 4-bit range (0..15) and at
// or below the reserved OS floor (device IRQs at priority >= OS_PRIO_FLOOR),
// then the IRQs are enabled. Lower number means higher urgency, so 4 is more
// urgent than 8. The configured values are read back and must round-trip.
#include "../cortex_m_stubs.h"

int main(void) {
    nvic_set_priority(IRQ_UART, 4u);
    nvic_set_priority(IRQ_TIMER, 8u);
    nvic_set_priority(IRQ_SPI, 8u);
    nvic_enable_irq(IRQ_UART);
    nvic_enable_irq(IRQ_TIMER);
    nvic_enable_irq(IRQ_SPI);
    int ok = (nvic_get_priority(IRQ_UART) == 4u) &&
             (nvic_get_priority(IRQ_TIMER) == 8u) &&
             (nvic_get_priority(IRQ_SPI) == 8u);
    return (ok && !stub_has_failed()) ? 0 : 1;
}
