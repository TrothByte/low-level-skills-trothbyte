/*
 * stubs.h — self-contained host stubs for Linux kthread-shaped code.
 *
 * Simulates the kthread lifecycle (create, run, should_stop, stop) as a
 * deterministic single-threaded state machine, so the create/teardown
 * ordering rules can be exercised with a plain gcc build. No kernel
 * headers, no pthreads.
 *
 * Model:
 *   - kthread_create_emu()  allocates a ktask in state KT_ST_CREATED. The
 *     threadfn is NOT executed (mirrors kthread_create not starting the
 *     thread).
 *   - kthread_run_emu()     is create + wake: it marks the task running and
 *     advances it one step (mirrors kthread_run = create + wake_up_process).
 *   - run_thread_step_emu() advances the simulated thread by one loop
 *     iteration. The threadfn is a STEP BODY: each call handles one
 *     iteration, polls kthread_should_stop_emu(), and returns KT_RUNNING to
 *     keep polling, or any other int to exit with that value (that value is
 *     what kthread_stop_emu() returns — mirroring the kernel contract).
 *   - kthread_should_stop_emu() reads the stop flag of the currently
 *     executing simulated task (the real kernel kthread_should_stop() reads
 *     `current`, so it takes no argument).
 *   - kthread_stop_emu()     enforces the contract: it must only be called
 *     on a live task (BUG print otherwise), sets the stop flag, runs the
 *     thread to exit, and returns the threadfn's return value.
 *   - kthread_data_free_emu() is the freed-data sentinel: freeing a
 *     resource while the thread is still alive is detected and reported as
 *     the stop-before-free order inversion.
 *
 * Harness constraint (documented): threadfn returns KT_RUNNING (0) to
 * continue, so a threadfn result of exactly 0 cannot be expressed while the
 * thread is still polling. Kernels accept any int; the step scheduler needs
 * one reserved value, so 0 is it.
 */
#ifndef KERNEL_KTHREAD_STUBS_H
#define KERNEL_KTHREAD_STUBS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KT_RUNNING 0 /* threadfn return: keep polling next step */

enum ktask_state {
	KT_ST_CREATED,
	KT_ST_RUNNING,
	KT_ST_EXITED
};

typedef int (*kthread_fn_t)(void *data);

struct ktask {
	int alive;        /* 1 = task object still usable, 0 = exited */
	int stop_flag;    /* set by kthread_stop_emu(); polled by threadfn */
	int state;        /* enum ktask_state */
	int data_freed;   /* sentinel: resources freed while thread alive */
	int result;       /* threadfn return value (= kthread_stop return) */
	int steps;        /* diagnostics: steps executed */
	kthread_fn_t fn;
	void *data;
	char name[32];
};

static struct ktask *current_ktask; /* "current" of the simulated thread */

static inline struct ktask *kthread_create_emu(kthread_fn_t fn, void *data,
					       const char *name)
{
	struct ktask *t = (struct ktask *)calloc(1, sizeof(*t));

	t->fn = fn;
	t->data = data;
	t->state = KT_ST_CREATED;
	t->alive = 1;
	strncpy(t->name, name, sizeof(t->name) - 1);
	t->name[sizeof(t->name) - 1] = '\0';
	return t;
}

static inline int kthread_should_stop_emu(void)
{
	return current_ktask ? current_ktask->stop_flag : 0;
}

/* Advance the simulated thread by one loop iteration. When the threadfn
 * returns KT_RUNNING and no stop was requested, the thread keeps running.
 * Any other outcome (explicit result, or stop requested while the threadfn
 * returned) moves the thread to KT_ST_EXITED. */
static inline int run_thread_step_emu(struct ktask *t)
{
	int rc;

	if (t->state == KT_ST_EXITED)
		return t->result;
	current_ktask = t;
	t->state = KT_ST_RUNNING;
	rc = t->fn(t->data);
	t->steps++;
	if (rc == KT_RUNNING && !t->stop_flag)
		return KT_RUNNING;
	t->result = rc;
	t->state = KT_ST_EXITED;
	t->alive = 0;
	current_ktask = NULL;
	return t->result;
}

static inline struct ktask *kthread_run_emu(kthread_fn_t fn, void *data,
					    const char *name)
{
	struct ktask *t = kthread_create_emu(fn, data, name);

	t->state = KT_ST_RUNNING;
	run_thread_step_emu(t); /* wake_up_process: first iteration runs */
	return t;
}

/* Freed-data sentinel: frees a resource owned by the (possibly still
 * running) thread. If the thread is alive, this is the stop-before-free
 * order inversion and it is reported. */
static inline void kthread_data_free_emu(struct ktask *t, void *p)
{
	if (t->state != KT_ST_EXITED) {
		printf("BUG reproduced: resources freed before kthread_stop\n");
		t->data_freed = 1;
	}
	free(p);
}

/* Contract enforcement + shutdown: only valid on a live task; sets the stop
 * flag, runs the thread to exit, returns the threadfn's return value. */
static inline int kthread_stop_emu(struct ktask *t)
{
	if (t->state == KT_ST_EXITED) {
		printf("BUG reproduced: kthread_stop on exited task\n");
		return 0;
	}
	t->stop_flag = 1;
	if (t->data_freed) {
		/* data already gone; the thread cannot safely execute */
		t->state = KT_ST_EXITED;
		t->alive = 0;
		current_ktask = NULL;
		return 0;
	}
	while (t->state != KT_ST_EXITED)
		run_thread_step_emu(t);
	return t->result;
}

#endif
