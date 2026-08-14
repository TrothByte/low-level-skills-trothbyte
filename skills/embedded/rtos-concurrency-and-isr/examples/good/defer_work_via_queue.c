/* GOOD: deferred interrupt processing (the no-deferral counterexample).
 * The ISR does the minimum: queue a pointer with the ISR-safe API and request
 * a context switch with portYIELD_FROM_ISR() if a receiver was woken. The
 * actual processing happens in the consumer task, which runs outside ISR
 * context, so the ISR stays short and the system stays responsive. The stub
 * verifies the yield request was made and that no rule violation occurred. */
#include "../freertos_stubs.h"

static QueueHandle_t g_work_queue;
static int g_payload;

static void vUARTISR(void) {
    int *p_item = &g_payload;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /* ISR-safe, non-blocking: queues a pointer (queued by copy). */
    xQueueSendFromISR(g_work_queue, &p_item, &xHigherPriorityTaskWoken);
    /* Ask the scheduler to switch to the woken consumer if needed. */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static int vConsumerTask(void) {
    int *p = NULL;
    int processed = 0;
    while (xQueueReceive(g_work_queue, &p, 0) == pdTRUE) {
        if (*p == g_payload) {
            processed++;
        }
    }
    return processed;   /* heavy processing happens here, outside the ISR */
}

int main(void) {
    int processed;
    g_work_queue = xQueueCreate(4, sizeof(int *));
    g_payload = 42;
    stub_enter_isr();
    vUARTISR();
    stub_exit_isr();
    processed = vConsumerTask();
    if (stub_failed()) {
        return 1;
    }
    if (stub_yields_requested() == 0) {
        return 1;   /* the woken consumer should have requested a yield */
    }
    return processed == 1 ? 0 : 1;
}
