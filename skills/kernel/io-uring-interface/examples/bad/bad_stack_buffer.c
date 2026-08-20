/*
 * BAD: stack buffer handed to an async request (use-after-free).
 *
 * TARGET-ONLY: NOT compiled on the Windows authoring host. Shown here as a
 * bug class for reviewers; do not ship code like this.
 *
 * io_uring reads/writes the buffer asynchronously. The iovec and the stack
 * buffer die when submit_async_read() returns, but the kernel completes the
 * read after that: classic use-after-free (KASAN / KCSAN territory). The
 * buffer must stay alive until the matching CQE is consumed.
 *
 * Host model: examples/bad/ring_misuse.py
 *   "BUG reproduced: buffer ... freed/reused before the matching CQE"
 */
#include <fcntl.h>
#include <sys/uio.h>
#include <liburing.h>

static int submit_async_read(struct io_uring *ring, int fd)
{
	struct iovec iov;
	char stack_buf[4096]; /* BUG: dies when this frame returns */
	struct io_uring_sqe *sqe;

	sqe = io_uring_get_sqe(ring);
	if (!sqe)
		return -1;
	iov.iov_base = stack_buf;
	iov.iov_len = sizeof(stack_buf);
	io_uring_prep_readv(sqe, fd, &iov, 1, 0);
	/* BUG: completion lands after this frame is gone */
	return io_uring_submit(ring);
}

int main(void)
{
	struct io_uring ring;

	if (io_uring_queue_init(8, &ring, 0) != 0)
		return 1;
	submit_async_read(&ring, 3); /* fd 3 assumed open + readable */
	io_uring_queue_exit(&ring);
	return 0;
}
