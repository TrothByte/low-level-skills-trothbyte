# Evaluation — tcp-congestion-control-internals

Skill: `skills/networking/tcp-congestion-control-internals`. Stability target:
`evaluated`.

## Verified facts (host, this run)

- All examples compile clean with `gcc -Wall -Wextra -Werror -O2` using
  self-contained stubs (`examples/stubs.h`) — no kernel headers required.
- Good example runs with all RFC 5681 assertions passing (exit 0, prints
  "ALL CHECKS PASSED").
- Bad example compiles, runs safely, reproduces each flaw with a "BUG
  reproduced" diagnostic (exit 0) without crashing the harness.

| Example | Compile | Run | Observation |
|---|---|---|---|
| good/good_tcpcc.c | 0 | 0 | "ALL CHECKS PASSED" — slow start doubles cwnd per RTT, avoidance +1 MSS per RTT, ssthresh switch, timeout cwnd=1, fast retransmit halves ssthresh, RTO backoff doubles |
| bad/bad_tcpcc.c | 0 | 0 | "BUG reproduced: congestion avoidance grows window too fast"; "BUG reproduced: timeout did not collapse window to 1"; "BUG reproduced: fast retransmit did not halve ssthresh" |

NOT verified on this host (documented targets, do NOT claim to have run):
kernel build, `sysctl net.ipv4.tcp_congestion_control`, `ss -tin`,
tools/testing/selftests/net, packetdrill.

## Historical CVE evals (adversarial)

| CVE | Class | Fixture | Detect | Fix | Verify |
|---|---|---|---|---|---|
| CVE-2019-11477 | integer overflow / OOB, CWE-190/787 — SACK Panic | `net/ipv4/tcp_sack.c` `tcp_shifted_skb()` | crafted SACK blocks against a very small window (`snd_wnd`) make skb shift counts overflow, causing division or out-of-bounds access; documented class: excessive memory allocation / crash via crafted SACK when `snd_wnd` is very small | validate shift counts against the skb's segment bounds (upstream 2019 fix) | KASAN + crafted-SACK reproducer |
| CVE-2018-5390 | resource amplification, CWE-400 — SegmentSmack | `net/ipv4/tcp_input.c` `tcp_fragment()`, skb queue handling | many small out-of-order segments force repeated skb allocation/split work per SACK, amplifying CPU and memory per packet | bound per-ACK queue/fragment work and coalescing limits (upstream 2018 fix) | perf + syzkaller with SACK enabled |

Each eval: DETECT (find the cwnd/window assumption that breaks) -> EXPLAIN
(which congestion control rule was violated) -> FIX (bound the arithmetic or
work) -> VERIFY (KASAN clean + reproducer). Only the well-documented classes
are used; no speculative details.

## Synthetic evals

- easy/positive: slow start adds exactly one MSS per ACK must NOT be flagged.
- easy/negative: `cwnd += cwnd` per ACK in slow start must be flagged.
- medium/negative: congestion avoidance growing cwnd by 1 MSS per ACK instead
  of 1/cwnd must be flagged.
- medium/negative: RTO timeout setting `cwnd = ssthresh` instead of 1 must be
  flagged.
- hard/negative: fast retransmit that never halves `ssthresh` must be flagged.
- hard/negative: CUBIC growth keyed to wall-clock time must be flagged.

## Adversarial evals

- A "CUBIC" module that doubles the window per RTT in avoidance — agent must
  not declare it correct.
- A `tcp_congestion_ops` that never implements `ssthresh` and so stays in
  slow-start growth forever.
- RTO code that skips backoff or hard-codes `RTO = 2 * SRTT` with no RTTVAR.
- ECN handling that applies Reno's halving inside CUBIC regardless of the
  active algorithm.

## False-positive evals (correct code must not be flagged)

- Correct RFC 5681 state machine: per-ACK +1 MSS in slow start, `1/cwnd` per
  ACK in avoidance — do NOT flag.
- Correct Reno fast recovery with inflate-by-dup-ACKs and deflate on the
  recovery ACK — do NOT flag.
- Correct CUBIC time-since-congestion growth with TCP-friendly floor — do NOT
  flag.
- RTO = SRTT + 4*RTTVAR with backoff and a cwnd floor of 1 MSS — do NOT flag.

## Verification commands

Host (self-contained stubs — recorded this run):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_tcpcc.c -o /tmp/good_tcpcc
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_tcpcc.c -o /tmp/bad_tcpcc
/tmp/good_tcpcc        # prints ALL CHECKS PASSED, exit 0
/tmp/bad_tcpcc         # prints 3 BUG reproduced lines, exit 0
```

Target (kernel) — documented only, NOT run here:

```
# active congestion control algorithm and per-connection state
sysctl net.ipv4.tcp_congestion_control
ss -tin

# Linux net selftests on a kernel with CONFIG_TCP_CONG_CUBIC and
# CONFIG_TCP_CONG_RENO (tools/testing/selftests/net)
make -C tools/testing/selftests/net

# packetdrill scripts against a loopback test TCP connection
packetdrill --wire_capture=test.pcap cc_script.pkt

# KASAN kernel + syzkaller for the CVE fixture classes
qemu-system-x86_64 -kernel arch/x86/boot/bzImage \
  -append "console=ttyS0 kasan=on" -nographic
```

## Scoring

- precision: every flagged pattern maps to a real RFC 5681 / tcp_congestion_ops
  rule.
- recall: each bad cwnd/loss/RTO pattern is detected by the host harness.
- FP-rate: correct state-machine, recovery, and CUBIC snippets produce zero
  flags.
