---
name: tcp-congestion-control-internals
description: Use when writing or reviewing TCP congestion control code — the tcp_congestion_ops interface, snd_cwnd/ssthresh evolution, slow start vs congestion avoidance, RTT/RTO estimation, packet loss vs ECN signals, and CUBIC/reno behavior. Teaches the RFC 5681 state machine so cwnd changes are correct and safe.
---

# TCP Congestion Control Internals

Rules for the Linux TCP congestion control path: the RFC 5681 state machine,
snd_cwnd/ssthresh evolution, loss handling, RTT/RTO estimation, ECN, and the
tcp_congestion_ops interface. Load `references/README.md` before writing code
that touches cwnd.

## When to use

- Writing or reviewing code touching `snd_cwnd` / `snd_ssthresh`.
- Implementing `tcp_congestion_ops`: `ssthresh`, `congestion_avoid`,
  `congestion_control`, `undo_cwnd`, `reno_ssthresh`, `cwnd_event`,
  `pkts_acked`, `min_cwnd`, `set_state`, `get_info`.
- Debugging throughput collapse, burstiness, or a wrong loss response.
- Reviewing ECN, RTT/RTO estimation, or CUBIC/reno growth logic.
- Answering "how fast should cwnd grow here?" — slow start vs avoidance.

## When not to use

- Userspace socket code; the stack already manages cwnd.
- Driver / NIC / NAPI / GRO paths with no cwnd (see `napi-network-driver`).
- Non-Linux stacks: BSD, Windows (Compound TCP), QUIC (RFC 9002 CC).
- Routing, firewalling, or protocol-independent networking.

## What the agent often gets wrong

- Thinking slow start grows cwnd by `cwnd` per RTT. It doubles per RTT, but
  the mechanism is +1 MSS per ACK — the ACK clock, not a timer.
- Confusing ACK counting with byte counting; byte-counting variants must not
  grow faster than the per-ACK MSS bound.
- Wrong ssthresh restore: on RTO it is set to at most half the old cwnd
  (min 2 MSS); on fast retransmit to `cwnd/2`. Agents keep the pre-loss value.
- Assuming loss always halves the window. CUBIC reduces to `beta * W_max`
  (~0.7), then grows as a cubic function of time since congestion.
- Using wall-clock time in CUBIC instead of RTT-relative time since the last
  congestion event (RFC 8312).
- Treating ECN CE marks as a universal halving; the cwnd response is
  algorithm-specific, documented per path.
- Believing RTO is simply `2 * SRTT`; the honest model is SRTT/RTTVAR with
  bounded gain, backoff, and per-destination state (`tcp_metrics`).

## How to reason correctly

1. Track `snd_cwnd` (MSS), `snd_ssthresh`, `inflight`, and a mode
   (SLOW_START / AVOIDANCE / RECOVERY) — the RFC 5681 state machine.
2. Slow start: `snd_cwnd += 1` MSS per cumulative ACK — roughly one doubling
   per RTT with a full window in flight; never faster than one MSS per ACK.
3. Congestion avoidance: `snd_cwnd += 1/snd_cwnd` per ACK — one MSS per RTT.
4. `snd_ssthresh` is the switch point: initialized high (RFC 5681
   `min(snd_wnd, rwnd)`), reduced on loss, ends slow start when reached.
5. Loss: on RTO timeout `snd_cwnd = 1` MSS, slow start re-entered; on three
   duplicate ACKs (or SACK) fast retransmit with
   `snd_ssthresh = max(cwnd/2, 2*MSS)` and fast recovery.
6. Fast recovery (Reno): inflate the window by the dup-ACK count; on the ACK
   covering the loss, deflate to `snd_ssthresh` and leave recovery.
7. RTT estimation: SRTT/RTTVAR to `RTO = SRTT + 4*RTTVAR` (RFC 6298-style).
   Describe honestly: RFC 793 defined the estimator and timeout backoff,
   RFC 5681 relies on it, Linux keeps per-destination state in `tcp_metrics`.
8. ECN: CE marks are congestion signals; apply the loss response through the
   active CC algorithm's own reduction — not a universal halving.
9. CUBIC (RFC 8312): `W(t) = C*(t - t_cong)^3 + W_max` with C ~ 0.4, drop to
   `beta * W_max` (beta ~ 0.7), TCP-friendly region below the curve, cwnd
   floor of 1 MSS; time is RTT-relative, never wall-clock.
10. `tcp_congestion_ops` callbacks have contracts: `ssthresh` computes the
    post-loss window, `congestion_avoid` grows cwnd per ACK, `undo_cwnd`
    reverses spurious reductions, `reno_ssthresh` is `cwnd/2`.

## What to verify

- Slow start adds exactly one MSS per cumulative ACK, never faster.
- Congestion avoidance adds `1/cwnd` per ACK — one MSS per RTT.
- `snd_ssthresh` halved (min 2 MSS) on fast retransmit, at most half on RTO.
- RTO timeout collapses `snd_cwnd` to 1 MSS.
- Fast recovery inflates by dup-ACK count, deflates on the recovery ACK.
- RTO estimator seeded, finite, and doubled on timeout; CUBIC uses time since
  congestion with the TCP-friendly region and a 1 MSS floor.
- No wall-clock timers for RTT; ECN paths apply a documented reduction.

## How to verify

Host-compilable logic checks (self-contained stubs, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_tcpcc.c -o /tmp/good_tcpcc
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_tcpcc.c -o /tmp/bad_tcpcc
```

Target (kernel) checks — document these, do not claim to have run them:

```
sysctl net.ipv4.tcp_congestion_control   # active CC algorithm
ss -tin                                  # per-connection cwnd/ssthresh/RTT
# Linux net selftests (tools/testing/selftests/net) + packetdrill scripts
```

## Where the knowledge comes from

- `linux-tcp-docs` — tcp_congestion_ops API and TCP CC docs
- `rfc-5681` — slow start, avoidance, fast retransmit/recovery
- `rfc-793` — RTO estimation and exponential backoff
- `rfc-8312` — CUBIC growth, beta reduction, TCP-friendly region
- `linux-networking-docs` — stack behavior, tcp_metrics, sysctls
- `kernel-source` — tcp_cong.c, tcp_input.c, tcp_cubic.c
- `nvd-cve` — CVE-2019-11477 and CVE-2018-5390 classes

## Related skills

- `sk-buff-socket-buffer-management` (recommend) — skb ownership in the CC path
- `napi-network-driver` (recommend) — receive context that delivers ACKs
- `kernel-atomic-context` (recommend) — what CC callbacks may do in BH context
- `memory-ordering-reasoning` (recommend) — shared cwnd/sk field ordering
- `fuzzing-harness-kernel` (recommend) — packetdrill/syzkaller seeds for CC
- `networking-hardware-rdma-nic-offload` (recommend) — offloaded transports

## Evaluation

Historical CVEs: CVE-2019-11477 (SACK Panic — `tcp_shifted_skb` integer
overflow on small window / zero-window skb, leading to division or OOB;
documented class: excessive memory allocation / crash via crafted SACK when
`snd_wnd` is very small) and CVE-2018-5390 (SegmentSmack — `tcp_fragment` /
skb allocation amplification from crafted segments). Only the documented
classes are used; no speculative details.

Synthetic: slow start faster than one MSS per ACK; avoidance adding one MSS
per ACK; timeout setting `cwnd = ssthresh`; fast retransmit not halving
`ssthresh`. Adversarial: CUBIC on wall-clock time, avoidance doubling per
RTT, missing `ssthresh` callback, RTO without backoff. False-positive:
correct RFC 5681 machine, Reno fast recovery, CUBIC time-since-congestion,
correct `1/cwnd` avoidance — must NOT be flagged.
