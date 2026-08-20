/*
 * BAD: SQE submitted with uninitialized fields.
 *
 * TARGET-ONLY: NOT compiled on the Windows authoring host. Shown here as a
 * bug class for reviewers; do not ship code like this. The kernel reads
 * whatever garbage occupies the untouched SQE fields (opcode/fd/off/addr/
 * len/rw_flags), typically failing with -EFAULT/-EINVAL or, worse, targeting
 * a random fd/offset.
 *
 * The host model catches this in examples/bad/ring_misuse.py:
 *   "BUG reproduced: SQE slot N submitted with uninitialized field(s)"
 */
#include <liburing.h>

int main(void)
{
	struct io_uring ring;
	struct io_uring_sqe *sqe;

	if (io_uring_queue_init(8, &ring, 0) != 0)
		return 1;

	sqe = io_uring_get_sqe(&ring);
	/* BUG: only opcode/user_data set; fd, off, addr, len, rw_flags are
	 * whatever was left in the reused ring slot */
	sqe->opcode = IORING_OP_READV;
	sqe->user_data = 1;
	io_uring_submit(&ring); /* kernel reads garbage fields */

	io_uring_queue_exit(&ring);
	return 0;
}
