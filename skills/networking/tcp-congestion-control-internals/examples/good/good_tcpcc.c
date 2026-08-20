/* GOOD: RFC 5681 congestion control state machine — slow start, congestion
 * avoidance, ssthresh switch, fast retransmit/recovery, RTO handling with
 * backoff, and a CUBIC growth function — all validated against the RFC 5681
 * growth rules with assert(). */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	struct tcp_cc_state s;
	uint32_t start_cwnd, rto_before;

	/* 1. Slow start: +1 MSS per ACK -> cwnd doubles per RTT. */
	tcp_cc_init(&s, 1, 100);
	assert(s.mode == CC_SLOW_START);
	tcp_ack_emu(&s, 1, 10, 0);          /* one ACK: 1 -> 2 */
	assert(s.snd_cwnd == 2);
	tcp_ack_emu(&s, 2, 10, 0);          /* one RTT of ACKs: 2 -> 4 */
	assert(s.snd_cwnd == 4);
	assert(s.mode == CC_SLOW_START);

	/* 2. ssthresh is the switch point: crossing it ends slow start. */
	tcp_cc_init(&s, 1, 3);
	tcp_ack_emu(&s, 2, 10, 0);          /* one RTT of ACKs: 1 -> 3 */
	assert(s.snd_cwnd == 3);
	assert(s.mode == CC_AVOIDANCE);     /* cwnd reached ssthresh */
	tcp_ack_emu(&s, 1, 10, 0);          /* 1 ACK in avoidance: no growth */
	assert(s.snd_cwnd == 3);
	tcp_ack_emu(&s, 3, 10, 0);          /* full RTT at cwnd 3: +1 MSS */
	assert(s.snd_cwnd == 4);

	/* 3. Congestion avoidance: +1/cwnd per ACK -> +1 MSS per RTT. */
	tcp_cc_init(&s, 1, 1);
	tcp_ack_emu(&s, 1, 10, 0);          /* cross into avoidance */
	assert(s.mode == CC_AVOIDANCE);
	start_cwnd = s.snd_cwnd;            /* 2 */
	tcp_ack_emu(&s, start_cwnd, 10, 0); /* one full RTT of ACKs */
	assert(s.snd_cwnd == start_cwnd + 1); /* +1 MSS per RTT, not per ACK */

	/* 4. RTO timeout: cwnd collapses to 1 MSS, ssthresh = max(cwnd/2, 2),
	 *    and the RTO doubles (backoff). */
	tcp_cc_init(&s, 10, 100);
	rto_update_emu(&s, 10);             /* seed the RTO estimator */
	rto_update_emu(&s, 10);
	rto_before = s.rto;
	tcp_on_loss_emu(&s, 0);             /* timeout */
	assert(s.snd_cwnd == 1);
	assert(s.ssthresh == 5);            /* 10 / 2 */
	assert(s.mode == CC_SLOW_START);
	assert(s.rto >= 2 * rto_before);    /* RTO doubling after a timeout */

	/* 5. Fast retransmit: ssthresh = cwnd/2, window inflated to
	 *    ssthresh + dup_acks, fast recovery entered. */
	tcp_cc_init(&s, 10, 100);
	tcp_on_loss_emu(&s, 3);             /* three duplicate ACKs */
	assert(s.ssthresh == 5);            /* 10 / 2 */
	assert(s.snd_cwnd == 8);            /* ssthresh + 3 dup ACKs (inflate) */
	assert(s.mode == CC_RECOVERY);

	/* 6. Fast recovery: dup ACKs inflate further, the recovery ACK
	 *    deflates back to ssthresh. */
	tcp_ack_emu(&s, 1, 10, 0);          /* another dup ACK */
	assert(s.snd_cwnd == 9);
	tcp_ack_emu(&s, 1, 10, 1);          /* ACK covering the loss */
	assert(s.snd_cwnd == s.ssthresh);   /* deflate back to ssthresh */
	assert(s.mode == CC_AVOIDANCE);

	/* 7. RTO estimator: bounded SRTT/RTTVAR -> RTO, seeded and stable. */
	tcp_cc_init(&s, 1, 100);
	{
		uint32_t i;
		for (i = 0; i < 8; i++)
			tcp_ack_emu(&s, 1, 10, 0);
	}
	assert(s.rto > 0);
	assert(s.rto >= s.srtt);            /* RTO = SRTT + 4*RTTVAR >= SRTT */

	/* 8. CUBIC: growth depends on time since congestion (never wall
	 *    clock), monotone in t, floored at beta*W_max, capped at W_max,
	 *    and never below one MSS. */
	{
		uint32_t w0 = cubic_growth_emu(&s, 0, 100, 7);    /* beta 0.7 */
		uint32_t w1 = cubic_growth_emu(&s, 100, 100, 7);
		uint32_t w2 = cubic_growth_emu(&s, 200, 100, 7);
		assert(w0 == 70);               /* beta * W_max at t = 0 */
		assert(w1 >= w0);
		assert(w2 >= w1);
		assert(w2 <= 100);              /* cubic cap: W_max */
		assert(w0 >= 1);                /* cwnd floor of 1 MSS */
	}

	printf("ALL CHECKS PASSED\n");
	return 0;
}
