/* GOOD: spin_lock_irqsave/irqrestore when the same lock is used in both
 * process and interrupt context — local irqs are disabled around the
 * critical section so an interrupt on this CPU cannot deadlock on the
 * already-held lock. */
#include "../stubs.h"
#include <stdio.h>

struct shared {
	spinlock_t lock;
	unsigned long counter;
};

static void process_context_good(struct shared *s, unsigned long delta)
{
	unsigned long flags;

	spin_lock_irqsave_emu(&s->lock, flags);
	s->counter += delta;
	spin_unlock_irqrestore_emu(&s->lock, flags);
}

static void irq_context_good(struct shared *s, unsigned long delta)
{
	unsigned long flags;

	/* irqs are already disabled here, but the save/restore pair is still
	 * required so the process-context critical section is interrupt-safe */
	spin_lock_irqsave_emu(&s->lock, flags);
	s->counter += delta;
	spin_unlock_irqrestore_emu(&s->lock, flags);
}

int main(void)
{
	struct shared s = {0};

	process_context_good(&s, 1);
	g_hardirq = 1;
	irq_context_good(&s, 2);
	g_hardirq = 0;
	process_context_good(&s, 4);

	if (g_sleep_in_atomic) {
		printf("FAIL: sleep detected\n");
		return 1;
	}
	if (s.counter != 7) {
		printf("FAIL: counter corruption\n");
		return 1;
	}
	printf("GOOD: irqsave/restore around a lock shared with interrupt context\n");
	return 0;
}
