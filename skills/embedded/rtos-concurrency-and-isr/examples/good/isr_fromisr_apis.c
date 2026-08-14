/* GOOD: the ISR-calling-non-FromISR-API counterexample, fixed.
 * In interrupt context only the ISR-safe variants are allowed: xQueueSendFromISR()
 * and xSemaphoreGiveFromISR(). They never block; the wake decision is reported
 * through pxHigherPriorityTaskWoken and acted on with portYIELD_FROM_ISR()
 * before the ISR returns. The stub asserts that no non-FromISR API was called
 * in ISR context and that a yield was requested. */
#include "../freertos_stubs.h"

static QueueHandle_t g_events;
static SemaphoreHandle_t g_done;

static void vTimerISR(void) {
    int event = 1;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(g_events, &event, &xHigherPriorityTaskWoken);
    xSemaphoreGiveFromISR(g_done, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int main(void) {
    int event = 0;
    g_events = xQueueCreate(4, sizeof(int));
    g_done = xSemaphoreCreateBinary();

    stub_enter_isr();
    vTimerISR();
    stub_exit_isr();

    if (stub_failed()) {
        return 1;
    }
    if (stub_yields_requested() == 0) {
        return 1;   /* a higher-priority consumer was woken */
    }
    if (xQueueReceive(g_events, &event, 0) != pdTRUE || event != 1) {
        return 1;
    }
    if (xSemaphoreTake(g_done, 0) != pdTRUE) {
        return 1;
    }
    return 0;
}
