/* BAD: blocking APIs called from an ISR.
 * The timer ISR calls xSemaphoreTake() (which may block waiting for the
 * semaphore) and vTaskDelay() inside interrupt context. Blocking in an ISR
 * stalls the whole system: the scheduler cannot run while the ISR waits, and
 * on Cortex-M the ISR masks lower-priority interrupts for the whole wait.
 * The stub marks any blocking API call made while ISR context is active as a
 * rule violation; main() exits nonzero when a violation was recorded. */
#include "../freertos_stubs.h"

static SemaphoreHandle_t g_data_ready;

static void vTimerISR(void) {
    /* ISR context: must never block or wait. */
    xSemaphoreTake(g_data_ready, portMAX_DELAY);   /* BAD: may block in ISR */
    vTaskDelay(10);                                /* BAD: relative delay in ISR */
}

int main(void) {
    g_data_ready = xSemaphoreCreateBinary();
    stub_enter_isr();
    vTimerISR();
    stub_exit_isr();
    return stub_failed() ? 1 : 0;
}
