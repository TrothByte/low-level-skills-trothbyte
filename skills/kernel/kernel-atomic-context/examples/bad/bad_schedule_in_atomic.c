/* BAD: schedule() while holding a spinlock — preemption disabled. */
#include "../stubs.h"
#include <stdio.h>

struct ring {
	spinlock_t lock;
	int tail;
};

/* BAD: schedule() needs to preempt and takes runqueue locks; with a
 * spinlock held it deadlocks or triggers the scheduling-while-atomic splat. */
static void ring_drain_bad(struct ring *r)
{
	spin_lock_emu(&r->lock);
	while (r->tail > 0) {
		r->tail--;
		schedule_emu();
	}
	spin_unlock_emu(&r->lock);
}

int main(void)
{
	struct ring r = {0};
	r.tail = 3;
	ring_drain_bad(&r);
	if (g_schedule_in_atomic)
		printf("BUG reproduced: schedule() in atomic context\n");
	return 0;
}
