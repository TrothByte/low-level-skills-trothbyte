/*
 * stubs.h — self-contained host stubs for Linux kernel timer-shaped code.
 * Simulates the legacy timer_list API and the high-resolution hrtimer API
 * as deterministic state machines so their contracts can be exercised with
 * a plain gcc build: mod_timer/del_timer/del_timer_sync return values,
 * firing at the right time, restart semantics, callback context flags, and
 * teardown ordering. No kernel headers required. Not kernel code.
 *
 * The stubs also model the kernel's hard races: after a timer expires its
 * callback is "queued to run in softirq" (legacy_defer_fire); a bare
 * del_timer() on such a timer returns 0 but leaves the callback queued, so
 * a later timer_tick_emu() fires it — exactly the use-after-free window
 * del_timer_sync()/hrtimer_cancel() exists to close.
 */
#ifndef KERNEL_TIMERS_STUBS_H
#define KERNEL_TIMERS_STUBS_H

#include <stddef.h>
#include <stdint.h>

/* ----------------------- legacy timer_list emulation -------------------- */

typedef enum {
	LEGACY_TIMER_IDLE = 0,
	LEGACY_TIMER_ARMED,
	LEGACY_TIMER_EXPIRED,	/* fired; callback still queued (simulated) */
} legacy_timer_state_t;

struct legacy_timer;

/* Callback contract mirrors the kernel: invoked in softirq context (bottom
 * halves disabled) and therefore must not sleep. */
typedef void (*legacy_timer_fn_t)(struct legacy_timer *, void *);

struct legacy_timer {
	legacy_timer_state_t state;
	unsigned long expires_at;	/* absolute tick of the next fire */
	legacy_timer_fn_t callback;
	void *data;
};

unsigned long legacy_global_tick;	/* simulated jiffies counter */
int legacy_in_softirq_ctx;		/* 1 while a legacy callback runs */
int legacy_defer_fire;			/* 1 = expired callback is only queued
					 * (softirq delay) instead of running
					 * immediately */
struct legacy_timer *legacy_still_running; /* orphaned callback after del */

#define LEGACY_MAX_TIMERS 8
struct legacy_timer *legacy_registry[LEGACY_MAX_TIMERS];
int legacy_registry_count;

/* Mirrors timer_setup(): initialises the timer; a callback fires only after
 * mod_timer_emu() arms it. */
static inline void timer_setup_emu(struct legacy_timer *t,
				   legacy_timer_fn_t callback, void *data)
{
	int i;
	t->state = LEGACY_TIMER_IDLE;
	t->expires_at = 0;
	t->callback = callback;
	t->data = data;
	for (i = 0; i < legacy_registry_count; i++)
		if (legacy_registry[i] == t)
			return;
	if (legacy_registry_count < LEGACY_MAX_TIMERS)
		legacy_registry[legacy_registry_count++] = t;
}

static inline int timer_pending_emu(const struct legacy_timer *t)
{
	return t->state == LEGACY_TIMER_ARMED;
}

/* mod_timer(): arms (or re-arms) the timer at now + jiffies_delta. Returns
 * 1 if the timer was active (pending) before the call, 0 if it was
 * inactive — the kernel contract (kernel/time/timer.c). */
static inline int mod_timer_emu(struct legacy_timer *t,
				unsigned long jiffies_delta)
{
	int was_active = (t->state == LEGACY_TIMER_ARMED);
	t->expires_at = legacy_global_tick + jiffies_delta;
	t->state = LEGACY_TIMER_ARMED;
	return was_active;
}

/* del_timer(): removes a pending timer and returns 1 if it was pending, 0
 * if not. It does NOT wait for a callback that already expired and is still
 * queued to run: that callback is recorded so a later timer_tick_emu()
 * fires it (the kernel SMP race). */
static inline int del_timer_emu(struct legacy_timer *t)
{
	if (t->state == LEGACY_TIMER_ARMED) {
		t->state = LEGACY_TIMER_IDLE;
		return 1;
	}
	if (t->state == LEGACY_TIMER_EXPIRED) {
		legacy_still_running = t;
		return 0;
	}
	return 0;
}

/* del_timer_sync(): removes the timer AND waits until its callback is no
 * longer running. In the simulation an expired-but-queued callback is run
 * to completion before returning. */
static inline int del_timer_sync_emu(struct legacy_timer *t)
{
	int was_pending = (t->state == LEGACY_TIMER_ARMED);
	if (t->state == LEGACY_TIMER_EXPIRED && legacy_still_running == t) {
		legacy_still_running = NULL;
		t->state = LEGACY_TIMER_IDLE;
		legacy_in_softirq_ctx = 1;
		t->callback(t, t->data);
		legacy_in_softirq_ctx = 0;
	}
	if (was_pending)
		t->state = LEGACY_TIMER_IDLE;
	return was_pending;
}

/* Advances the simulated jiffies clock by one tick and fires every armed
 * timer whose expiry has been reached, plus any callback that was left
 * queued by a bare del_timer(). */
static inline void timer_tick_emu(void)
{
	int i;
	legacy_global_tick++;
	if (legacy_still_running) {
		struct legacy_timer *orphan = legacy_still_running;
		legacy_still_running = NULL;
		orphan->state = LEGACY_TIMER_IDLE;
		legacy_in_softirq_ctx = 1;
		orphan->callback(orphan, orphan->data);
		legacy_in_softirq_ctx = 0;
	}
	for (i = 0; i < legacy_registry_count; i++) {
		struct legacy_timer *t = legacy_registry[i];
		if (t->state != LEGACY_TIMER_ARMED ||
		    t->expires_at > legacy_global_tick)
			continue;
		if (legacy_defer_fire) {
			t->state = LEGACY_TIMER_EXPIRED;
			legacy_still_running = t;
		} else {
			t->state = LEGACY_TIMER_IDLE;
			legacy_in_softirq_ctx = 1;
			t->callback(t, t->data);
			legacy_in_softirq_ctx = 0;
		}
	}
}

/* ------------------------- hrtimer emulation --------------------------- */

typedef enum {
	HRTIMER_MODE_ABS = 0,
	HRTIMER_MODE_REL = 1,
	HRTIMER_MODE_ABS_SOFT = 2,
	HRTIMER_MODE_REL_SOFT = 3,
} hrtimer_mode_t;

#define HRTIMER_NORESTART 0
#define HRTIMER_RESTART   1

typedef enum {
	HRTIMER_STATE_INACTIVE = 0,
	HRTIMER_STATE_QUEUED,
	HRTIMER_STATE_RUNNING,
} hrtimer_state_t;

struct hrtimer;

/* Callback contract mirrors the kernel: by default it runs in hardirq
 * context and must not sleep; the _SOFT modes run it in softirq context
 * (still no sleeping). The return value is HRTIMER_RESTART to re-arm the
 * timer or HRTIMER_NORESTART to stop. */
typedef int (*hrtimer_fn_t)(struct hrtimer *, void *);

struct hrtimer {
	hrtimer_state_t state;
	int64_t expires_ns;	/* absolute expiry, nanoseconds */
	hrtimer_mode_t mode;
	hrtimer_fn_t callback;
	void *data;
	int restart_ret;	/* last callback return value */
};

int64_t hrtimer_global_now_ns;	/* simulated monotonic clock */
int hrtimer_in_hardirq_ctx;	/* 1 while a hardirq-mode callback runs */
int hrtimer_in_softirq_ctx;	/* 1 while a _SOFT-mode callback runs */

#define HRTIMER_MAX_TIMERS 8
struct hrtimer *hrtimer_registry[HRTIMER_MAX_TIMERS];
int hrtimer_registry_count;

static inline int hrtimer_active_emu(const struct hrtimer *t)
{
	/* active = queued or currently running (not inactive) */
	return t->state != HRTIMER_STATE_INACTIVE;
}

static inline int hrtimer_is_queued_emu(const struct hrtimer *t)
{
	return t->state == HRTIMER_STATE_QUEUED;
}

/* hrtimer_start(): arms (or re-arms) the timer, replacing any pending
 * expiry. The real hrtimer_start() returns void; this emulation returns
 * whether the timer was active before the call (mirrors hrtimer_active()).
 * REL modes are converted to an absolute expiry at arming time, exactly
 * like the kernel converts to CLOCK_MONOTONIC nanoseconds. */
static inline int hrtimer_start_emu(struct hrtimer *t, int64_t expires,
				    hrtimer_mode_t mode,
				    hrtimer_fn_t callback, void *data)
{
	int was_active = hrtimer_active_emu(t);
	int i, found = 0;
	t->callback = callback;
	t->data = data;
	t->mode = mode;
	if (mode == HRTIMER_MODE_REL || mode == HRTIMER_MODE_REL_SOFT)
		t->expires_ns = hrtimer_global_now_ns + expires;
	else
		t->expires_ns = expires;
	t->state = HRTIMER_STATE_QUEUED;
	for (i = 0; i < hrtimer_registry_count; i++) {
		if (hrtimer_registry[i] == t) {
			found = 1;
			break;
		}
	}
	if (!found && hrtimer_registry_count < HRTIMER_MAX_TIMERS)
		hrtimer_registry[hrtimer_registry_count++] = t;
	return was_active;
}

/* Fires every queued timer whose expiry has been reached. A callback that
 * returns HRTIMER_RESTART is re-queued with the expiry the callback set;
 * HRTIMER_NORESTART leaves it inactive. The context flag is set from the
 * mode so examples can assert hardirq vs softirq context. */
static inline void hrtimer_run_emu(int64_t now_ns)
{
	int i;
	hrtimer_global_now_ns = now_ns;
	for (i = 0; i < hrtimer_registry_count; i++) {
		struct hrtimer *t = hrtimer_registry[i];
		int ret;
		if (t->state != HRTIMER_STATE_QUEUED ||
		    t->expires_ns > now_ns)
			continue;
		t->state = HRTIMER_STATE_RUNNING;
		hrtimer_in_hardirq_ctx =
			(t->mode != HRTIMER_MODE_ABS_SOFT &&
			 t->mode != HRTIMER_MODE_REL_SOFT);
		hrtimer_in_softirq_ctx = !hrtimer_in_hardirq_ctx;
		ret = t->callback(t, t->data);
		t->restart_ret = ret;
		hrtimer_in_hardirq_ctx = 0;
		hrtimer_in_softirq_ctx = 0;
		if (ret == HRTIMER_RESTART) {
			/* the callback must have advanced expires_ns */
			t->state = HRTIMER_STATE_QUEUED;
		} else {
			t->state = HRTIMER_STATE_INACTIVE;
		}
	}
}

/* Mirrors the kernel hrtimer_cancel(): 0 = the timer was inactive, 1 = it
 * was active and is now cancelled, -1 = its callback was running and the
 * caller had to wait. The RUNNING state is transient in this synchronous
 * harness, so -1 is unreachable in practice — but the real API returns it,
 * and hrtimer_cancel() from inside the callback would wait on itself. */
static inline int hrtimer_cancel_emu(struct hrtimer *t)
{
	if (t->state == HRTIMER_STATE_RUNNING)
		return -1;
	if (t->state == HRTIMER_STATE_QUEUED) {
		t->state = HRTIMER_STATE_INACTIVE;
		return 1;
	}
	return 0;
}

/* Mirrors CONFIG_DEBUG_ATOMIC_SLEEP: returns 1 if a "sleep" is attempted
 * from a timer callback context (hardirq or softirq — sleeping is forbidden
 * in both). */
static inline int context_forbids_sleep_emu(void)
{
	return hrtimer_in_hardirq_ctx || hrtimer_in_softirq_ctx ||
	       legacy_in_softirq_ctx;
}

#endif
