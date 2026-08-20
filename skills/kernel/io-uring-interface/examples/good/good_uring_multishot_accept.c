/*
 * GOOD: multishot accept with a provided buffer ring (IORING_REGISTER_PBUF_RING).
 *
 * TARGET-ONLY: NOT compiled on the Windows authoring host. Requires Linux
 * 5.19+ (provided buffer rings) and liburing >= 0.7. Build and run on target:
 *
 *   gcc -O2 -Wall -Wextra good_uring_multishot_accept.c \
 *       -o good_uring_multishot_accept -luring
 *   ./good_uring_multishot_accept
 *
 * Semantics modelled in ring_protocol.py scenario "multishot accept":
 *   - io_uring_register_buf_ring()/io_uring_setup_buf_ring() register a
 *     ring of provided buffers under a buf_group id
 *   - io_uring_prep_multishot_accept() re-arms itself: every accepted
 *     connection consumes the next provided buffer and posts a CQE with
 *     IORING_CQE_F_MORE
 *   - when the provided buffer ring empties, the request stays armed and
 *     resumes producing CQEs once the user replenishes buffers with
 *     io_uring_buf_ring_add() + io_uring_buf_ring_advance()
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <liburing.h>

#define BUF_NR 4
#define BUF_SZ 4096

int main(void)
{
	struct io_uring ring;
	struct io_uring_buf_ring *br;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	char *bufs;
	int fd, ret, i, done = 0;

	if (io_uring_queue_init(64, &ring, 0) != 0)
		return 1;

	/* register the provided buffer ring under buf_group 0 */
	bufs = malloc(BUF_NR * BUF_SZ);
	if (!bufs) {
		io_uring_queue_exit(&ring);
		return 1;
	}
	br = io_uring_setup_buf_ring(&ring, BUF_NR, 0, 0, &ret);
	if (!br) {
		free(bufs);
		io_uring_queue_exit(&ring);
		return 1;
	}
	for (i = 0; i < BUF_NR; i++)
		io_uring_buf_ring_add(br, bufs + i * BUF_SZ, BUF_SZ, i,
				      BUF_NR - 1);
	io_uring_buf_ring_advance(br, BUF_NR);

	fd = 3; /* listening socket passed on stdin/argv in a real server */

	sqe = io_uring_get_sqe(&ring);
	io_uring_prep_multishot_accept(sqe, fd, NULL, NULL, 0);
	sqe->buf_group = 0;
	sqe->user_data = 0xB1;

	if (io_uring_submit(&ring) != 1) {
		free(bufs);
		io_uring_queue_exit(&ring);
		return 1;
	}

	/* drain completions; every cqe->flags has IORING_CQE_F_MORE while the
	 * multishot request stays armed. Replenish the buffer ring whenever
	 * CQEs arrive so the request keeps producing. */
	while (done < 8) {
		ret = io_uring_wait_cqe(&ring, &cqe);
		if (ret != 0)
			break;
		if (cqe->res < 0) {
			io_uring_cqe_seen(&ring, cqe);
			break;
		}
		io_uring_cqe_seen(&ring, cqe);
		io_uring_buf_ring_add(br, bufs + (done % BUF_NR) * BUF_SZ,
				      BUF_SZ, done % BUF_NR, BUF_NR - 1);
		io_uring_buf_ring_advance(br, 1);
		done++;
	}

	io_uring_queue_exit(&ring);
	free(bufs);
	return 0;
}
