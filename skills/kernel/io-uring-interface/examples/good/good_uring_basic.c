/*
 * GOOD: minimal io_uring lifecycle with liburing (NOP).
 *
 * TARGET-ONLY: this file is NOT compiled on the Windows authoring host.
 * It documents the real liburing API for the ring protocol modelled in
 * ring_protocol.py. Build and run on Linux 5.1+ with liburing:
 *
 *   gcc -O2 -Wall -Wextra good_uring_basic.c -o good_uring_basic -luring
 *   ./good_uring_basic
 *
 * Protocol steps shown: get_sqe (private tail advance), prep (all SQE
 * fields), submit (release store on sq_tail + io_uring_enter), wait_cqe
 * (acquire load on cq_tail), cqe_seen (release store on cq_head).
 */
#include <liburing.h>

int main(void)
{
	struct io_uring ring;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;

	if (io_uring_queue_init(8, &ring, 0) != 0)
		return 1;

	/* 1. fetch the next free SQE (index = tail & mask, advanced per fetch) */
	sqe = io_uring_get_sqe(&ring);
	if (!sqe) {
		io_uring_queue_exit(&ring);
		return 1;
	}

	/* 2. fully initialize every SQE field */
	io_uring_prep_nop(sqe);
	sqe->user_data = 0x10;

	/* 3. commit sq_tail (release store inside liburing) + io_uring_enter */
	if (io_uring_submit(&ring) != 1) {
		io_uring_queue_exit(&ring);
		return 1;
	}

	/* 4. acquire cq_tail; a completion is pending */
	if (io_uring_wait_cqe(&ring, &cqe) != 0) {
		io_uring_queue_exit(&ring);
		return 1;
	}

	/* CQE: res >= 0 result, res < 0 is -errno */
	if (cqe->res != 0) {
		io_uring_cqe_seen(&ring, cqe);
		io_uring_queue_exit(&ring);
		return 1;
	}

	/* 5. re-commit cq_head with a release store once consumed */
	io_uring_cqe_seen(&ring, cqe);

	io_uring_queue_exit(&ring);
	return 0;
}
