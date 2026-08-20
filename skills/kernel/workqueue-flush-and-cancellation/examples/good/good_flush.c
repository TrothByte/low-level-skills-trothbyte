/* GOOD: flush/cancel discipline — the work_struct is only freed after the
 * work is guaranteed idle (cancel_work_sync), so it never runs after free.
 * Deterministic, no threads: the emulator executes work items itself. */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>

struct dev {
	int processed;
	struct work_struct work;
};

static void dev_work_func(struct work_struct *work)
{
	struct dev *d = container_of(work, struct dev, work);
	d->processed++;
}

int main(void)
{
	struct dev d = { 0 };

	init_work_emu(&d.work, dev_work_func);

	/* queue_work: IDLE -> PENDING; returns true on the first queue */
	assert(queue_work_emu(&the_wq, &d.work));
	assert(d.processed == 0);

	/* flush_work returns true when it had to wait (item was pending),
	 * and the item has now executed to completion */
	assert(flush_work_emu(&the_wq, &d.work));
	assert(d.processed == 1);

	/* flush_work on an already-idle item returns false */
	assert(flush_work_emu(&the_wq, &d.work) == false);
	assert(d.processed == 1);

	/* explicit runner executes one pending item */
	assert(queue_work_emu(&the_wq, &d.work));
	assert(run_pending_work_emu(&the_wq, 1) == 1);
	assert(d.processed == 2);

	/* teardown: cancel_work_sync removes a pending item so it never
	 * executes; second call finds nothing and returns false */
	assert(queue_work_emu(&the_wq, &d.work));
	assert(cancel_work_sync_emu(&the_wq, &d.work));
	assert(d.processed == 2);
	assert(cancel_work_sync_emu(&the_wq, &d.work) == false);

	/* only now, with the item guaranteed idle, is the free legal */
	mark_freed_emu(&d.work);

	/* nothing may run after the free */
	assert(run_pending_work_emu(&the_wq, 8) == 0);
	assert(emulated_uaf_detected == 0);
	assert(d.processed == 2);

	printf("ALL CHECKS PASSED\n");
	return 0;
}
