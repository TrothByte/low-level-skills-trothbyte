/*
 * BAD: buffer reused before its matching CQE is consumed.
 *
 * TARGET-ONLY: NOT compiled on the Windows authoring host. Shown here as a
 * bug class for reviewers; do not ship code like this.
 *
 * A request owns its buffer from submit until its CQE is read (cqe_seen).
 * Writing the buffer again here races the kernel's readv; the result is
 * either corrupted data or the application reading data it just overwrote.
 *
 * Host model: examples/bad/ring_misuse.py
 *   "BUG reproduced: buffer ... freed/reused before the matching CQE"
 */
#include <string.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <liburing.h>

int main(void)
{
	struct io_uring ring;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	struct iovec iov;
	char *region;
	int fd = 3; /* assumed open + readable */

	if (io_uring_queue_init(8, &ring, 0) != 0)
		return 1;

	region = malloc(4096);
	if (!region)
		return 1;

	sqe = io_uring_get_sqe(&ring);
	iov.iov_base = region;
	iov.iov_len = 4096;
	io_uring_prep_readv(sqe, fd, &iov, 1, 0);
	io_uring_submit(&ring);

	/* BUG: kernel may still be writing into region */
	memset(region, 0, 4096);
	free(region);

	io_uring_wait_cqe(&ring, &cqe); /* CQE for a freed buffer */
	io_uring_cqe_seen(&ring, cqe);
	io_uring_queue_exit(&ring);
	return 0;
}
