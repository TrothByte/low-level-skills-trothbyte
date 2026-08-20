/* GOOD: correct use of both Linux kernel timer APIs, exercised against the
 * deterministic host stubs. Every contract is asserted: mod_timer return
 * values, firing on the right tick, del_timer vs del_timer_sync, hrtimer
 * ABS/REL modes, hrtimer_start replacing a pending expiry, HRTIMER_RESTART
 * self-restart, callback context flags, and teardown ordering before the
 * callback data is freed. Prints "ALL CHECKS PASSED" and returns 0. */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- legacy timer_list: watchdog style ---------------- */

struct legacy_data {
	struct legacy_timer tmr;
	unsigned long fires;
	int ctx_softirq;	/* context flag observed inside the callback */
	int ctx_no_sleep;	/* sleep-forbidden flag observed in callback */
};

static void legacy_cb(struct legacy_timer *t, void *p)
{
	struct legacy_data *d = (struct legacy_data *)p;
	(void)t;
	d->fires++;
	d->ctx_softirq = legacy_in_softirq_ctx;
	d->ctx_no_sleep = context_forbids_sleep_emu();
}

static void legacy_teardown_sync(struct legacy_data *d)
{
	/* unload ordering: stop arming, then wait for a running/queued
	 * callback (del_timer_sync), then the caller frees the data */
	(void)del_timer_sync_emu(&d->tmr);
}

/* ---------------- hrtimer: one-shot and periodic callbacks ---------------- */

struct hrtimer_data {
	struct hrtimer tmr;
	int fires;
	int ctx_hard;		/* 1 = callback ran in hardirq context */
	int ctx_no_sleep;	/* sleep-forbidden flag inside the callback */
};

static int hrt_cb_once(struct hrtimer *t, void *p)
{
	struct hrtimer_data *hd = (struct hrtimer_data *)p;
	(void)t;
	hd->fires++;
	hd->ctx_hard = hrtimer_in_hardirq_ctx;
	hd->ctx_no_sleep = context_forbids_sleep_emu();
	return HRTIMER_NORESTART;
}

static int hrt_cb_periodic(struct hrtimer *t, void *p)
{
	struct hrtimer_data *hd = (struct hrtimer_data *)p;
	hd->fires++;
	hd->ctx_hard = hrtimer_in_hardirq_ctx;
	if (hd->fires < 3) {
		t->expires_ns += 1000;	/* advance the next fire time */
		return HRTIMER_RESTART;
	}
	return HRTIMER_NORESTART;
}

int main(void)
{
	/* ---- legacy: arm, fire on tick, del_timer_sync before free ---- */
	struct legacy_data *ld = calloc(1, sizeof *ld);
	assert(ld != NULL);
	timer_setup_emu(&ld->tmr, legacy_cb, ld);

	assert(mod_timer_emu(&ld->tmr, 3) == 0);	/* was inactive -> 0 */
	assert(timer_pending_emu(&ld->tmr) == 1);
	assert(mod_timer_emu(&ld->tmr, 5) == 1);	/* was active -> 1 */

	timer_tick_emu();				/* t = 1 */
	assert(ld->fires == 0);				/* not due yet */
	timer_tick_emu(); timer_tick_emu();
	timer_tick_emu(); timer_tick_emu();		/* t = 5 */
	assert(ld->fires == 1);				/* fired at the tick */
	assert(ld->ctx_softirq == 1);			/* softirq context */
	assert(ld->ctx_no_sleep == 1);			/* BH disabled: no sleep */

	/* re-arm and delete a pending timer: del_timer returns was-pending */
	assert(mod_timer_emu(&ld->tmr, 2) == 0);	/* inactive after fire */
	assert(del_timer_emu(&ld->tmr) == 1);		/* was pending -> 1 */
	assert(ld->fires == 1);				/* never fired again */

	/* the dangerous window: the timer expires and its callback is queued
	 * (softirq has not run it yet). del_timer_sync must wait for it
	 * before the data can be freed. */
	legacy_defer_fire = 1;
	mod_timer_emu(&ld->tmr, 1);
	timer_tick_emu();				/* expires; queued */
	legacy_defer_fire = 0;
	assert(timer_pending_emu(&ld->tmr) == 0);
	assert(del_timer_sync_emu(&ld->tmr) == 0);	/* was not pending */
	assert(ld->fires == 2);				/* sync ran queued cb */
	legacy_teardown_sync(ld);			/* idempotent after that */
	free(ld);					/* data is safe now */

	/* ---- hrtimer: ABS expiry, replace, REL, restart, cancel ---- */
	struct hrtimer_data hd;
	memset(&hd, 0, sizeof hd);

	/* B1: ABS mode fires at the absolute ns expiry; hrtimer_start() on an
	 * active timer replaces the pending expiry and reports was-active. */
	hrtimer_start_emu(&hd.tmr, 1000, HRTIMER_MODE_ABS, hrt_cb_once, &hd);
	assert(hrtimer_active_emu(&hd.tmr) == 1);
	assert(hrtimer_is_queued_emu(&hd.tmr) == 1);
	assert(hrtimer_start_emu(&hd.tmr, 3000, HRTIMER_MODE_ABS,
				 hrt_cb_once, &hd) == 1);
	hrtimer_run_emu(1500);
	assert(hd.fires == 0);				/* old 1000 replaced */
	hrtimer_run_emu(3000);
	assert(hd.fires == 1);
	assert(hd.ctx_hard == 1);			/* hardirq by default */
	assert(hd.ctx_no_sleep == 1);			/* cannot sleep */

	/* B2: REL mode is converted to an absolute expiry at arming time. */
	hd.fires = 0;
	hrtimer_global_now_ns = 0;
	hrtimer_start_emu(&hd.tmr, 500, HRTIMER_MODE_REL, hrt_cb_once, &hd);
	hrtimer_run_emu(400);
	assert(hd.fires == 0);				/* now + 500 = 900 */
	hrtimer_run_emu(1000);
	assert(hd.fires == 1);

	/* B3: a periodic hrtimer self-restarts by returning HRTIMER_RESTART
	 * after advancing expires_ns; HRTIMER_NORESTART stops it. */
	hd.fires = 0;
	hrtimer_global_now_ns = 0;
	hrtimer_start_emu(&hd.tmr, 1000, HRTIMER_MODE_ABS, hrt_cb_periodic, &hd);
	hrtimer_run_emu(1000);
	assert(hd.fires == 1);
	assert(hd.tmr.restart_ret == HRTIMER_RESTART);
	assert(hrtimer_active_emu(&hd.tmr) == 1);	/* re-queued */
	hrtimer_run_emu(2000);
	assert(hd.fires == 2);
	assert(hd.tmr.restart_ret == HRTIMER_RESTART);
	hrtimer_run_emu(3000);
	assert(hd.fires == 3);
	assert(hd.tmr.restart_ret == HRTIMER_NORESTART);
	assert(hrtimer_active_emu(&hd.tmr) == 0);	/* stopped */

	/* B4: hrtimer_cancel before free prevents the callback from running. */
	hd.fires = 0;
	hrtimer_global_now_ns = 0;
	hrtimer_start_emu(&hd.tmr, 5000, HRTIMER_MODE_REL, hrt_cb_once, &hd);
	assert(hrtimer_cancel_emu(&hd.tmr) == 1);	/* was active */
	assert(hrtimer_active_emu(&hd.tmr) == 0);
	assert(hrtimer_cancel_emu(&hd.tmr) == 0);	/* already inactive */
	hrtimer_run_emu(6000);
	assert(hd.fires == 0);				/* never fired */

	/* B5: _SOFT mode runs the callback in softirq context, not hardirq. */
	hd.fires = 0;
	hrtimer_global_now_ns = 0;
	hrtimer_start_emu(&hd.tmr, 1000, HRTIMER_MODE_ABS_SOFT, hrt_cb_once,
			  &hd);
	hrtimer_run_emu(1000);
	assert(hd.fires == 1);
	assert(hd.ctx_hard == 0);			/* softirq context */
	assert(hd.ctx_no_sleep == 1);			/* still cannot sleep */

	printf("ALL CHECKS PASSED\n");
	return 0;
}
