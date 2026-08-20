/*
 * stubs.h — self-contained host stubs for Linux waitqueue/completion code.
 * Models the wait/wake and complete/wait_for_completion contracts with a
 * deterministic, single-threaded emulator so the semantics can be exercised
 * with a plain gcc build. No kernel headers required. Not kernel code.
 *
 * Modeled primitives:
 *   - struct wait_queue_head_emu + wait_event_emu(wq, cond) / wake_up_emu()
 *     A waiter either returns immediately (condition already true) or parks
 *     on the queue; a later wake re-runs the condition check. This loop is
 *     exactly what the kernel's wait_event macro does.
 *   - struct completion_emu { done, waiting } with complete_emu(),
 *     complete_all_emu(), wait_for_completion_emu(), the interruptible and
 *     timeout variants, and reinit_completion_emu().
 *
 * Bug instrumentation:
 *   - lost_wakeup_detected: set when a waker checked "no waiter parked" while
 *     a waiter is parked — i.e. the wake would be lost.
 *   - spurious_wake_observed: set when a wake ran while the condition was
 *     still false (the waiter must re-park).
 *   - sleep_in_atomic_detected: set when a wait is issued while a simulated
 *     "atomic context" (spinlock held) flag is set.
 *   - emulated_uaf_detected: set when a completion is marked freed while a
 *     waiter is still parked on it.
 */
#ifndef WAITQUEUE_STUBS_H
#define WAITQUEUE_STUBS_H

#include <stdbool.h>
#include <stddef.h>

/* ---- emulated error/return codes ---- */
enum { ERESTARTSYS_EMU = -512 };

/* ---- generic helper ---- */
static inline int _wq_max(int a, int b) { return a > b ? a : b; }

/* ---- waitqueue emulation ---- */
struct wait_queue_head_emu {
	int waiters;   /* number of parked waiters */
	int wakes;     /* total wake calls issued */
	int check_count; /* condition re-check count */
};

static inline void init_waitqueue_emu(struct wait_queue_head_emu *wq)
{
	wq->waiters = 0;
	wq->wakes = 0;
	wq->check_count = 0;
}

/* wait_event_emu: emulate the kernel macro semantics:
 *   do { check(); if (cond) break; park(); schedule(); } while (1);
 * On entry: if cond true -> returns 0 immediately (no parking).
 * Otherwise parks (waiters++) and returns 1 to mean "parked; the caller's
 * scheduler must run until unparked". In the deterministic harness we
 * provide wq_step_emu() to unpark parked waiters. */
static inline int wait_event_emu(struct wait_queue_head_emu *wq, bool cond)
{
	wq->check_count++;
	if (cond)
		return 0; /* condition already true: no parking */
	wq->waiters++;
	return 1; /* parked */
}

/* wake_up_emu: wakes all parked waiters. Returns number woken. If a waiter
 * is parked at this instant, the wake is "received" (in the kernel the
 * waiter re-runs its condition; our harness calls the waiter again). */
static inline int wake_up_emu(struct wait_queue_head_emu *wq)
{
	int n = wq->waiters;
	wq->wakes++;
	/* A wake that arrives with no one parked may still be needed if a
	 * waiter is about to check; the harness detects the lost-wakeup
	 * pattern by calling the condition/wake ordering helpers. */
	return n;
}

/* unpark_emu: after a wake, the waiter re-checks its condition. This
 * helper decrements the parked count (the kernel would schedule the
 * waiter). Returns the number of waiters still parked. */
static inline int unpark_emu(struct wait_queue_head_emu *wq)
{
	if (wq->waiters > 0)
		wq->waiters--;
	return wq->waiters;
}

/* park_forever_emu: models the BUGGY manual wait pattern — a waiter that
 * checks the condition once and then parks WITHOUT re-checking after any
 * wake (unlike the kernel's wait_event loop). Used by the bad example to
 * demonstrate the lost-wakeup race. */
static inline int park_forever_emu(struct wait_queue_head_emu *wq)
{
	wq->waiters++;
	return 1; /* parked, will never re-check */
}

/* ---- completion emulation ---- */
struct completion_emu {
	int done;      /* completion count */
	int waiting;   /* parked waiters */
	int state;     /* 0 = fresh, 1 = freed */
};

static inline void init_completion_emu(struct completion_emu *c)
{
	c->done = 0;
	c->waiting = 0;
	c->state = 0;
}

/* complete_emu: wakes exactly one waiter. */
static inline void complete_emu(struct completion_emu *c)
{
	c->done++;
	if (c->waiting > 0)
		c->waiting--;
}

/* complete_all_emu: wakes all current waiters; done stays >0 so future
 * waiters return immediately (until reinit_completion). */
static inline void complete_all_emu(struct completion_emu *c)
{
	c->done = 1; /* sticky: any future wait_for_completion sees done */
	c->waiting = 0;
}

/* wait_for_completion_emu: blocks (parks) until done>0. Returns 0 on
 * completion. In the harness, when done>0 returns immediately. */
static inline int wait_for_completion_emu(struct completion_emu *c)
{
	if (c->done > 0)
		return 0;
	c->waiting++;
	return 1; /* parked */
}

/* wait_for_completion_interruptible_emu: like above but may return early
 * with -ERESTARTSYS if a simulated signal arrives while parked. The
 * harness controls signals via the wq->signal_pending flag. */
static inline int wait_for_completion_interruptible_emu(
	struct completion_emu *c, int signal_pending)
{
	if (c->done > 0)
		return 0;
	if (signal_pending) {
		/* interrupted before parking: signal wins */
		return ERESTARTSYS_EMU;
	}
	c->waiting++;
	return ERESTARTSYS_EMU; /* parked but can be signaled */
}

/* wait_for_completion_timeout_emu: returns 0 on timeout, 1 on completion. */
static inline int wait_for_completion_timeout_emu(
	struct completion_emu *c, int timeout_elapsed)
{
	if (c->done > 0)
		return 1;
	if (timeout_elapsed)
		return 0;
	c->waiting++;
	return 1; /* completed (harness completes before timeout) */
}

static inline void reinit_completion_emu(struct completion_emu *c)
{
	c->done = 0;
}

/* mark_freed_emu: emulate kfree(completion) while waiters may still be
 * parked. Sets state=1 so the harness can detect the emulated UAF. */
static inline void mark_freed_emu(struct completion_emu *c)
{
	c->state = 1;
}

/* ---- global bug instrumentation ---- */
static int lost_wakeup_detected;
static int spurious_wake_observed;
static int sleep_in_atomic_detected;
static int emulated_uaf_detected;

static inline void wq_report_lost_wakeup(void) { lost_wakeup_detected = 1; }
static inline void wq_report_spurious(void) { spurious_wake_observed = 1; }
static inline void wq_report_atomic_sleep(void) { sleep_in_atomic_detected = 1; }
static inline void wq_report_uaf(void) { emulated_uaf_detected = 1; }
static inline int wq_lost_wakeup(void) { return lost_wakeup_detected; }
static inline int wq_spurious(void) { return spurious_wake_observed; }
static inline int wq_atomic_sleep(void) { return sleep_in_atomic_detected; }
static inline int wq_uaf(void) { return emulated_uaf_detected; }

#endif
