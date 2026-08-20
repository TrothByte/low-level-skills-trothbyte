/*
 * stubs.h — self-contained host stubs for an RFC 5681 TCP congestion control
 * simulator. Models snd_cwnd / ssthresh / inflight evolution over an emulated
 * ACK clock so RFC 5681 growth rules (slow start, congestion avoidance, fast
 * retransmit/recovery) and RTO estimation/backoff can be exercised with a
 * plain gcc build. No kernel headers, no pthread. Not kernel code.
 *
 * Units: snd_cwnd and ssthresh are in MSS (one MSS = 1). RTT and RTO share
 * one arbitrary time unit; RTT samples are supplied by the caller.
 */
#ifndef TCP_CC_STUBS_H
#define TCP_CC_STUBS_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define CC_MSS 1u

enum cc_mode {
	CC_SLOW_START = 0,
	CC_AVOIDANCE = 1,
	CC_RECOVERY = 2,
};

struct tcp_cc_state {
	uint32_t snd_cwnd;      /* congestion window, MSS */
	uint32_t ssthresh;      /* slow-start threshold, MSS */
	uint32_t inflight;      /* segments in flight */
	uint32_t dup_acks;      /* consecutive duplicate ACKs */
	uint32_t rtt_estimate;  /* last sampled RTT, time units */
	uint32_t rto;           /* retransmission timeout, time units */
	uint32_t srtt;          /* smoothed RTT */
	uint32_t rttvar;        /* RTT variation */
	uint32_t ack_accum;     /* integer 1/cwnd accumulator (avoidance) */
	int mode;
};

static inline void tcp_cc_init(struct tcp_cc_state *s, uint32_t initial_cwnd,
			       uint32_t initial_ssthresh)
{
	s->snd_cwnd = initial_cwnd;
	s->ssthresh = initial_ssthresh;
	s->inflight = 0;
	s->dup_acks = 0;
	s->rtt_estimate = 0;
	s->rto = 0;
	s->srtt = 0;
	s->rttvar = 0;
	s->ack_accum = 0;
	s->mode = CC_SLOW_START;
}

/* RFC 6298-style RTO estimator, described honestly: SRTT/RTTVAR gains of 1/8
 * and 1/4, RTO = SRTT + 4*RTTVAR, clamped to >= 1 MSS-unit of time. The first
 * sample seeds SRTT = R and RTTVAR = R/2. */
static inline void rto_update_emu(struct tcp_cc_state *s, uint32_t sample)
{
	if (s->srtt == 0) {
		s->srtt = sample;
		s->rttvar = sample / 2;
	} else {
		uint32_t delta = (s->srtt > sample) ? (s->srtt - sample)
						    : (sample - s->srtt);
		s->rttvar = (3 * s->rttvar + delta) / 4;
		s->srtt = (7 * s->srtt + sample) / 8;
	}
	s->rto = s->srtt + 4 * s->rttvar;
	if (s->rto < CC_MSS)
		s->rto = CC_MSS;
	s->rtt_estimate = sample;
}

/* Exponential backoff: an RTO timeout doubles the RTO (RFC 793 semantics kept
 * by RFC 5681), described honestly here as a plain doubling. */
static inline void rto_backoff_emu(struct tcp_cc_state *s)
{
	if (s->rto == 0)
		s->rto = 2 * CC_MSS;
	else
		s->rto *= 2;
}

/* ACK clock: process `acks` ACKs spaced one RTT apart, with an optional
 * `recovery_ack` flag for the ACK that covers the lost segment.
 *  - slow start:        +1 MSS per ACK (window roughly doubles per RTT);
 *  - congestion avoid:  +1/cwnd per ACK (window grows one MSS per RTT);
 *  - recovery (Reno):   dup ACK inflates by 1, recovery ACK deflates to
 *                       ssthresh and leaves recovery. */
static inline void tcp_ack_emu(struct tcp_cc_state *s, uint32_t acks,
			       uint32_t rtt, int recovery_ack)
{
	uint32_t i;
	for (i = 0; i < acks; i++) {
		rto_update_emu(s, rtt);
		if (s->mode == CC_RECOVERY) {
			if (recovery_ack) {
				s->snd_cwnd = s->ssthresh; /* deflate */
				s->mode = CC_AVOIDANCE;
				continue;
			}
			s->snd_cwnd += CC_MSS; /* inflate per dup ACK */
			continue;
		}
		if (s->mode == CC_SLOW_START) {
			s->snd_cwnd += CC_MSS; /* +1 MSS per ACK */
			if (s->snd_cwnd >= s->ssthresh)
				s->mode = CC_AVOIDANCE;
		} else {
			s->ack_accum += CC_MSS;
			if (s->ack_accum >= s->snd_cwnd) {
				s->ack_accum -= s->snd_cwnd;
				s->snd_cwnd += CC_MSS; /* +1 MSS per RTT */
			}
		}
	}
}

/* Loss handling:
 *  dup_acks >= 3: fast retransmit — ssthresh = max(cwnd/2, 2*MSS), window
 *                 inflated to ssthresh + dup_acks, fast recovery entered.
 *  otherwise:     RTO timeout — ssthresh = max(cwnd/2, 2*MSS), cwnd = 1 MSS,
 *                 RTO doubled (backoff), slow start re-entered. */
static inline void tcp_on_loss_emu(struct tcp_cc_state *s, uint32_t dup_acks)
{
	if (s->snd_cwnd / 2 < 2 * CC_MSS)
		s->ssthresh = 2 * CC_MSS;
	else
		s->ssthresh = s->snd_cwnd / 2;

	if (dup_acks >= 3) {
		s->dup_acks = dup_acks;
		s->snd_cwnd = s->ssthresh + dup_acks;
		s->mode = CC_RECOVERY;
	} else {
		s->dup_acks = 0;
		s->snd_cwnd = CC_MSS; /* collapse to 1 MSS */
		rto_backoff_emu(s);
		s->mode = CC_SLOW_START;
	}
}

/* Simplified RFC 8312 CUBIC window growth, per RFC 8312:
 *  - W(t) = beta * W_max + C*(t - t_cong)^3 with C ~ 0.4, anchored so the
 *    curve reaches W_max and flattens there;
 *  - t is time since the congestion event in RTT units (never wall clock);
 *  - a TCP-friendly region (beta*W_max + 1 MSS per RTT) is the lower bound;
 *  - the window never drops below one MSS.
 * Deterministic integer math; `beta` is scaled x10 (CUBIC default 7 = 0.7). */
static inline uint32_t cubic_growth_emu(struct tcp_cc_state *s,
					uint32_t t_since_loss, uint32_t w_max,
					uint32_t beta)
{
	uint32_t rtt = s->rtt_estimate ? s->rtt_estimate : 1;
	uint32_t t = t_since_loss / rtt;        /* elapsed RTTs */
	uint32_t base = (w_max * beta) / 10;    /* beta * W_max */
	uint32_t w;

	/* W(t) = base + 0.4 * t^3, capped at W_max (reached the plateau). */
	if (t * t * t >= (w_max - base) * 10 / 4) {
		w = w_max;
	} else {
		w = base + (4 * t * t * t) / 10;
		if (w > w_max)
			w = w_max;
	}

	/* TCP-friendly region: never grow slower than Reno's +1 MSS per RTT. */
	{
		uint32_t tcp_friendly = base + t;
		if (w < tcp_friendly)
			w = tcp_friendly;
	}
	if (w < CC_MSS)
		w = CC_MSS;                     /* cwnd floor of 1 MSS */
	return w;
}

#endif
