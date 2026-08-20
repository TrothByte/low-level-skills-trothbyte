/* GOOD: correct waitqueue and completion usage.
 * Demonstrates: condition-before-wake ordering, wait_event re-check,
 * spurious-wakeup tolerance, one-shot complete, complete_all broadcast,
 * reinit for reuse, and interruptible-return handling. */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	/* --- 1. condition-before-wake: no lost wakeup --- */
	struct wait_queue_head_emu wq;
	init_waitqueue_emu(&wq);
	int ready = 0;

	/* waker: store condition, then wake */
	ready = 1;
	assert(wake_up_emu(&wq) >= 0); /* no one parked yet is fine */

	/* waiter: condition already true -> returns immediately, no park */
	assert(wait_event_emu(&wq, ready) == 0);
	assert(wq.waiters == 0);

	/* --- 2. wait_event parks, wake unparks, condition re-checked --- */
	init_waitqueue_emu(&wq);
	ready = 0;

	/* waiter parks because condition false */
	assert(wait_event_emu(&wq, ready) == 1);
	assert(wq.waiters == 1);

	/* a spurious wake with condition still false: the waiter re-parks
	 * (the harness simulates the loop) — this must NOT be treated as
	 * completion. */
	ready = 0;
	{
		int n = wake_up_emu(&wq);
		assert(n == 1);
		/* waiter re-checks: condition still false -> re-park */
		unpark_emu(&wq);
		assert(wait_event_emu(&wq, ready) == 1);
		assert(wq.waiters == 1);
	}

	/* real event: condition true before wake -> waiter proceeds */
	ready = 1;
	assert(wake_up_emu(&wq) == 1);
	unpark_emu(&wq);
	assert(wait_event_emu(&wq, ready) == 0);
	assert(wq.waiters == 0);

	/* --- 3. completion: one-shot, complete wakes exactly one --- */
	struct completion_emu c;
	init_completion_emu(&c);

	/* waiter parks; complete() wakes one */
	assert(wait_for_completion_emu(&c) == 1); /* parked */
	assert(c.waiting == 1);
	complete_emu(&c);
	assert(c.waiting == 0);

	/* after completion, later waits return immediately */
	assert(wait_for_completion_emu(&c) == 0);

	/* --- 4. complete_all broadcasts to all current waiters --- */
	init_completion_emu(&c);
	assert(wait_for_completion_emu(&c) == 1);
	assert(wait_for_completion_emu(&c) == 1);
	assert(c.waiting == 2);
	complete_all_emu(&c);
	assert(c.waiting == 0);
	/* future waiters also return immediately (done stays >0) */
	assert(wait_for_completion_emu(&c) == 0);

	/* --- 5. reinit for reuse --- */
	reinit_completion_emu(&c);
	assert(wait_for_completion_emu(&c) == 1); /* parks again */

	/* --- 6. interruptible return handling --- */
	init_completion_emu(&c);
	{
		int r = wait_for_completion_interruptible_emu(&c, 1 /* signal */);
		assert(r == ERESTARTSYS_EMU);
		/* caller must abort, not proceed */
	}
	init_completion_emu(&c);
	complete_emu(&c); /* completed before wait */
	assert(wait_for_completion_interruptible_emu(&c, 0) == 0);

	/* --- 7. timeout: event fires before timeout -> completed --- */
	init_completion_emu(&c);
	complete_emu(&c);
	assert(wait_for_completion_timeout_emu(&c, 0) == 1);

	/* --- 8. no lost wakeup, no spurious wake, no atomic sleep, no UAF --- */
	assert(wq_lost_wakeup() == 0);
	assert(wq_spurious() == 0);
	assert(wq_atomic_sleep() == 0);
	assert(wq_uaf() == 0);

	printf("ALL CHECKS PASSED\n");
	return 0;
}
