/*
 * stubs.h — self-contained host stubs for Linux atomic-context-shaped code.
 * Models the preempt_count/context bits (hardirq, bottom-half disabled,
 * preemption disabled) so that sleep attempts in atomic context can be
 * detected deterministically with a plain gcc build. No kernel headers
 * required. Not kernel code.
 */
#ifndef KERNEL_ATOMIC_CONTEXT_STUBS_H
#define KERNEL_ATOMIC_CONTEXT_STUBS_H

#include <stddef.h>
#include <stdio.h>

#define __GFP_WAIT 0x10u
#define GFP_KERNEL __GFP_WAIT
#define GFP_ATOMIC 0x20u

#define ENOMEM 12
#define EINVAL 22

/* simulated context bits (mirror of preempt_count semantics) */
int g_hardirq;            /* 1 while inside an interrupt handler */
int g_atomic_depth;       /* >0 while preemption is disabled (spinlock held) */
int g_bh_disabled;        /* >0 while bottom halves are disabled */

/* recorded violations, read by the examples */
int g_sleep_in_atomic;    /* a sleeping function was called in atomic context */
int g_schedule_in_atomic; /* schedule() called in atomic context */

unsigned char stub_alloc_buf[64];

typedef struct {
	int held;
} spinlock_t;

typedef struct {
	int held;
} mutex_t;

typedef struct work_struct {
	void (*func)(void *data);
	void *data;
	int pending;
	int ran;
} work_struct_t;

static inline void spin_lock_emu(spinlock_t *l)
{
	l->held = 1;
	g_atomic_depth++;
}

static inline void spin_unlock_emu(spinlock_t *l)
{
	l->held = 0;
	g_atomic_depth--;
}

/* irqsave/irqrestore: capture the local interrupt state into a plain
 * unsigned long, exactly like the kernel macro interface. */
#define spin_lock_irqsave_emu(lock, flags)					\
	do {								\
		(flags) = (unsigned long)g_hardirq;			\
		g_hardirq = 1;						\
		(lock)->held = 1;					\
		g_atomic_depth++;					\
	} while (0)

#define spin_unlock_irqrestore_emu(lock, flags)				\
	do {								\
		(lock)->held = 0;					\
		g_atomic_depth--;					\
		g_hardirq = (int)(flags);				\
	} while (0)

static inline void spin_lock_bh_emu(spinlock_t *l)
{
	l->held = 1;
	g_atomic_depth++;
	g_bh_disabled++;
}

static inline void spin_unlock_bh_emu(spinlock_t *l)
{
	l->held = 0;
	g_atomic_depth--;
	g_bh_disabled--;
}

static inline int in_interrupt_emu(void)
{
	return g_hardirq || g_bh_disabled;
}

static inline int in_atomic_emu(void)
{
	return g_atomic_depth > 0;
}

/* kmalloc: GFP_KERNEL may sleep; the stub records the attempt and fails. */
static inline void *kmalloc_emu(size_t size, unsigned int gfp)
{
	(void)size;
	if ((g_atomic_depth > 0 || g_hardirq) && (gfp & __GFP_WAIT)) {
		g_sleep_in_atomic = 1;
		return NULL;
	}
	return stub_alloc_buf;
}

static inline void kfree_emu(void *p)
{
	(void)p;
}

/* mutex: may sleep; recorded as a violation when taken in atomic context. */
static inline void mutex_lock_emu(mutex_t *m)
{
	if (g_hardirq || g_atomic_depth > 0) {
		g_sleep_in_atomic = 1;
		return;
	}
	m->held = 1;
}

static inline void mutex_unlock_emu(mutex_t *m)
{
	m->held = 0;
}

/* schedule: forbidden whenever preemption is disabled. */
static inline void schedule_emu(void)
{
	if (g_hardirq || g_atomic_depth > 0)
		g_schedule_in_atomic = 1;
}

/* workqueue: handlers run in process context (kworker) — sleeping legal. */
static inline void init_work_emu(work_struct_t *w, void (*fn)(void *),
				 void *data)
{
	w->func = fn;
	w->data = data;
	w->pending = 0;
	w->ran = 0;
}

static inline int queue_work_emu(work_struct_t *w)
{
	w->pending = 1;
	return 1;
}

static inline void flush_work_emu(work_struct_t *w)
{
	if (w->pending) {
		w->pending = 0;
		w->ran = 1;
		w->func(w->data);
	}
}

#endif
