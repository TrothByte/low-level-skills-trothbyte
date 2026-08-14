/* BAD: mutex_lock() called from interrupt context — it may sleep. */
#include "../stubs.h"
#include <stdio.h>

static mutex_t config_lock;
static unsigned long config_live;

/* BAD: interrupt context cannot sleep; mutex_lock may sleep even when the
 * mutex is immediately available. */
static void dev_irq_handler_bad(void)
{
	mutex_lock_emu(&config_lock);
	config_live = 1;
	mutex_unlock_emu(&config_lock);
}

int main(void)
{
	g_hardirq = 1;            /* simulate interrupt entry on this CPU */
	dev_irq_handler_bad();
	g_hardirq = 0;
	if (g_sleep_in_atomic)
		printf("BUG reproduced: mutex_lock() in interrupt context\n");
	return 0;
}
