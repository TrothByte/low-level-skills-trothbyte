# Evaluation — sel4-sddf-driver-framework

Skill: `skills/kernel/sel4-sddf-driver-framework`. Stability target: `evaluated`.
Current stability: `researched` — host-side simulations and stub compiles were
run on this host (gcc 16.1.0, python 3.11.9); the seL4/Microkit build itself
requires a Linux host with the Microkit SDK and is documented, NOT run here.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/seL4_driver_stub_bad.c` | unbound IRQ + un-granted MMIO + swallowed errors | compiles (structural bug, see below) |
| easy/negative | `bad/virtio_queue_unsafe.py` | cross-component ring corruption accepted silently | prints violation, exit 0 — MASKED |
| medium/negative | `good/virtio_queue.py` tampered to touch `used` from frontend | must raise a fault | raises `Fault` (ownership table) |
| hard/positive | `good/seL4_driver_stub.c` | correct cap sequence: bind, check error, then wait | compiles with `-Werror` |
| hard/positive | `good/virtio_queue.py` | ownership respected, completion delivered | exit 0 |
| hard/negative | a ring where the driver rewrites `desc` | must be flagged | see `Driver.clobber_desc` in bad fixture |

Detection rule: for every component-side access, ask "who owns this field /
region in the system description?". A stub that compiles is not a granted
component; the grant lives in the description.

## False-positive evals (correct code must NOT be flagged)

- `good/seL4_driver_stub.c`: binding + error check + wait on the *bound*
  notification — no flag.
- A frontend that writes only `avail`/`desc` and reads only `used` — correct
  ring ownership, no flag.
- A driver that maps its device frame through a capability and checks the
  mapping error before first touch — correct, no flag.

## Historical evals

- sDDF exists because pre-sDDF seL4 drivers shared memory between driver and
  client with no defined ring protocol, and cross-component corruption and
  unbounded IPC round-trips were common; the virtio-style ring + capability
  grant is the fix. KNOWN abstract (design doc); the ownership-violation
  shape is reproduced locally by `bad/virtio_queue_unsafe.py`.
- UNVERIFIED: specific CVE-level incidents in SDDF itself (component is
  experimental; no public CVE list found during research).

## Adversarial evals

- `bad/virtio_queue_unsafe.py` runs and prints its violation — an agent that
  stops at "it runs and exits 0" certifies the broken pattern. The fault is
  supposed to come from the ownership check that is missing.
- `bad/seL4_driver_stub_bad.c` compiles with `-Werror` yet implements a dead
  IRQ path and an un-granted MMIO access — compile-clean ≠ configuration
  correct. This is the "compiles in the stub" trap
  (meta-verification-harness-validity).

## Verification commands (host, ACTUAL)

```
python examples/good/virtio_queue.py
  prints "GOOD: ownership respected, completion delivered"      exit 0
python examples/bad/virtio_queue_unsafe.py
  prints "BAD: isolation violated silently ..."                 exit 0 (MASKED)
gcc -Wall -Wextra -Werror -O2 -c examples/good/seL4_driver_stub.c
  exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/bad/seL4_driver_stub_bad.c
  exit 0 (compiles: the bug is architectural, not syntactic)
```

## Verification commands (target, RESEARCHED — not run on this host)

```
# Linux host with Microkit SDK 2.3.0 + sdfgen:
pip install sdfgen==0.33.0
# inside an sDDF example checkout:
make MICROKIT_SDK=/path/to/microkit-sdk-2.3.0
# boot with QEMU, exercise the device, observe the driver wake per IRQ;
# remove the IRQ binding and observe the driver never wakes.
# A component fault prints via the Microkit debugging guide.
```

## Verified facts

- Host compiles: `good/seL4_driver_stub.c` and `bad/seL4_driver_stub_bad.c`
  both compile with `-Wall -Wextra -Werror -O2` (exit 0) — KNOWN on this
  host (the point: syntax is not isolation).
- `python examples/good/virtio_queue.py` → exit 0, ownership OK. KNOWN.
- `python examples/bad/virtio_queue_unsafe.py` → exit 0, prints violation —
  KNOWN; demonstrates the masking trap.
- seL4 capability/IRQ/notification semantics — KNOWN from seL4 docs
  (fetched 2026-08-17), cited to proposed source `sel4-docs`.
- sDDF device classes, virtio-style protocols, sdfgen dependency — KNOWN
  from sDDF README (fetched 2026-08-17), cited to proposed source `sddf`.
- Microkit system-description semantics — KNOWN from Microkit docs,
  cited to proposed source `microkit-docs`.
- Any seL4/Microkit *runtime* behavior (fault on un-granted access, IRQ
  wakeups, QEMU boot) — UNVERIFIED on this host.

## Scoring

- precision: every flagged pattern must have a demonstrable isolation
  violation (missing grant, wrong field owner, unbound IRQ, swallowed
  error).
- recall: the system description, IRQ binding, error checks, and DMA
  frame-caps must all be demanded before "driver is correct".
- FP-rate: the two good fixtures and the ownership-table simulation produce
  zero flags.
- Strongest single fact: `virtio_queue.py` raises `Fault` on cross-owner
  access while `virtio_queue_unsafe.py` silently accepts it — the
  ownership-check delta is recorded, not assumed.
