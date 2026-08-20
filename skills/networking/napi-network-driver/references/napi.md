# NAPI — the New API for Linux network drivers

Deep reference for `napi-network-driver`. The operational rules live in
`SKILL.md`; this file carries the kernel-level detail, the exact API surface,
and the failure modes.

## What NAPI is

NAPI is the interrupt-mitigation model Linux uses for NIC receive paths.
Instead of the NIC firing an IRQ per packet, the driver:

1. receives an IRQ announcing that the queue has data,
2. runs minimal IRQ-handler work: `napi_schedule()` and mask the queue IRQ,
3. lets the kernel raise `NET_RX_SOFTIRQ`, which calls the registered
   `->poll()` callback in softirq context with a per-round budget,
4. processes up to `budget` packets per poll round, returning the count
   actually processed,
5. re-enables the IRQ only when the ring is drained (work_done < budget).

The payoff: under heavy load the IRQ stays masked and the driver polls in
batches; under light load the IRQ is re-armed so latency stays low.

## Lifecycle / state machine

States (bit flags in `napi->state`):

- `NAPI_STATE_SCHED` — instance is queued on the softnet backlog; poll will
  run (or is running). `napi_schedule()` sets it.
- `NAPI_STATE_POLL` — poll is currently executing.
- `NAPI_STATE_DISABLE` — `napi_disable()` is in progress (waits for poll to
  return).
- `NAPI_STATE_BUSY_POLL` — busy polling in progress.

Transitions:

```
        IRQ (queue has data)
        |
        v
 IDLE ---------> SCHEDULED        IRQ handler: napi_schedule() + mask IRQ
        ^                |
        |                | NET_RX_SOFTIRQ picks the instance
        |                v
        |         poll(weight/budget)
        |                |
        |   +------------+-------------+
        |   |                          |
        |   v                          v
        |  work_done < budget     work_done == budget
        |   |                          |
        |   | napi_complete_done       | return budget; DO NOT complete;
        |   | (re-enable IRQ if        | instance stays SCHEDULED, softirq
        |   |  it returns true)        | re-runs poll next pass
        |   |                          |
        +---+                          +------> SCHEDULED (re-queued)
```

An IRQ that arrives while the instance is SCHEDULED is effectively a no-op:
`napi_schedule()` sees `NAPI_STATE_SCHED` and returns false. That is why the
queue IRQ is masked by the driver while polling — otherwise the IRQ would
re-fire continuously even though the work is already queued.

## Core API surface

- `netif_napi_add_weight(dev, napi, poll, weight)` — register a NAPI instance
  bound to a net_device; `weight` is the budget handed to `poll()` per round.
  Must be called before the device is up. (Old name: `netif_napi_add`.)
- `napi_schedule(napi)` — if not already scheduled, set `NAPI_STATE_SCHED`
  and queue the instance for `NET_RX_SOFTIRQ`. Safe from hard IRQ.
  `napi_schedule_prep()` + `__napi_schedule()` is the explicit two-step form.
- `napi_complete(napi)` — clear scheduled state, re-enable the IRQ. Does NOT
  take a work counter.
- `napi_complete_done(napi, work_done)` — same, but returns a bool: true when
  the driver should re-enable the IRQ, false when the kernel wants polling to
  continue (e.g. busy polling). Modern drivers call this and re-enable the IRQ
  only on true.
- `napi_gro_receive(napi, skb)` — hand an skb to the GRO engine. This is the
  receive path for NAPI drivers; `netif_receive_skb()` bypasses GRO and is
  only correct for non-NAPI/non-GRO paths.
- `napi_disable(napi)` — wait for an in-flight poll to return and prevent new
  ones. Pair with `netif_napi_del()` at teardown.
- `napi_enable(napi)` — allow scheduling again at `open()`.
- `napi_busy_loop()` — synchronous busy polling for low-latency sockets.

## Budget and weight semantics

- The budget passed to `poll()` is the maximum number of packets for THIS
  poll round, not the ring size. Drivers that read `budget` from the ring
  depth are wrong.
- Returning the full budget keeps the instance scheduled; the softirq loop
  (`net_rx_action` in `net/core/dev.c`) re-queues it. Returning less means
  "I drained the queue" and expects `napi_complete*()`.
- `net_rx_action` itself is bounded per softirq invocation (quota), after
  which the softirq is re-raised — so a saturated driver consumes CPU in
  bounded slices, not unboundedly.

## GRO

GRO coalesces multiple skbs matching a flow into one larger skb before the
stack sees them, cutting per-packet overhead. Driver responsibilities:

- call `napi_gro_receive()`, not `netif_receive_skb()`, on the NAPI path;
- keep `NAPI_GRO_CB(skb)` metadata valid (NAPI instance, frag accounting);
- let `napi_skb_finish()` / GRO state machinery decide flush timing.

Counter evidence: `ethtool -S <iface>` shows `rx_gro_receive` /
`rx_gro_flush` on GRO-capable drivers; a driver that quietly switched to
`netif_receive_skb` shows those counters flat and CPU rises.

## Context and locking rules

- `poll()` runs in softirq context: no sleeping, no `mutex_lock()`, no
  `schedule_timeout()`, no `wait_event*()`, no `GFP_KERNEL` allocation that
  can sleep. Use spinlocks, local_bh discipline, `GFP_ATOMIC` (avoid even that
  if possible via skb pools).
- The IRQ handler must be minimal: ack + `napi_schedule()` + mask. Per-packet
  work there re-introduces the exact IRQ overhead NAPI removes.
- Teardown ordering: mask/disable the IRQ, `synchronize_irq()` (or
  `free_irq`), `napi_disable()`, `netif_napi_del()`. This closes the
  poll/remove race: `napi_disable()` waits for a running poll to return before
  you free the private data it touches.

## Common failure modes and evidence

| Failure | How it looks | Evidence |
|---|---|---|
| `napi_complete` at full budget | IRQ re-enabled while ring full; re-fire, storm, CPU pegged | `watch -n1 cat /proc/interrupts`; irq count climbing |
| Return budget without processing | `work_done==budget` but no packets consumed | softnet_stat backlog growth |
| Poll returns 0 without complete | instance stays scheduled forever; device wedged | `/proc/net/softnet_stat` stalled, `ethtool -S rx_dropped` climbing |
| Work in IRQ handler | high irq time, low softirq time, no batching | `perf top`; `top` irq vs softirq |
| `netif_receive_skb` in NAPI driver | GRO counters flat, high CPU | `ethtool -S ... | grep gro` |
| IRQ not masked on schedule | re-entrant IRQs while poll queued | `irq` counter > `rx` counter growth ratio |

## Verification commands (target — Linux host with the driver installed)

```
make -C /lib/modules/$(uname -r)/build M=$PWD modules   # build module
sudo insmod <module>.ko                                  # load
ethtool -S <iface> | grep -iE 'gro|drop'                 # GRO + drop counters
cat /proc/net/softnet_stat                                # backlog / time_squeeze
bpftrace -e 'kprobe:napi_schedule { @[comm]=count(); }'  # schedule call rate
tcpdump -i <iface> -c 1000 -w rx.pcap                     # traffic capture
```

## Sources

- NAPI documentation — https://docs.kernel.org/networking/napi.html
- netdev/net-next API — `netif_napi_add`, `napi_schedule` (https://docs.kernel.org/),
  kernel source `net/core/dev.c` (`net_rx_action`, `napi_complete_done`)
- "The Linux Networking Architecture: Design and Implementation of Advanced
  Linux Networking" (Klaus Wehrle et al.), chapter "NAPI and the softnet
  driver model"
- `ethtool -S` stats and GRO counters documentation
