/*
 * stubs.h — self-contained host stubs for Linux workqueue-shaped code.
 * Models the workqueue flush/cancel contract with a deterministic,
 * single-threaded emulator so the semantics can be exercised with a plain
 * gcc build. No kernel headers required. Not kernel code.
 *
 * Modeled state machine (per work item):
 *   IDLE -> PENDING -> RUNNING -> IDLE
 * A work item is on at most one queue at a time; queue_work_emu returns
 * false if the item is already PENDING, and a RUNNING item cannot be queued
 * by another caller — only the item's own work function may re-queue it
 * (self-reschedule), which the kernel docs explicitly permit.
 *
 * mark_freed_emu() stands in for kfree(work_struct): it stamps the memory
 * with a WS_FREED sentinel and — like a real kfree — leaves a PENDING item
 * still linked in the queue. The runner then reaches the freed item and
 * records the emulated use-after-free in emulated_uaf_detected instead of
 * crashing. A freed item is never executed.
 */
#ifndef WORKQUEUE_FLUSH_STUBS_H
#define WORKQUEUE_FLUSH_STUBS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - (size_t)&((type *)0)->member))

typedef enum {
	WS_IDLE = 0,   /* not on any queue, not executing */
	WS_PENDING,    /* linked on a queue, awaiting execution */
	WS_RUNNING,    /* being executed right now */
	WS_FREED = 127 /* memory was freed; any later use is a use-after-free */
} work_state_t;

#define WS_IS_FREED(w) ((w)->state == (work_state_t)WS_FREED)

struct work_struct {
	work_state_t state;
	unsigned int runs; /* execution count (for assertions) */
	int rearmed;       /* internal: self-requeue happened during execution */
	struct work_struct *next;
	void (*func)(struct work_struct *work);
};

struct workqueue_struct {
	struct work_struct *head;
	struct work_struct *tail;
	int ran_count;             /* total items executed on this queue */
	struct work_struct *running_work; /* item being executed, if any */
};

int emulated_uaf_detected;

static struct workqueue_struct the_wq;

static inline void init_work_emu(struct work_struct *w,
				 void (*func)(struct work_struct *))
{
	w->state = WS_IDLE;
	w->runs = 0;
	w->rearmed = 0;
	w->next = NULL;
	w->func = func;
}

static inline bool queue_work_emu(struct workqueue_struct *wq,
				  struct work_struct *work)
{
	if (work->state == WS_PENDING)
		return false; /* already queued once: no second queue entry */
	if (work->state == WS_RUNNING) {
		/* A running item is not queueable by others. The one legal
		 * exception is the item re-queueing itself from its own work
		 * function (self-reschedule); the runner re-processes it. */
		if (work == wq->running_work) {
			work->rearmed = 1;
			work->next = NULL;
			if (wq->tail)
				wq->tail->next = work;
			else
				wq->head = work;
			wq->tail = work;
			return true;
		}
		return false;
	}
	if (WS_IS_FREED(work))
		return false; /* never queue freed memory */
	work->state = WS_PENDING;
	work->next = NULL;
	if (wq->tail)
		wq->tail->next = work;
	else
		wq->head = work;
	wq->tail = work;
	return true;
}

static inline bool dequeue_pending_emu(struct workqueue_struct *wq,
				       struct work_struct *work)
{
	struct work_struct *cur, *prev;

	prev = NULL;
	for (cur = wq->head; cur && cur != work; prev = cur, cur = cur->next)
		;
	if (!cur)
		return false; /* PENDING but not linked: nothing to remove */
	if (prev)
		prev->next = cur->next;
	else
		wq->head = cur->next;
	if (wq->tail == cur)
		wq->tail = prev;
	cur->next = NULL;
	work->state = WS_IDLE;
	return true;
}

static inline int run_pending_work_emu(struct workqueue_struct *wq,
				       int max_items)
{
	int ran = 0;

	while (max_items-- > 0) {
		struct work_struct *w = wq->head;
		if (!w)
			break;
		wq->head = w->next;
		if (wq->tail == w)
			wq->tail = NULL;
		w->next = NULL;
		if (WS_IS_FREED(w)) {
			/* The emulated UAF: a freed item was still pending on
			 * the queue and the worker reached it. In a real kernel
			 * this would execute a freed work function (KASAN:
			 * use-after-free). */
			emulated_uaf_detected = 1;
			continue;
		}
		w->state = WS_RUNNING;
		wq->running_work = w;
		w->rearmed = 0;
		if (w->func)
			w->func(w);
		wq->running_work = NULL;
		if (WS_IS_FREED(w)) {
			/* Work function freed its own work_struct. */
			emulated_uaf_detected = 1;
			break;
		}
		if (w->rearmed)
			w->state = WS_PENDING; /* self-rescheduled */
		else
			w->state = WS_IDLE;
		w->runs++;
		wq->ran_count++;
		ran++;
	}
	return ran;
}

/* flush_work: returns true if the item was pending/running (it waited for
 * completion), false if the item was already idle. The emulator is the only
 * executor, so "waiting" means draining the queue until the target is idle.
 * Like the kernel, no guarantee is offered if the item re-queues itself. */
static inline bool flush_work_emu(struct workqueue_struct *wq,
				  struct work_struct *work)
{
	int guard = 100000;

	if (work->state == WS_IDLE)
		return false;
	while (work->state != WS_IDLE && wq->head && guard-- > 0)
		run_pending_work_emu(wq, 1);
	return true;
}

/* cancel_work (async): removes a PENDING item only. A RUNNING item keeps
 * running; a re-queued item is back on the queue. No waiting, no guarantee. */
static inline bool cancel_work_emu(struct workqueue_struct *wq,
				   struct work_struct *work)
{
	if (work->state != WS_PENDING)
		return false;
	return dequeue_pending_emu(wq, work);
}

/* cancel_work_sync: removes a PENDING item, and in the kernel also waits for
 * a RUNNING instance to finish (and handles self-requeue). On return the item
 * is guaranteed not pending or executing — as long as nothing races a new
 * enqueue. In the single-threaded emulator the RUNNING case can only occur
 * when called from inside the work function itself, which is a kernel
 * deadlock; that path is reported as "no pending item" and not emulated. */
static inline bool cancel_work_sync_emu(struct workqueue_struct *wq,
					struct work_struct *work)
{
	if (work->state == WS_PENDING)
		return dequeue_pending_emu(wq, work);
	if (work->state == WS_RUNNING)
		return false; /* kernel: waits; emulator: deadlock, not modeled */
	return false; /* already idle */
}

/* Emulated kfree(work_struct): stamps freed memory but, exactly like a real
 * kfree, does NOT unlink the item from the queue. A PENDING item survives in
 * the queue and the runner will reach it (emulated use-after-free). */
static inline void mark_freed_emu(struct work_struct *work)
{
	work->state = (work_state_t)WS_FREED;
}

#endif
