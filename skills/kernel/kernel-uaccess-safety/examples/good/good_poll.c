/* GOOD: poll registers the wait queue; fasync is stored and fired. */
#include "../stubs.h"
#include <assert.h>
#include <string.h>

struct dev_state {
	struct wait_queue_head rq;
	int fasync_on;
	int data_ready;
};

static unsigned int dev_poll_good(struct file *filp, poll_table *pt)
{
	struct dev_state *ds = (struct dev_state *)filp->private_data;
	unsigned int mask = 0;

	poll_wait_emu(filp, &ds->rq, pt);
	if (ds->data_ready)
		mask |= POLLIN;
	return mask;
}

/* called from the VFS F_SETFL / O_ASYNC path */
static int dev_fasync_good(int fd, struct file *filp, int on)
{
	struct dev_state *ds = (struct dev_state *)filp->private_data;
	return fasync_helper_emu(fd, filp, on, &ds->fasync_on);
}

static void dev_data_push(struct dev_state *ds)
{
	ds->data_ready = 1;
	kill_fasync_emu(&ds->fasync_on, SIGIO, POLL_IN);
}

int main(void)
{
	struct dev_state ds;
	struct file f;

	memset(&ds, 0, sizeof ds);
	memset(&f, 0, sizeof f);
	f.private_data = &ds;

	assert(dev_fasync_good(0, &f, 1) == 0);
	assert(ds.fasync_on == 1);
	assert(dev_poll_good(&f, NULL) == 0);

	dev_data_push(&ds);
	assert(dev_poll_good(&f, NULL) == POLLIN);
	assert(fasync_signalled == 1);

	dev_fasync_good(0, &f, 0);
	assert(ds.fasync_on == 0);
	return 0;
}
