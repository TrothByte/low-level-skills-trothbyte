/*
 * BAD: ring index computed with the wrong mask.
 *
 * TARGET-ONLY: NOT compiled on the Windows authoring host. Shown here as a
 * bug class for reviewers; do not ship code like this.
 *
 * Ring indices must be `tail & (entries - 1)` (rings are power-of-two in the
 * number of ENTRIES, not bytes). Masking with the ring's byte size
 * (entries * sizeof(struct io_uring_sqe)) wraps at the wrong boundary and
 * indexes out of the ring. The bug is latent in userspace-only tests (the
 * write "happens to work" at small tails) and only misbehaves under real
 * wrap-around.
 *
 * Host model: examples/bad/ring_misuse.py
 *   "BUG reproduced: ring index N computed with byte-size mask"
 */
#include <stddef.h>
#include <liburing.h>

/* BUG: byte-size mask instead of entry mask */
static struct io_uring_sqe *bad_sqe_at(struct io_uring_sqe *sqes,
				       unsigned sq_entries, unsigned tail)
{
	unsigned ring_bytes = sq_entries * sizeof(struct io_uring_sqe);
	unsigned idx = tail & (ring_bytes - 1); /* WRONG boundary */
	return &sqes[idx];
}

int main(void)
{
	struct io_uring ring;
	struct io_uring_sqe *sqe;

	if (io_uring_queue_init(16, &ring, 0) != 0)
		return 1;

	/* tail 16 wraps to 16 with the byte mask (16 * 64 = 1024 -> mask 1023)
	 * but 16 is out of bounds for a 16-entry ring -> OOB access */
	sqe = bad_sqe_at(ring.sq.sqes, 16, 16);
	(void)sqe;

	io_uring_queue_exit(&ring);
	return 0;
}
