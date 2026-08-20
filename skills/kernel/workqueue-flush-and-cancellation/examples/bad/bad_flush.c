/* BAD: the work_struct is freed after cancel_work() (async) but without
 * cancel_work_sync(). cancel_work() only dequeues the pending instance that
 * exists at that instant; a racing producer re-queues the item, so the
 * work_struct is freed while still PENDING — and the worker then reaches the
 * freed item (use-after-free in a real kernel). The stub detects it and
 * reports the bug instead of crashing. */
#include "../stubs.h"
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

/* producer: stands in for an ISR or another CPU submitting work */
static void kick(struct dev *d)
{
	queue_work_emu(&the_wq, &d->work);
}

static void remove_dev_bad(struct dev *d)
{
	/* BUG: cancel_work() is async. It removes a pending instance but gives
	 * no exclusion against a concurrent queue_work(), and never waits for
	 * a running instance. The racing producer below re-queues the item,
	 * then kfree(dev) happens with the work_struct still pending. */
	cancel_work_emu(&the_wq, &d->work);
	kick(d);			/* racing producer re-queues */
	mark_freed_emu(&d->work);	/* kfree(dev) while item is PENDING */
}

int main(void)
{
	struct dev d = { 0 };

	init_work_emu(&d.work, dev_work_func);
	kick(&d);			/* normal traffic */
	remove_dev_bad(&d);		/* device removal path */

	/* the worker runs: it reaches the freed item -> emulated UAF */
	run_pending_work_emu(&the_wq, 8);

	if (emulated_uaf_detected)
		printf("BUG reproduced: work ran after work_struct freed\n");
	else
		printf("no bug detected (unexpected)\n");
	return 0;
}
