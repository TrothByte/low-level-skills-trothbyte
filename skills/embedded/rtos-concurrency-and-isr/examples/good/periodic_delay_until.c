/* GOOD: fixed-period task timing with vTaskDelayUntil() and stack monitoring.
 * vTaskDelay() sleeps relative to the moment it is called, so variable
 * processing time makes the period drift. vTaskDelayUntil() sleeps to an
 * absolute wake time (*pxPreviousWakeTime + xTimeIncrement) and updates the
 * wake time itself, giving a constant period. The stub advances a simulated
 * tick counter and also reports a stack high-water mark, so a task can
 * validate that its stack is sized safely at runtime. main() exits 0 only if
 * the period is exact and the high-water mark looks sane. */
#include "../freertos_stubs.h"

static void vPeriodicTask(void *unused) {
    (void)unused;
}

static void do_processing(void) {
    /* Simulated variable-length work: takes 2 ticks. */
    g_stub.tick += 2;
}

static TickType_t run_fixed_period(void) {
    TickType_t last;
    g_stub.tick = 0;
    last = xTaskGetTickCount();   /* must be initialised with the current time */
    for (int i = 0; i < 5; i++) {
        do_processing();
        vTaskDelayUntil(&last, 10);   /* absolute wake time: exact 10-tick period */
    }
    return xTaskGetTickCount();
}

static TickType_t run_drifting(void) {
    g_stub.tick = 0;
    for (int i = 0; i < 5; i++) {
        do_processing();
        vTaskDelay(10);               /* relative delay: period = 12 ticks, drifts */
    }
    return xTaskGetTickCount();
}

int main(void) {
    TaskHandle_t h = NULL;
    UBaseType_t high_water;
    TickType_t fixed, drifting;

    (void)xTaskCreate(vPeriodicTask, "periodic", 256, NULL, 3, &h);
    high_water = uxTaskGetStackHighWaterMark(h);

    fixed = run_fixed_period();
    drifting = run_drifting();

    if (stub_failed()) {
        return 1;
    }
    /* vTaskDelayUntil gives exactly 5 x 10 = 50 ticks. */
    if (fixed != 50) {
        return 1;
    }
    /* vTaskDelay accumulates the processing time: 5 x (10 + 2) = 60 ticks. */
    if (drifting != 60) {
        return 1;
    }
    if (high_water == 0u) {
        return 1;
    }
    return 0;
}
