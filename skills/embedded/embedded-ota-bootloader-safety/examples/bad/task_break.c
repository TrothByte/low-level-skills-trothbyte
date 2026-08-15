/*
 * BAD: // intentionally incorrect — FreeRTOS task `break` in a foreign loop.
 * In a task-shaped function, `break` exits the only enclosing loop, which is
 * the task's own loop: the function falls off the end and the task is deleted
 * by the idle task, hitting the fatal idle hook (configUSE_IDLE_HOOK) — the
 * device halts mid-OTA. In a real integration the "loop" may live in a helper
 * macro owned by someone else, making the break return out of the caller's
 * task.
 *
 * Host compile: gcc -Wall -Wextra -O2 task_break.c  (models the control flow)
 */
#include <stdio.h>

static int chunk_no = 0;

static int ota_step(void) {
    chunk_no++;
    if (chunk_no > 3)
        return -1;      /* "malformed chunk" arrives */
    return 1;
}

static void task_ota(void) {
    while (1) {
        int chunk = ota_step();
        if (chunk < 0)
            break;                  /* BUG: exits the task's own loop */
        printf("applied chunk\n");
    }
    /* falls off the end: task deleted -> idle hook fault in FreeRTOS */
}

int main(void) {
    task_ota();
    printf("task returned (fatal in FreeRTOS)\n");
    return 0;
}
