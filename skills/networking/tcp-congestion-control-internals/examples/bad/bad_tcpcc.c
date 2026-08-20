/* BAD: three typical agent bugs in TCP congestion control code, each detected
 * at runtime against the correct RFC 5681 simulator in stubs.h:
 *  (1) congestion avoidance grows cwnd by 1 MSS per ACK instead of 1/cwnd,
 *      so the window runs far above the RFC 5681 bound;
 *  (2) an RTO timeout sets cwnd = ssthresh instead of collapsing to 1 MSS;
 *  (3) fast retransmit inflates the window but never halves ssthresh. */
#include "../stubs.h"
#include <stdio.h>

/* BUG 1: per-ACK +1 MSS growth in congestion avoidance (too aggressive). */
static void buggy_avoidance_ack(struct tcp_cc_state *s)
{
	s->snd_cwnd += CC_MSS;   /* wrong: avoidance adds 1/cwnd per ACK */
}

/* BUG 2: timeout leaves the window at ssthresh instead of 1 MSS. */
static void buggy_timeout(struct tcp_cc_state *s)
{
	s->snd_cwnd = s->ssthresh;   /* wrong: RFC 5681 collapses cwnd to 1 */
	s->mode = CC_SLOW_START;
}

/* BUG 3: fast retransmit inflates but never halves ssthresh. */
static void buggy_fast_retransmit(struct tcp_cc_state *s)
{
	s->snd_cwnd = s->ssthresh + 3;   /* wrong: ssthresh must drop to cwnd/2 */
	s->mode = CC_RECOVERY;
}

int main(void)
{
	struct tcp_cc_state good, bad;
	uint32_t i;

	/* 1. Congestion avoidance growth bound (RFC 5681: 1/cwnd per ACK).
	 *    Run the correct simulator and the buggy one on the same ACK
	 *    stream; the buggy window must end far above the RFC 5681 bound. */
	tcp_cc_init(&good, 1, 1);
	tcp_cc_init(&bad, 1, 1);
	tcp_ack_emu(&good, 1, 10, 0);
	tcp_ack_emu(&bad, 1, 10, 0);
	for (i = 0; i < 32; i++) {
		tcp_ack_emu(&good, 1, 10, 0); /* +1/cwnd per ACK */
		buggy_avoidance_ack(&bad);     /* +1 MSS per ACK */
	}
	if (bad.snd_cwnd > good.snd_cwnd + 1)
		printf("BUG reproduced: congestion avoidance grows window too fast\n");

	/* 2. RTO timeout must collapse the window to 1 MSS. */
	tcp_cc_init(&good, 8, 4);
	tcp_cc_init(&bad, 8, 4);
	tcp_on_loss_emu(&good, 0);             /* RFC 5681 timeout */
	buggy_timeout(&bad);
	if (bad.snd_cwnd != 1 && good.snd_cwnd == 1)
		printf("BUG reproduced: timeout did not collapse window to 1\n");

	/* 3. Fast retransmit must halve ssthresh. */
	tcp_cc_init(&good, 10, 100);
	tcp_cc_init(&bad, 10, 100);
	tcp_on_loss_emu(&good, 3);             /* RFC 5681 fast retransmit */
	buggy_fast_retransmit(&bad);
	if (bad.ssthresh == 100 && good.ssthresh == 5)
		printf("BUG reproduced: fast retransmit did not halve ssthresh\n");

	return 0;
}
