/* BAD: typical waitqueue/completion agent bugs, detected by the stubs.
 * Demonstrates: (1) condition stored AFTER wake_up (lost wakeup),
 * (2) complete_all reuse without reinit (stale done), (3) ignored
 * interruptible return, (4) wait_for_completion in atomic context,
 * (5) freeing a completion with a parked waiter. Exits 0 after printing
 * BUG reproduced lines — the stubs simulate the faults safely. */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>

static struct wait_queue_head_emu wq;
static struct completion_emu c;
static int cond;
static int lock_held;

static void bug_lost_wakeup(void)
{
	init_waitqueue_emu(&wq);
	cond = 0;

	/* The classic lost-wakeup race, modeled step by step:
	 *   1. waiter checks the condition (false) and decides to sleep
	 *   2. waker runs entirely between check and park: sets the condition
	 *      and calls wake_up while no waiter is parked yet
	 *   3. waiter parks — and (buggy pattern) never re-checks
	 * The wait_event loop would save this; the manual check-then-park
	 * pattern below loses the wake. */

	/* step 1: waiter's single check */
	int precheck = cond;              /* == 0, so waiter will sleep */

	/* step 2: waker runs now — condition set and wake fired while nobody
	 * is parked yet (the waiter has not reached park_forever) */
	cond = 1;
	if (wake_up_emu(&wq) == 0) {
		/* wake found no parked waiter: the event is about to be lost */
	}

	/* step 3: buggy waiter parks unconditionally, never re-checking
	 * (a correct wait_event would re-check cond and return immediately) */
	if (precheck == 0) {
		park_forever_emu(&wq);
		wq_report_lost_wakeup();
		printf("BUG reproduced: condition stored after wake point (lost wakeup)\n");
	}
}

static void bug_stale_completion(void)
{
	init_completion_emu(&c);

	/* Bug: complete_all leaves done>0; reusing without reinit makes the
	 * next wait_for_completion return immediately even though the event
	 * never occurred. */
	complete_all_emu(&c);
	assert(c.waiting == 0);
	if (wait_for_completion_emu(&c) == 0) {
		/* second event never happened, but the wait "succeeded" */
		printf("BUG reproduced: completion reused without reinit (stale done)\n");
	}
}

static void bug_ignored_interruptible(void)
{
	init_completion_emu(&c);

	/* Bug: interruptible wait return ignored — code proceeds even though
	 * a signal interrupted the wait and the event never occurred. */
	int r = wait_for_completion_interruptible_emu(&c, 1 /* signal */);
	if (r != 0) {
		printf("BUG reproduced: interruptible wait returned %d but return ignored\n",
		       r);
	} else {
		printf("BUG reproduced: interruptible wait return mishandled\n");
	}
}

static void bug_atomic_sleep(void)
{
	init_completion_emu(&c);

	/* Bug: wait_for_completion while "holding a spinlock" (atomic
	 * context) — sleeping is forbidden there. */
	lock_held = 1;
	if (wait_for_completion_emu(&c) == 1) {
		wq_report_atomic_sleep();
		printf("BUG reproduced: wait_for_completion in atomic context\n");
	}
	lock_held = 0;
}

static void bug_uaf_completion(void)
{
	init_completion_emu(&c);

	/* Bug: completion freed (kfree) while a waiter is still parked. */
	wait_for_completion_emu(&c); /* waiter parks */
	assert(c.waiting == 1);
	mark_freed_emu(&c);          /* kfree(drv->done) too early */
	if (c.state == 1 && c.waiting > 0) {
		wq_report_uaf();
		printf("BUG reproduced: completion freed while waiter parked (UAF)\n");
	}
}

int main(void)
{
	bug_lost_wakeup();
	bug_stale_completion();
	bug_ignored_interruptible();
	bug_atomic_sleep();
	bug_uaf_completion();
	return 0;
}
