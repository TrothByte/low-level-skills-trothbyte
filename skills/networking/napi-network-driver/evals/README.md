# Evaluation — napi-network-driver

Skill: `skills/networking/napi-network-driver`.
Stability: `researched` (needs a Linux kernel for target checks). The NAPI
state machine and the bug-detection rules are host-verified with python
3.11.9 on 2026-08-20; kernel-header/hardware steps are documented and NOT run
on this host.

## Verified facts (host, recorded 2026-08-20)

`python examples/good/napi_model.py` — exit 0, 3/3 scenarios PASS:

```
[PASS] heavy load: 8 poll rounds, all returned budget=64, stays SCHEDULED, IRQ masked, completes=0, schedules=1/1
[PASS] light load: work_done=5, state=IDLE, IRQ re-enabled=True, completes=1
[PASS] IRQ re-entrancy: 3 IRQs but only 1 schedule, poll_calls=8 (no re-entry)

NAPI model: 3/3 scenarios PASS
```

`python examples/bad/napi_misuse.py` — exit 0, all three bug classes caught:

```
[PASS] bug=complete_on_budget telemetry={'work_done': 64, 'state': 'IDLE', 'irq_fires': 0, 'ring': 136}
       DIAGNOSTIC: napi_complete with work_done == budget; ring not drained, instance must stay scheduled
[PASS] bug=heavy_irq telemetry={'irq_ret': 'processed_in_irq:200', 'poll_calls': 0}
       DIAGNOSTIC: packet processing inside IRQ handler defeats NAPI
[PASS] bug=irq_not_masked telemetry={'work_done': 64, 'state': 'SCHEDULED', 'irq_fires': 2, 'ring': 136}
       DIAGNOSTIC: napi_schedule without masking the queue IRQ (re-entrancy)

NAPI misuse checker: 3 bugs checked, ALL FLAGGED
```

`python examples/tools/napi_check.py examples/good/driver_sketch.c examples/bad/driver_sketch.c`
— exit 1 (violations found, as intended):

```
[OK] .../examples/good/driver_sketch.c (polls=1, irqs=3)
[5 VIOLATION(S)] .../examples/bad/driver_sketch.c (polls=1, irqs=1)
  FLAG mynic_poll_buggy:14: netif_receive_skb() in poll path bypasses GRO; use napi_gro_receive() for GRO-enabled NICs
  FLAG mynic_poll_buggy:19: napi_complete() called without `work_done < budget` guard; instance must stay scheduled at full budget
  FLAG mynic_poll_buggy:7: blocking call `mutex_lock()` in poll() — forbidded in softirq context
  FLAG mynic_irq_buggy:6: packet processing in IRQ handler defeats NAPI — schedule the poll round instead
  FLAG mynic_irq_buggy:10: napi_schedule() without masking the queue IRQ — re-entrancy / interrupt storm

napi_check: 5 violation(s) across 2 file(s)
```

Known limitation (honest): `napi_check.py` is textual. It strips comments and
splits functions by brace counting; a call is considered "guarded" only if an
`if (...)` line mentioning `work_done`/`budget` appears in the preceding 6
lines. It cannot evaluate real control flow — a human review of the
surrounding code is still required, and the good sketch is the false-positive
regression fixture.

## Synthetic evals

| Case | Fixture | Expected | Result |
|---|---|---|---|
| heavy load, budget exhausted | `good/napi_model.py` scenario (a) | stays SCHEDULED, no complete, IRQ masked | PASS (8 rounds, completes=0) |
| light load | `good/napi_model.py` scenario (b) | napi_complete_done, IDLE, IRQ on | PASS (work_done=5) |
| IRQ re-entrancy | `good/napi_model.py` scenario (c) | no re-schedule, no re-entry | PASS (3 IRQs -> 1 schedule) |
| complete-on-budget bug | `bad/napi_misuse.py` | diagnostic fired | PASS |
| heavy-work-in-IRQ bug | `bad/napi_misuse.py` | diagnostic fired | PASS |
| unmasked-IRQ bug | `bad/napi_misuse.py` | diagnostic fired | PASS |
| good C sketch | `good/driver_sketch.c` | 0 violations | PASS |
| bad C sketch | `bad/driver_sketch.c` | 5 violations | PASS |

## False-positive evals

- A correct poll with `napi_gro_receive()` + guarded `napi_complete_done()`
  must not be flagged — verified: the good sketch reports `OK`.
- A clean IRQ handler (schedule + `mynic_disable_queue_irq`) must not trigger
  the unmasked-IRQ rule — verified (mask call detected, no flag).
- A driver whose poll explicitly checks the ring before completing must pass
  even if it returns `budget` on one round (stay-scheduled is legal).
- `napi_complete_done(napi, work_done)` under an `if (work_done < budget)`
  guard must pass the guard heuristic — verified on the good sketch.

## Historical evals

- Wedged-device class: drivers that returned 0 without `napi_complete`, or
  returned full budget without processing, historically left instances
  scheduled forever (`NAPI_STATE_SCHED` stuck) with no IRQ and no polling.
  Reproduced as `bad/napi_misuse.py` bug 1 fixture; UNVERIFIED against a
  curated CVE corpus in this repository.
- GRO-regression class: drivers whose `netif_receive_skb()` replaced
  `napi_gro_receive()` showed flat `rx_gro_*` counters and higher CPU;
  reproduced as the `netif_receive_skb` flag in `napi_check.py`. No registered
  CVE is attached; the failure signature is the documented counter evidence.
- Real-driver equivalent noted in the community (UNVERIFIED here): IRQ storms
  after an unconditional `napi_complete` at full budget.

## Adversarial evals

- A claim that returning `budget` AND calling `napi_complete` is correct must
  be rejected — `napi_misuse.py` bug 1 and `napi_check.py` flag it.
- A claim that "processing packets in the IRQ handler is fine for batching"
  must be rejected — bug 2 fixture.
- A driver that schedules without masking and claims "no storm because the
  kernel dedupes" must be rejected — bug 3 fixture; the re-entrant IRQ is
  still a storm even if `napi_schedule` is a no-op.
- A poll that uses `mutex_lock` with a claim of "it rarely contends" must be
  flagged — blocking is forbidden in softirq context regardless of
  contention.

## Verification commands (target — Linux host)

```
make -C /lib/modules/$(uname -r)/build M=$PWD modules   # build module
sudo insmod <module>.ko                                  # load
ethtool -S <iface> | grep -iE 'gro|drop'                 # GRO + drop counters
cat /proc/net/softnet_stat                                # backlog / time_squeeze
bpftrace -e 'kprobe:napi_schedule { @[comm]=count(); }'  # schedule call rate
tcpdump -i <iface> -c 1000 -w rx.pcap                     # capture with NAPI visible
```

Elevation path to `source-backed`: run the target commands on a Linux host
with a GRO-capable NIC and kernel headers, and record `ethtool -S` gro
counter deltas under `iperf` traffic plus `softnet_stat` behavior.

## Scoring

- 0 verified-host facts diverging from recorded output (all three scripts
  reproduced above verbatim).
- 0 false positives on the good fixtures.
- 5/5 targeted violations detected in the bad C sketch.
- Target-hardware claims scored as `researched` until the elevation commands
  above produce recorded counters on a Linux host.
