---
name: napi-network-driver
description: Use when writing, reviewing, or debugging Linux network drivers using NAPI — napi_struct, napi_schedule, poll budget, napi_complete, GRO, or IRQ-coalescing and softirq behavior. Teaches the NAPI poll discipline agents get wrong in driver code.
---

# NAPI Network Driver Discipline

## When to use

- Writing or reviewing a NIC driver's NAPI receive path: `napi_struct`
  registration, the IRQ handler, the `poll()` callback, and teardown.
- Reasoning about `napi_schedule`, `napi_complete_done`, poll budgets, and
  the `NET_RX_SOFTIRQ` re-run loop.
- Debugging "device wedged", "interrupt storm", or "high IRQ CPU" in a
  network driver, or GRO regression after a receive-path change.
- Answering questions about IRQ coalescing vs NAPI and softirq context
  rules in drivers.

## When not to use

- Userspace networking, sockets, or NIC offload configuration (`ethtool -k`
  tuning) with no driver code — use `networking-hardware-rdma-nic-offload`
  for offload-layer claims.
- Writing a full driver from scratch on this host: kernel headers and a
  Linux testbed are required for real verification.
- Non-NAPI / non-GRO legacy receive paths (pre-2.4 style `netif_rx` drivers).

## What the agent often gets wrong

1. Calling `napi_complete` when `work_done == budget`. The budget was
   exhausted, so the instance must STAY scheduled; completing re-enables the
   IRQ while the ring is still full — immediate re-fire and interrupt storm.
2. Returning the full budget without having processed anything, or returning
   0 without calling `napi_complete` — the instance is left scheduled forever
   and the device wedges (no polling, no IRQ).
3. Doing heavy work (building skbs, running protocol hooks) in the IRQ
   handler. This defeats NAPI entirely.
4. Forgetting to mask the queue IRQ when scheduling NAPI — a re-entrant IRQ
   fires while the instance is already scheduled.
5. Calling `netif_receive_skb()` instead of `napi_gro_receive()` in the NAPI
   path — GRO disabled, wrong semantics for GRO-enabled NICs, CPU collapse.
6. Calling `napi_schedule` from the wrong context: from inside `poll()`
   itself, or before `netif_napi_add_weight`.
7. Missing the poll/remove race: `netif_napi_del` or freeing private data
   while `poll()` is still running (no `napi_disable`).
8. Confusing the NAPI budget (packets per poll round) with ring sizes.

## How to reason correctly

1. Model the state machine: `IDLE -> (IRQ) -> SCHEDULED -> (softirq poll) ->
   IDLE | SCHEDULED`. The only thing that un-schedules is `napi_complete*`;
   the only thing that schedules is `napi_schedule` (from the IRQ handler).
2. In `poll()`: process up to `budget` packets. If you consumed the full
   budget, return `budget` WITHOUT `napi_complete` — the softirq re-runs
   poll. If you drained the ring (`work_done < budget`), call
   `napi_complete_done(napi, work_done)` and return `work_done`.
3. IRQ handler is minimal: `napi_schedule()` (or `napi_schedule_prep` +
   `__napi_schedule`) plus masking the queue IRQ. Re-enabling happens via
   `napi_complete_done` — never in the IRQ handler.
4. Use the GRO path: `napi_gro_receive()`, keep `NAPI_GRO_CB(skb)` valid.
   `netif_receive_skb()` is only for non-NAPI paths.
5. For any driver claim, name the counters that prove it: `ethtool -S`
   (gro/coalesce counters), `/proc/net/softnet_stat`, interrupt counters.
6. Teardown: disable the IRQ, `synchronize_irq()`, `napi_disable()`, then
   `netif_napi_del()` — never the reverse, never skipping `napi_disable`.

## What to verify

- IRQ handler contains only `napi_schedule` + minimal bookkeeping, and the
  queue IRQ is masked on schedule.
- `poll()` returns the ACTUAL count processed, and `napi_complete*` is only
  called under `work_done < budget`.
- GRO is used (`napi_gro_receive`); no `netif_receive_skb` on the NAPI path.
- No blocking/sleeping calls (`mutex_lock`, `msleep`, `wait_event*`) in
  `poll()` — softirq context.
- Clean teardown: `napi_disable` before `netif_napi_del`; no use-after-free
  on driver removal.
- Budget is treated as a per-round packet cap, not a ring size.

## How to verify

```
# Host (python 3.11, runs on this machine):
python examples/good/napi_model.py          # 3/3 PASS (budget/batching state machine)
python examples/bad/napi_misuse.py          # 3/3 bugs flagged with diagnostics
python examples/tools/napi_check.py examples/good/driver_sketch.c examples/bad/driver_sketch.c
#   -> good: 0 violations; bad: 5 violations (GRO bypass, unguarded complete,
#      mutex in poll, work in IRQ, unmasked IRQ)

# Target (Linux host with kernel headers; NOT run on this host):
make -C /lib/modules/$(uname -r)/build M=$PWD modules   # build the module
ethtool -S <iface> | grep -i gro                         # gro counters moving
cat /proc/net/softnet_stat                               # backlog / time_squeeze
bpftrace -e 'kprobe:napi_schedule { @[comm]=count(); }'  # schedule call rate
```

The three python scripts were executed on this host and their output is
recorded verbatim in `evals/README.md`. The C sketches are target-only: they
are deliberately not compiled here because kernel headers are absent.

## Where the knowledge comes from

- NAPI documentation (https://docs.kernel.org/networking/napi.html)
- netdev/net-next API — netif_napi_add, napi_schedule (https://docs.kernel.org/),
  kernel source net/core/dev.c
- The Linux Networking Architecture (book), "NAPI and the softnet driver
  model"
- ethtool -S stats and gro counters docs

## Related skills

- `kernel-driver-char-device-lifecycle` — driver registration/teardown order
  around NAPI-enabled devices.
- `kernel-atomic-context` — softirq context rules that constrain `poll()`.
- `networking-hardware-rdma-nic-offload` — NIC offload layer where GRO/NAPI
  claims must be attributed correctly.
- `kernel-debugging-ftrace-kprobes-kdump` — kprobe/bpftrace and ftrace
  verification of `napi_schedule` and poll paths.
- `kernel-scheduler-mm-vfs-internals` — when to attribute scheduling/softirq
  stalls to NAPI vs core kernel behavior.

## Evaluation

- Synthetic: `napi_model.py` scenario tests (heavy/light load, IRQ
  re-entrancy) must all PASS; `napi_misuse.py` must flag all three bug
  classes with the expected diagnostics; `napi_check.py` must flag the bad
  sketch and pass the good one.
- False-positive: a correct poll (`napi_gro_receive` + guarded
  `napi_complete_done`) and a clean IRQ handler must not be flagged; the good
  sketch is the regression fixture.
- Adversarial: a driver that returns budget without processing, or that
  calls `napi_complete` at full budget while claiming correctness, must be
  caught; see `evals/README.md`.
- Historical: the wedged-device and GRO-regression classes are reproduced as
  fixtures (UNVERIFIED against an upstream CVE corpus in this repository).
- Stability `researched`: the state machine is host-verified; kernel-source
  and hardware behavior require the target Linux host documented above.
