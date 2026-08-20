/* GOOD: correct kthread lifecycle.
 *
 * kthread_run() a thread whose threadfn loops polling kthread_should_stop(),
 * let it process items, then kthread_stop() it while it is alive and free
 * its resources only after the stop returns. The order stop-before-free is
 * asserted by the harness's freed-data sentinel. Deterministic: the stubs
 * advance the simulated thread one step at a time; no real threads.
 */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct work_state {
	int *items;
	int len;
	int next;
	long sum;
};

/* threadfn: one loop iteration. Polls kthread_should_stop() (returns when
 * true), otherwise processes one queued item and keeps polling. */
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

/* threadfn that only exits when stopped (the clean kthread pattern) */
static int noop(void *data)
{
	(void)data;
	if (kthread_should_stop_emu())
		return 0;
	return KT_RUNNING;
}

int main(void)
{
	static const int seed[] = { 10, 20, 30 };
	struct work_state ws;
	struct ktask *t;
	int ret;

	/* kthread_create() does NOT start the thread: state is CREATED and
	 * no step has run. kthread_stop() on a created (never-woken) task is
	 * still valid: it sets the flag and the first iteration exits. */
	ws.items = NULL;
	ws.len = 0;
	ws.next = 0;
	ws.sum = 0;
	t = kthread_create_emu(noop, &ws, "created");
	assert(t->state == KT_ST_CREATED);
	assert(t->steps == 0);
	kthread_stop_emu(t);
	assert(t->state == KT_ST_EXITED);

	/* kthread_run() = create + wake: the thread is running and the
	 * first iteration executed. */
	ws.items = (int *)malloc(3 * sizeof(int));
	memcpy(ws.items, seed, sizeof(seed));
	ws.len = 3;
	ws.next = 0;
	ws.sum = 0;
	t = kthread_run_emu(worker, &ws, "good-worker");
	assert(t->state == KT_ST_RUNNING);
	assert(ws.next == 1 && ws.sum == 10);

	/* let the thread drain the queue, polling kthread_should_stop() on
	 * every step */
	while (ws.next < ws.len)
		run_thread_step_emu(t);
	assert(ws.next == 3 && ws.sum == 60);

	/* producer hands the thread one more item while it is alive */
	ws.items = (int *)realloc(ws.items, 4 * sizeof(int));
	ws.items[3] = 7;
	ws.len = 4;
	run_thread_step_emu(t);
	assert(ws.sum == 67);
	assert(t->state == KT_ST_RUNNING);

	/* kthread_stop() while alive: sets the stop flag, the threadfn sees
	 * kthread_should_stop() and returns, and kthread_stop() returns the
	 * threadfn's return value. Free the resources only AFTER the stop. */
	ret = kthread_stop_emu(t);
	assert(ret == 67);
	assert(t->state == KT_ST_EXITED);
	assert(!t->alive);
	kthread_data_free_emu(t, ws.items); /* stopped first, then free */
	ws.items = NULL;

	printf("ALL CHECKS PASSED\n");
	return 0;
}
