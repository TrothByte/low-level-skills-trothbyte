/*
 * GOOD: linked readv + linked timeout (all-or-nothing chain semantics).
 *
 * TARGET-ONLY: NOT compiled on the Windows authoring host. Build on Linux
 * 5.1+ with liburing:
 *
 *   gcc -O2 -Wall -Wextra good_uring_linked_timeout.c \
 *       -o good_uring_linked_timeout -luring
 *   ./good_uring_linked_timeout
 *
 * Semantics modelled in ring_protocol.py scenario "linked timeout":
 *   - sqe->flags |= IOSQE_IO_LINK attaches the NEXT SQE as part of the chain
 *   - io_uring_prep_link_timeout() arms a timeout for the head request; if it
 *     fires, the whole chain is cancelled (head gets -ETIME / -ECANCELED
 *     depending on kernel version) and no independent completion occurs
 *   - every CQE res < 0 is -errno
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <liburing.h>

int main(void)
{
	struct io_uring ring;
	struct io_uring_sqe *sqe, *lt;
	struct io_uring_cqe *cqe;
	struct iovec iov;
	struct __kernel_timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	char buf[4096];
	int fd, ret;

	if (io_uring_queue_init(8, &ring, 0) != 0)
		return 1;

	fd = open("/dev/zero", O_RDONLY);
	if (fd < 0) {
		io_uring_queue_exit(&ring);
		return 1;
	}

	/* head request: never completes by itself -> the link timeout fires */
	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	sqe = io_uring_get_sqe(&ring);
	io_uring_prep_readv(sqe, fd, &iov, 1, 0);
	sqe->user_data = 0xA1;
	sqe->flags |= IOSQE_IO_LINK;

	/* linked timeout for the head request */
	lt = io_uring_get_sqe(&ring);
	io_uring_prep_link_timeout(lt, &ts, 0);
	lt->user_data = 0xA2;

	if (io_uring_submit(&ring) != 2) {
		io_uring_queue_exit(&ring);
		return 1;
	}

	/* both chain members complete: head cancelled, timeout -ETIME */
	for (int i = 0; i < 2; i++) {
		ret = io_uring_wait_cqe(&ring, &cqe);
		if (ret != 0) {
			io_uring_queue_exit(&ring);
			return 1;
		}
		if (cqe->res < 0)
			printf("user_data %llu: res %d (%s)\n",
			       (unsigned long long)cqe->user_data, cqe->res,
			       strerror(-cqe->res));
		io_uring_cqe_seen(&ring, cqe);
	}

	io_uring_queue_exit(&ring);
	close(fd);
	return 0;
}
