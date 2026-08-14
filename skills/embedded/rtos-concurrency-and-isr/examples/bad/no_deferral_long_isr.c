/* BAD: time-consuming work performed inside the ISR instead of deferring it.
 * The ISR computes a checksum over the whole payload while in interrupt
 * context. During that time no task runs and lower-priority interrupts stay
 * masked, so real-time deadlines are missed even though the code is otherwise
 * correct. The stub cannot preempt, so the work units accumulated in ISR
 * context are compared against a sane ISR budget: a well-formed ISR would
 * hand the loop to a task and only signal it. main() exits nonzero when the
 * ISR exceeded the budget. */
#include "../freertos_stubs.h"

static unsigned g_isr_work_units;

static unsigned vUARTISR_Checksum(const unsigned char *buf, unsigned len) {
    unsigned c = 0u;
    for (unsigned i = 0u; i < len; i++) {
        c = (c + buf[i]) * 31u;
        g_isr_work_units++;   /* each iteration is work done in ISR context */
    }
    return c;
}

int main(void) {
    unsigned char payload[64] = {0};
    unsigned checksum;
    payload[0] = 7;
    payload[63] = 9;
    stub_enter_isr();
    checksum = vUARTISR_Checksum(payload, sizeof(payload));   /* long ISR */
    stub_exit_isr();
    if (checksum == 0u) {
        return 1;   /* keep the result used */
    }
    return g_isr_work_units <= 8u ? 0 : 1;   /* far above a sane ISR budget */
}
