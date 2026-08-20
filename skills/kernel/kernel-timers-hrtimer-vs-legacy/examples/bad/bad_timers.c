/* BAD: frees timer data after del_timer() (not del_timer_sync()) while the
 * expired callback is still queued to run — the classic timer use-after-
 * free (module-unload / device-close race). The stubs detect the orphaned
 * callback firing on the "freed" data and report it. Also shown: freeing
 * hrtimer data without hrtimer_cancel(). Prints "BUG reproduced: ..." lines
 * and exits 0. */
#include "../stubs.h"
#include <stdio.h>
#include <stdlib.h>

static int bug_reported;

static void report_bug(const char *msg)
{
	printf("%s\n", msg);
	bug_reported = 1;
}

/* ---------------- legacy timer_list bug: del_timer without sync --------- */

struct legacy_device {
	struct legacy_timer tmr;
	int freed;		/* 1 = the driver "freed" the device data */
	unsigned long fires;
};

static void legacy_irq_timeout(struct legacy_timer *t, void *p)
{
	struct legacy_device *dev = (struct legacy_device *)p;
	(void)t;
	dev->fires++;
	if (dev->freed)
		report_bug("BUG reproduced: timer fired after data freed "
			   "(del_timer without sync)");
}

static void bad_legacy(void)
{
	struct legacy_device *dev = malloc(sizeof *dev);
	if (!dev)
		return;
	dev->freed = 0;
	dev->fires = 0;
	timer_setup_emu(&dev->tmr, legacy_irq_timeout, dev);

	/* arm a 3-tick timeout */
	mod_timer_emu(&dev->tmr, 3);

	/* simulate the race: the timer expires and its callback is queued to
	 * run in softirq; teardown (rmmod/close) happens right now. */
	legacy_defer_fire = 1;
	timer_tick_emu();
	timer_tick_emu();
	timer_tick_emu();
	legacy_defer_fire = 0;

	/* BAD: del_timer() only. It returns 0 here because the timer is no
	 * longer pending (it already expired) — but the callback is still
	 * queued. del_timer_sync() was required. */
	if (del_timer_emu(&dev->tmr) != 0) {
		/* not the scenario we want to show; teardown is safe */
		free(dev);
		return;
	}

	/* BAD: free the data the queued callback still references. */
	dev->freed = 1;

	/* the queued softirq callback finally runs, on "freed" data */
	timer_tick_emu();

	free(dev);
}

/* ------------------- hrtimer bug: no hrtimer_cancel before free --------- */

struct hrtimer_device {
	struct hrtimer tmr;
	int freed;
	int fires;
};

static int hrt_irq_timeout(struct hrtimer *t, void *p)
{
	struct hrtimer_device *dev = (struct hrtimer_device *)p;
	(void)t;
	dev->fires++;
	if (dev->freed)
		report_bug("BUG reproduced: hrtimer callback ran after data "
			   "freed (no hrtimer_cancel)");
	return HRTIMER_NORESTART;
}

static void bad_hrtimer(void)
{
	struct hrtimer_device *dev = malloc(sizeof *dev);
	if (!dev)
		return;
	dev->freed = 0;
	dev->fires = 0;

	/* start a 1000 ns one-shot */
	hrtimer_start_emu(&dev->tmr, 1000, HRTIMER_MODE_ABS, hrt_irq_timeout,
			  dev);

	/* BAD: teardown frees the device without hrtimer_cancel() first. */
	dev->freed = 1;

	/* the queued callback is due and runs, on "freed" data */
	hrtimer_run_emu(1000);

	free(dev);
}

int main(void)
{
	bad_legacy();
	bad_hrtimer();
	if (!bug_reported)
		printf("BUG NOT REPRODUCED (harness regression)\n");
	return 0;
}
