# Linux TCP Congestion Control Rules

Source-backed rule set for the RFC 5681 congestion control state machine and
the Linux `tcp_congestion_ops` interface. Each entry: RULE -> WHY AI GETS IT
WRONG -> CORRECT REASONING -> EXAMPLE -> COUNTEREXAMPLE -> VERIFICATION ->
SOURCE. Confidence markers: KNOWN (documented contract), INFERRED (derived),
UNVERIFIED (never use in a stable skill).

## 1. Slow start grows one MSS per ACK (doubling per RTT)

- **RULE**: in slow start, each cumulative ACK increases `snd_cwnd` by at
  most one MSS. A full window in flight yields roughly one doubling per RTT.
  The ACK clock — not a timer — drives the growth.
- **WHY AI GETS IT WRONG**: agents write `cwnd += cwnd` on a timer tick, or
  grow by one MSS per RTT instead of per ACK.
- **CORRECT REASONING**: RFC 5681 section 3.1: for each ACK in slow start,
  `cwnd += SMSS` (if the cwnd is not limited by the receiver window). With
  `cwnd` segments in flight, one RTT delivers ~`cwnd` ACKs, so the window
  doubles each RTT.
- **EXAMPLE** (bad):
  ```c
  if (cc->mode == CC_SLOW_START)
      cc->cwnd += cc->cwnd;      /* per-ACK doubling: 4x per RTT */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (cc->mode == CC_SLOW_START)
      cc->cwnd += 1;             /* +1 MSS per ACK, ~doubling per RTT */
  ```
- **VERIFICATION**: emulate one RTT of ACKs and assert `cwnd == 2 * start`;
  `tcp_ack_emu` in the host stubs does exactly this.
- **SOURCE**: rfc-5681 (sec 3.1); linux-tcp-docs.

## 2. Congestion avoidance adds 1/cwnd per ACK (one MSS per RTT)

- **RULE**: in congestion avoidance, each ACK increases `snd_cwnd` by at most
  `1/cwnd` MSS; over a full RTT the window grows by one MSS.
- **WHY AI GETS IT WRONG**: reuses the slow-start rule and grows by one MSS
  per ACK, so the window balloons by `cwnd` MSS per RTT.
- **CORRECT REASONING**: RFC 5681 section 3.1 allows `cwnd += SMSS * SMSS /
  cwnd` per ACK, the `1/cwnd` per-ACK form. One RTT contains `cwnd` ACKs, so
  the total increment is `cwnd * (1/cwnd) = 1` MSS — one packet per RTT.
- **EXAMPLE** (bad):
  ```c
  cc->cwnd += 1;                  /* +1 MSS per ACK in avoidance */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  cc->acc += 1;                   /* integer 1/cwnd accumulator */
  if (cc->acc >= cc->cwnd) { cc->cwnd++; cc->acc = 0; }
  ```
- **VERIFICATION**: emulate a full RTT of ACKs at fixed cwnd and assert
  `cwnd` grew by exactly 1; a per-ACK grower overshoots far beyond that.
- **SOURCE**: rfc-5681 (sec 3.1); linux-tcp-docs.

## 3. ssthresh is the switch point between slow start and avoidance

- **RULE**: `snd_ssthresh` separates the exponential (slow start) and linear
  (avoidance) phases. It is initialized high — RFC 5681: `snd_wnd =
  min(snd_wnd, rwnd)` with `snd_wnd = max(2*MSS, cwnd)` — and reduced on
  loss; crossing it from below ends slow start.
- **WHY AI GETS IT WRONG**: leaves ssthresh at its initial value forever or
  forgets to switch modes at the threshold.
- **CORRECT REASONING**: slow start ends when `cwnd >= ssthresh`; afterwards
  every ACK uses the avoidance increment. The threshold itself is
  intentionally dynamic because it encodes the estimated bottleneck
  capacity at the last loss event.
- **EXAMPLE** (bad):
  ```c
  if (cc->cwnd >= cc->ssthresh)
      ;                              /* no mode switch: still slow start */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (cc->cwnd >= cc->ssthresh)
      cc->mode = CC_AVOIDANCE;       /* switch at the threshold */
  ```
- **VERIFICATION**: assert the mode flips exactly when `cwnd` reaches
  `ssthresh` and growth rate changes afterward.
- **SOURCE**: rfc-5681 (sec 3.1); linux-tcp-docs.

## 4. Loss handling: RTO collapses cwnd to 1; 3 dup ACKs fast-retransmit

- **RULE**: on an RTO timeout, `snd_cwnd` is set to one MSS, `snd_ssthresh`
  to no more than half the old cwnd (min 2 MSS), and slow start is re-entered
  (RFC 5681 sec 3.1, 5.1). On three duplicate ACKs (or SACK indications), the
  sender performs fast retransmit: retransmit the missing segment, set
  `snd_ssthresh = max(cwnd/2, 2*MSS)`, and enter fast recovery.
- **WHY AI GETS IT WRONG**: sets `cwnd = ssthresh` on timeout (treating it
  like a soft reduction) or forgets to halve `ssthresh` on fast retransmit.
- **CORRECT REASONING**: a timeout means the network has lost the ACK clock;
  the window must restart at one MSS. Fast retransmit halves `ssthresh`
  because the path is congested. RFC 793's original retransmission semantics
  are inherited by RFC 5681.
- **EXAMPLE** (bad):
  ```c
  ssthresh = max(cc->cwnd / 2, 2);   /* never done: no timeout path */
  cc->cwnd = cc->ssthresh;           /* timeout should collapse to 1 */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  ssthresh = max(cc->cwnd / 2, 2);
  if (dup_acks >= 3) { cc->cwnd = ssthresh + 3; mode = CC_RECOVERY; }
  else               { cc->cwnd = 1;            mode = CC_SLOW_START; }
  ```
- **VERIFICATION**: `tcp_on_loss_emu`: `dup_acks == 0` must yield `cwnd ==
  1`; `dup_acks == 3` must yield `ssthresh == cwnd/2`.
- **SOURCE**: rfc-5681 (sec 3.1, 5.1); rfc-793.

## 5. Fast recovery: inflate by dup ACKs, deflate on the recovery ACK

- **RULE**: during fast recovery, the effective window is `ssthresh + dup_acks`
  (each duplicate ACK proves one segment left the network); each further dup
  ACK inflates by one. The ACK that covers the lost segment deflates the
  window to `ssthresh` and ends recovery.
- **WHY AI GETS IT WRONG**: leaves cwnd at `ssthresh` the whole recovery, or
  never deflates after the loss is acknowledged, or keeps counting dup ACKs
  after recovery ends.
- **CORRECT REASONING**: RFC 5681 sec 3.2 (Reno): the inflation only
  compensates for segments already removed from the path; it is not new
  capacity. The deflate on the recovery ACK restores the normal avoidance
  baseline.
- **EXAMPLE** (bad):
  ```c
  cc->cwnd = cc->ssthresh;           /* no inflate: underutilized */
  /* ... never deflates: cwnd stays ssthresh + k */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  cc->cwnd = cc->ssthresh + dup_acks;    /* inflate */
  /* per extra dup ACK: cc->cwnd++; */
  /* on ACK covering the loss: cc->cwnd = cc->ssthresh; leave recovery */
  ```
- **VERIFICATION**: assert inflated window permits `ssthresh + dup_acks`
  segments, then the recovery ACK returns cwnd to `ssthresh`.
- **SOURCE**: rfc-5681 (sec 3.2); linux-tcp-docs.

## 6. RTT-to-RTO estimation: SRTT/RTTVAR with bounded gain and backoff

- **RULE**: maintain `SRTT` and `RTTVAR`; compute `RTO = SRTT + 4*RTTVAR`,
  gains 1/8 (SRTT) and 1/4 (RTTVAR), with RTO clamped (RFC 6298-style rules).
  An RTO timeout doubles the RTO (exponential backoff). Describe honestly:
  RFC 793 specified the original estimator and the doubling; RFC 5681 relies
  on RTO-based retransmission; Linux caches per-destination RTT/ssthresh in
  `tcp_metrics`.
- **WHY AI GETS IT WRONG**: hard-codes `RTO = 2 * SRTT`, skips the backoff, or
  derives RTO from a wall-clock timestamp delta.
- **CORRECT REASONING**: the RTO must track both the mean and the variation
  of the RTT; `RTO = SRTT + 4*RTTVAR` absorbs jitter. Timeout implies the
  path state is unknown, so the next RTO is doubled and cwnd restarts.
- **EXAMPLE** (bad):
  ```c
  rto = 2 * srtt;                    /* no variation term, no backoff */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  rto = srtt + 4 * rttvar;           /* RFC 6298-style */
  if (rto < min_rto) rto = min_rto;
  /* on timeout: rto *= 2; */
  ```
- **VERIFICATION**: `rto_update_emu` produces `RTO >= SRTT` and stable values
  for constant samples; `rto_backoff_emu` doubles on timeout.
- **SOURCE**: rfc-793; rfc-5681 (sec 5.1); linux-tcp-docs (tcp_metrics).

## 7. ECN: CE marks are congestion signals, response is algorithm-specific

- **RULE**: packets marked CE are congestion signals; the documented loss
  response treats them like loss for cwnd reduction, but not every ECN path
  reduces the window by the same amount — the reduction is defined by the CC
  algorithm and the stack's ECN mapping (documented behavior, not a universal
  halving).
- **WHY AI GETS IT WRONG**: writes "ECN halves cwnd" as a blanket rule and
  applies Reno's halving inside CUBIC, or ignores CE marks entirely.
- **CORRECT REASONING**: ECN gives explicit congestion before loss; the kernel
  records CE and applies the congestion signal through the active CC
  algorithm's cwnd response. Keep the algorithm's own reduction path and pass
  ECN as a signal into it.
- **EXAMPLE** (bad):
  ```c
  if (ce_mark) cc->cwnd /= 2;        /* ignores the active CC algorithm */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (ce_mark) cc->ssthresh = cc->ssthresh_emu(cc);  /* CC's own response */
  ```
- **VERIFICATION**: host harness feeds a CE flag into `tcp_ack_emu` and
  asserts the reduction matches the algorithm's documented factor.
- **SOURCE**: linux-networking-docs; linux-tcp-docs; kernel-source.

## 8. tcp_congestion_ops: registration and callback contracts

- **RULE**: a congestion control module registers a `tcp_congestion_ops`
  (name, owner, flags) and implements callbacks: `ssthresh` (window after a
  congestion event), `congestion_avoid` (per-ACK growth), `congestion_control`
  (Reno hook), `undo_cwnd` (undo spurious reductions, F-RTO/ECN),
  `reno_ssthresh` (standard `cwnd/2`), plus `cwnd_event`, `pkts_acked`,
  `min_cwnd`, `set_state`, `get_info`.
- **WHY AI GETS IT WRONG**: invents callback names, returns a window from the
  wrong callback, or never implements `ssthresh` so slow-start growth never
  ends.
- **CORRECT REASONING**: the core drives the RFC 5681 machine and calls the
  ops at fixed points; each callback has a narrow contract. `ssthresh` is
  called when the window must shrink, `congestion_avoid` per ACK while in
  avoidance, `undo_cwnd` when a reduction is proven spurious.
- **EXAMPLE** (bad):
  ```c
  static struct tcp_congestion_ops cc = { .name = "mycc",
      .cong_avoid = NULL, .ssthresh = NULL };   /* core falls back, mode never switches */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static struct tcp_congestion_ops cc = {
      .name = "mycc", .owner = THIS_MODULE,
      .ssthresh = my_ssthresh, .cong_avoid = my_cong_avoid,
      .undo_cwnd = my_undo, .reno_ssthresh = my_reno_ssthresh,
  };
  ```
- **VERIFICATION**: symbol-check the ops struct against the kernel docs; host
  harness asserts each callback returns a value in the documented range.
- **SOURCE**: linux-tcp-docs; kernel-source (net/ipv4/tcp_cong.c).

## 9. CUBIC: cubic growth in time since congestion, TCP-friendly floor

- **RULE**: CUBIC (RFC 8312) grows the window as `W(t) = C*(t - t_cong)^3 +
  W_max` with C ~ 0.4 (MSS units), where `t` is time since the last
  congestion event; on congestion the window drops to `beta * W_max`
  (beta ~ 0.7); a TCP-friendly region limits the curve so it is never slower
  than Reno's linear probe; the window is never below one MSS.
- **WHY AI GETS IT WRONG**: uses wall-clock time, grows linearly, or applies
  Reno's halving (beta = 0.5) instead of beta ~ 0.7.
- **CORRECT REASONING**: the cubic term makes the window recover quickly
  toward `W_max`, then flatten near it; the TCP-friendly region bounds the
  recovery by Reno's +1 MSS/RTT so CUBIC cannot under-perform TCP in low
  RTTs. Time is measured since the congestion event, not from boot.
- **EXAMPLE** (bad):
  ```c
  cc->cwnd = 0.4 * pow(now - t0, 3) + cc->w_max;  /* absolute wall clock */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  uint32_t t = (now - cc->t_cong) / cc->rtt;      /* RTT-relative */
  cc->cwnd = cc->w_max * beta / 10 + 0.4 * t * t * t;  /* cubic + floor */
  ```
- **VERIFICATION**: `cubic_growth_emu`: assert monotone growth with
  `t_since_loss`, the beta floor at t = 0, the `W_max` cap, and the
  TCP-friendly bound.
- **SOURCE**: rfc-8312 (sec 2, 4); linux-tcp-docs.

## 10. Byte counting vs ACK counting

- **RULE**: RFC 5681 expresses cwnd growth per ACK in units of MSS. A
  byte-counting variant may count bytes only as an implementation detail; it
  must never grow the window faster than the per-ACK MSS rule would allow.
- **WHY AI GETS IT WRONG**: agents write `cwnd += bytes_acked`, which grows
  the window proportional to the last burst and can exceed the RFC 5681 bound.
- **CORRECT REASONING**: each ACK acknowledges at most one flight's worth of
  data; growth faster than one MSS per ACK (slow start) or 1/cwnd per ACK
  (avoidance) is not RFC 5681 compliant regardless of how bytes are counted.
- **EXAMPLE** (bad):
  ```c
  cc->cwnd += segs_acked;            /* linear in burst size */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  cc->cwnd += 1;                     /* one MSS per ACK, capped by ACK clock */
  ```
- **VERIFICATION**: drive a burst of ACKs through the harness and assert the
  cwnd increase equals the RFC 5681 increment for the same ACK count.
- **SOURCE**: rfc-5681 (sec 3.1); linux-tcp-docs.

## 11. The cwnd floor is one MSS

- **RULE**: `snd_cwnd` never drops below one MSS. Timeout sets it to exactly
  one MSS; recovery deflates to `ssthresh` which is at least 2 MSS; a cwnd of
  zero stalls the connection.
- **WHY AI GETS IT WRONG**: computes `cwnd/2` repeatedly until cwnd reaches
  zero, or models zero-length windows.
- **CORRECT REASONING**: one segment is the minimum useful window; RFC 5681
  requires cwnd >= 1 MSS at all times (and ssthresh >= 2 MSS after loss). A
  zero cwnd means no segment can be sent — a livelock.
- **EXAMPLE** (bad):
  ```c
  cc->cwnd = cc->cwnd / 2;           /* 8 -> 4 -> 2 -> 1 -> 0 -> stalled */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  cc->cwnd = cc->cwnd / 2 < 1 ? 1 : cc->cwnd / 2;  /* floor of 1 MSS */
  ```
- **VERIFICATION**: assert `tcp_on_loss_emu` with any cwnd never yields
  `snd_cwnd < 1`.
- **SOURCE**: rfc-5681 (sec 3.1, 4); linux-tcp-docs.

## 12. Wall-clock timers are wrong for RTT measurement

- **RULE**: RTT samples come from the ACK clock — the time between sending a
  segment and receiving its ACK. Wall-clock-based periodic timers drift from
  the ACK stream, inflate SRTT, and produce RTOs that are too long or too
  short.
- **WHY AI GETS IT WRONG**: "time the interval between two ticks" is easy to
  write, and it hides the fact that the ACK stream is the clock TCP must
  follow.
- **CORRECT REASONING**: RTT must bound the send-to-ACK delay of the data
  actually in flight; a wall-clock tick measures scheduling latency, not path
  delay. RTO derived from wrong RTT samples either spuriously retransmits or
  delays loss recovery.
- **EXAMPLE** (bad):
  ```c
  rtt = now - last_hrtimer_tick;     /* scheduling noise, not path delay */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  rtt = now - skb->tstamp;           /* send-to-ACK delay of the segment */
  rto_update_emu(cc, rtt);
  ```
- **VERIFICATION**: feed noisy wall-clock samples and constant path RTT
  samples into the estimator; assert SRTT tracks the true path RTT.
- **SOURCE**: rfc-5681 (sec 3.1); rfc-793; linux-tcp-docs.

## Quick detection table

| Pattern | Class | Check |
|---|---|---|
| `cwnd += cwnd` per ACK in slow start | wrong growth (4x/RTT) | per-ACK +1 MSS |
| `cwnd += 1` per ACK in avoidance | too aggressive (cwnd/RTT) | `1/cwnd` per ACK |
| `cwnd = ssthresh` on timeout | wrong loss response | cwnd = 1 MSS |
| no `ssthresh = cwnd/2` on fast retransmit | stuck slow start | halve, min 2 MSS |
| no inflate/deflate in recovery | Reno violated | inflate by dup ACKs |
| `RTO = 2 * SRTT` hard-coded | no jitter/backoff | SRTT + 4*RTTVAR |
| no RTO doubling on timeout | broken backoff | `rto *= 2` |
| wall-clock in CUBIC | wrong time base | time since congestion |
| Reno halving inside CUBIC | wrong beta | beta ~ 0.7 |
| `cwnd -= 1` below zero | window stall | floor at 1 MSS |
| per-burst byte counting | exceeds ACK-clock bound | cap by per-ACK MSS |
| CE always halves cwnd | ECN over-generalized | CC's own reduction |
