/* BAD: two typical agent mistakes in kthread teardown, detected at runtime
 * by the stubs' lifecycle machinery. Deterministic; exits 0.
 *
 * (1) "frees the thread's data, then calls kthread_stop" — the order
 *     inversion. The freed-data sentinel detects the resource being freed
 *     while the simulated thread is still alive.
 * (2) "kthread_stop on an already-exited task" — the threadfn returns on
 *     its own (never polling kthread_should_stop), and the driver still
 *     calls kthread_stop() on the dead task.
 */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct work_state {
	int *items;
	int len;
	int next;
	long sum;
};

static int worker(void *data)
{
	struct work_state *ws = data;

	if (kthread_should_stop_emu())
		return (int)ws->sum;
	if (ws->next < ws->len) {
		ws->sum += ws->items[ws->next];
		ws->next++;
	}
	return KT_RUNNING;
}

/* BAD threadfn: finishes its job and returns without ever polling
 * kthread_should_stop(). The driver wrongly assumes it can kthread_stop()
 * this task later. */
static int one_shot(void *data)
{
	(void)data;
	return 42;
}

int main(void)
{
	struct work_state ws;
	struct ktask *t;

	/* (1) free the thread's data first, then kthread_stop */
	ws.items = (int *)malloc(2 * sizeof(int));
	ws.items[0] = 5;
	ws.items[1] = 6;
	ws.len = 2;
	ws.next = 0;
	ws.sum = 0;
	t = kthread_run_emu(worker, &ws, "bad1");
	assert(t->state == KT_ST_RUNNING);
	assert(ws.sum == 5); /* wake step consumed items[0] */
	run_thread_step_emu(t); /* thread still alive, still touching data */
	assert(ws.sum == 11);

	kthread_data_free_emu(t, ws.items); /* BUG reproduced here */
	ws.items = NULL;
	kthread_stop_emu(t); /* thread's data was already gone */
	assert(t->state == KT_ST_EXITED);

	/* (2) kthread_stop on a task that already exited on its own */
	ws.items = NULL;
	ws.len = 0;
	ws.next = 0;
	ws.sum = 0;
	t = kthread_run_emu(one_shot, &ws, "bad2");
	while (t->state != KT_ST_EXITED)
		run_thread_step_emu(t); /* thread returns 42 and exits */
	assert(t->state == KT_ST_EXITED);

	kthread_stop_emu(t); /* BUG reproduced here */

	return 0;
}
