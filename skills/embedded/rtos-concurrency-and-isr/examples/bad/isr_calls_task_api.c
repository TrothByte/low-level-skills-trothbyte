/* BAD: calling a task-context API from an ISR.
 * xQueueSend() may block when the queue is full, so it is not ISR-safe. The
 * ISR-safe variant is xQueueSendFromISR(), which never blocks and instead
 * reports via pxHigherPriorityTaskWoken whether a context switch is needed.
 * The stub treats a non-FromISR API call made while ISR context is active as
 * a rule violation; main() exits nonzero when one was recorded. */
#include "../freertos_stubs.h"

static QueueHandle_t g_queue;

static void vTimerISR(void) {
    int event = 1;
    xQueueSend(g_queue, &event, 0);   /* BAD: task API in ISR (may block) */
}

int main(void) {
    g_queue = xQueueCreate(4, sizeof(int));
    stub_enter_isr();
    vTimerISR();
    stub_exit_isr();
    return stub_failed() ? 1 : 0;
}
