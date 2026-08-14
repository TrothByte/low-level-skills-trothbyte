/* GOOD: defer sleeping work from interrupt context to a workqueue.
 * The handler only records state and queues; the work function runs in
 * process context (kworker) where GFP_KERNEL may sleep. */
#include "../stubs.h"
#include <stdio.h>

struct job {
	unsigned long size;
	unsigned char *mem;
	work_struct_t work;
};

static void job_worker_good(void *data)
{
	struct job *j = (struct job *)data;

	/* process context: sleeping allocation is legal here */
	j->mem = kmalloc_emu(j->size, GFP_KERNEL);
}

/* atomic context: only record + defer; never allocate or sleep */
static void irq_schedule_job_good(struct job *j)
{
	j->size = 4096;
	queue_work_emu(&j->work);
}

int main(void)
{
	struct job j = {0};

	init_work_emu(&j.work, job_worker_good, &j);

	g_hardirq = 1;                 /* enter hardirq */
	irq_schedule_job_good(&j);     /* legal: only queues */
	g_hardirq = 0;                 /* exit hardirq */

	if (g_sleep_in_atomic) {
		printf("FAIL: sleep in atomic context\n");
		return 1;
	}

	flush_work_emu(&j.work);       /* kworker runs the job in process context */

	if (j.mem == NULL || j.work.ran != 1) {
		printf("FAIL: deferred job did not complete\n");
		return 1;
	}
	printf("GOOD: sleeping work deferred to workqueue process context\n");
	return 0;
}
