/* GOOD: priority inversion fixed by using a mutex, which has priority
 * inheritance. This file is identical to examples/bad/priority_inversion.c
 * except that the lock is created with xSemaphoreCreateMutex() instead of
 * xSemaphoreCreateBinary(). When high-priority H blocks on the mutex held by
 * low-priority L, the kernel temporarily raises L to H's priority (inherited
 * priority), so L runs instead of the unrelated medium-priority M, finishes,
 * and releases the lock promptly. H completes quickly and main() exits 0. */
#include "../freertos_stubs.h"

#define NTASKS 3

typedef struct {
    int priority;        /* FreeRTOS: larger number = higher priority */
    int work_remaining;
    int wants_lock;
    int holds_lock;
    int blocked;
    int done;
    int arrive_at;
    int started;
} sim_task_t;

static sim_task_t tasks[NTASKS];
static SemaphoreHandle_t lock;
static int lock_holder;
static int inheritance;

static void init_tasks(void) {
    int i;
    for (i = 0; i < NTASKS; i++) {
        tasks[i].priority = 0;
        tasks[i].work_remaining = 0;
        tasks[i].wants_lock = 0;
        tasks[i].holds_lock = 0;
        tasks[i].blocked = 0;
        tasks[i].done = 0;
        tasks[i].arrive_at = 0;
        tasks[i].started = 0;
    }
    tasks[0].priority = 1;
    tasks[0].work_remaining = 2;
    tasks[0].wants_lock = 1;
    tasks[0].holds_lock = 1;
    tasks[0].arrive_at = 0;
    tasks[1].priority = 2;
    tasks[1].work_remaining = 12;
    tasks[1].arrive_at = 0;
    tasks[2].priority = 3;
    tasks[2].work_remaining = 2;
    tasks[2].wants_lock = 1;
    tasks[2].arrive_at = 1;
    lock_holder = 0;
    tasks[0].started = 1;
    tasks[1].started = 1;
}

static int effective_priority(int i) {
    int p = tasks[i].priority;
    if (inheritance && i == lock_holder) {
        for (int j = 0; j < NTASKS; j++) {
            if (tasks[j].blocked && tasks[j].priority > p) {
                p = tasks[j].priority;
            }
        }
    }
    return p;
}

static int run_simulation(void) {
    int tick = 0;
    while (tick < 1000) {
        int best = -1;
        int best_eff = -1;
        for (int i = 0; i < NTASKS; i++) {
            if (!tasks[i].started && tick >= tasks[i].arrive_at) {
                tasks[i].started = 1;
                if (tasks[i].wants_lock && !tasks[i].holds_lock &&
                    lock_holder >= 0) {
                    tasks[i].blocked = 1;
                }
            }
        }
        if (lock_holder < 0) {
            int best_waiter = -1;
            for (int i = 0; i < NTASKS; i++) {
                if (tasks[i].started && !tasks[i].done &&
                    tasks[i].wants_lock && !tasks[i].holds_lock) {
                    if (best_waiter < 0 || tasks[i].priority > tasks[best_waiter].priority) {
                        best_waiter = i;
                    }
                }
            }
            if (best_waiter >= 0) {
                (void)xSemaphoreTake(lock, portMAX_DELAY);
                tasks[best_waiter].blocked = 0;
                tasks[best_waiter].holds_lock = 1;
                lock_holder = best_waiter;
            }
        }
        for (int i = 0; i < NTASKS; i++) {
            int eff;
            if (!tasks[i].started || tasks[i].done || tasks[i].blocked) {
                continue;
            }
            if (tasks[i].work_remaining <= 0) {
                continue;
            }
            if (tasks[i].wants_lock && !tasks[i].holds_lock) {
                continue;
            }
            eff = effective_priority(i);
            if (eff > best_eff) {
                best_eff = eff;
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        tasks[best].work_remaining--;
        if (tasks[best].work_remaining == 0) {
            tasks[best].done = 1;
            if (tasks[best].holds_lock) {
                (void)xSemaphoreGive(lock);
                tasks[best].holds_lock = 0;
                lock_holder = -1;
            }
            if (best == 2) {
                return tick;
            }
        }
        tick++;
    }
    return 1000;
}

int main(void) {
    int finish;
    init_tasks();
    lock = xSemaphoreCreateMutex();   /* GOOD: mutex has priority inheritance */
    inheritance = stub_semaphore_is_mutex(lock);
    finish = run_simulation();
    if (stub_failed()) {
        return 1;
    }
    /* With inheritance H completes as soon as L's short work is done. */
    return finish > 8 ? 1 : 0;
}
