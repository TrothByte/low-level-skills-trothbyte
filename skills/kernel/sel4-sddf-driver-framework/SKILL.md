---
name: sel4-sddf-driver-framework
description: Use when writing or reviewing seL4 device drivers, SDDF components, or Microkit systems. Teaches capability-based driver isolation, virtio queue protocols for network/block/serial, DMA and shared-memory rules, and why drivers are user-level servers, not kernel modules.
---

# seL4 / SDDF Device Driver Framework

## When to use

- Writing a device driver as an seL4 user-level process, or porting a Linux
  driver to seL4.
- Using the seL4 Device Driver Framework (sDDF) interfaces for network, block,
  serial, I2C, or audio components.
- Designing a Microkit system: protection domains, channels, memory regions,
  IRQ claims, notification wiring.
- Reviewing who holds which capability and what a component can actually
  touch (MMIO, DMA, shared rings).

## When not to use

- Linux or other monolithic-kernel drivers using `request_irq`/`ioremap` —
  use `kernel-driver-char-device-lifecycle`.
- Bare-metal MMIO register discipline (no kernel, single thread) — use
  `embedded-hw-register-datasheet-verification`.
- Scheduling, preemption, or lock ordering inside a driver — use
  `deadlock-kernel-prevention`.
- Formal verification of the kernel itself — use `invariant-identification`.

## What the agent often gets wrong

- Treats an seL4 driver like a kernel module: `ioremap`-style global pointers
  and `request_irq`-style global handlers. seL4 has no global address space:
  every access needs a capability, and IRQs arrive as notifications, not
  handler calls.
- Assumes shared memory is shared. In seL4/Microkit a shared region exists
  only where a memory region is explicitly granted to the component; an
  un-granted address is a fault. "It compiles in the stub" is not "it is
  granted in the system description" (meta-verification-harness-validity).
- Writes the driver to wait for an IRQ notification it never bound: the
  notification object must be allocated, the IRQ capability used to bind the
  IRQ to that notification (`seL4_IRQHandler_SetNotification`), then the
  driver blocks on `seL4_Wait`.
- Ports a Linux driver verbatim into an sDDF component. sDDF prescribes
  virtio-style ring protocols between a frontend (guest) and backend
  (driver) — Linux `struct net_device` ops do not map onto them.
- Ignores DMA ownership: the device may DMA into memory the driver did not
  map for the device; on seL4 that memory is not shared, so the device data
  is garbage or faults — the driver must map/pass the exact frame capability.
- Calls seL4 syscalls and ignores their error codes (IPC can fail with
  `seL4_InvalidArgument` when a capability is missing). Error checks are the
  only oracle for capability mistakes.
- Confuses the Microkit system description with runtime allocation: regions
  and channels are declared statically (e.g. `system.xml`); an agent that
  "allocates a shared region at runtime" is inventing an API (B2).

## How to reason correctly

1. Draw the component diagram: which protection domains exist, which
   capabilities each has (memory regions, IRQ, notifications, CNode for IPC
   endpoints).
2. Choose the driver topology: which component owns the device, which
   component is the frontend (virtual device user), and which channel type
   (virtio queue for net/block; serial/I2C have their own protocols).
3. Grant only what is needed: the driver gets MMIO frames for the device and
   IRQ capability; the frontend gets the shared ring; nothing else.
4. Wire interrupts explicitly: allocate a notification, bind it to the IRQ
   handler capability, then block on it in a receive loop.
5. Return errors on every seL4 call and treat failed IPC as a capability or
   protocol bug.
6. Check DMA: the memory the device writes must be a frame capability you
   passed to the component/model that owns the device, with the cache/IO
   mapping it expects.
7. Verify in the system description: if the region/channel is not in the
   config, it does not exist at runtime.

## What to verify

- Every device memory range used by the driver appears as a granted memory
  region of its protection domain in the Microkit/sDDF system description.
- The IRQ the driver waits on is bound to a notification via the IRQ
  capability; the binding call's error is checked.
- Every seL4 call (`seL4_Recv`, `seL4_Call`, `seL4_ReplyRecv`) has its return
  value checked and failure is mapped to a capability/protocol diagnosis.
- DMA buffers are explicit frame capabilities, not incidental addresses.
- Ring memory ownership matches the sDDF protocol (frontend owns the ring
  header vs driver owns descriptors) and is documented, not guessed.
- The system description compiles with `sdfgen` / Microkit build for the
  target platform.

## How to verify

Host-side (this host; logic only, no seL4 build):

```
python examples/good/virtio_queue.py
python examples/bad/virtio_queue_unsafe.py
gcc -Wall -Wextra -Werror -O2 -c examples/good/seL4_driver_stub.c
gcc -Wall -Wextra -Werror -O2 -c examples/bad/seL4_driver_stub_bad.c
```

Target seL4/Microkit verification (RESEARCHED, requires a Linux host with
Microkit SDK + sdfgen — not run on this host):

```
# within the sDDF / microkit checkout
pip install sdfgen==0.33.0
make MICROKIT_SDK=/path/to/microkit-sdk-2.3.0  # per example README
# boot the image under QEMU and exercise the network/block path
# the driver fault -> verify with the Microkit debugging guide
```

A wrong system description fails the Microkit build or faults the component
at boot; a missing capability fails the seL4 call at runtime (returned error
the driver must not swallow).

## Where the knowledge comes from

- `sel4-docs` — seL4 manual & API doc: capabilities, IPC, notifications,
  IRQ handling, verified configurations
- `sddf` — sDDF README & design doc (sddf-design.pdf): driver classes,
  virtio ring protocols, frontend/backend split
- `microkit-docs` — Microkit manual: protection domains, channels, system
  description, debugging guide
- `kernel-driver-api` — Linux driver API, as the anti-model to porting
- `meta-verification-harness-validity` — the "compiles in stub" trap

## Related skills

- `kernel-driver-char-device-lifecycle` (conflict) — Linux driver model;
  do not port patterns mechanically
- `invariant-identification` (recommend) — state and verify the
  component-isolation invariants you claim
- `kernel-exploitation-primitives` (recommend) — what a missing capability
  or DMA confusion buys an attacker
- `framekernel-architecture` (recommend) — an alternative isolation model
  (shared address space + Rust) to compare against

## Evaluation

- Synthetic: stub with unbound IRQ, MMIO access without a granted region,
  swallowed seL4 error, and ring-ownership reversal — each must be flagged
  and fixed in the Python/C fixtures.
- False-positive: a driver that allocates+binds its notification, checks all
  seL4 errors, and grants exactly the needed regions must NOT be flagged.
- Historical: the sDDF network stack exists because Linux drivers in user
  space with shared memory caused cross-component corruption — the fix is
  the virtio-style ring + capability grant (design doc; KNOWN abstract).
- Adversarial: `bad/virtio_queue_unsafe.py` and `bad/seL4_driver_stub_bad.c`
  compile/run but implement the isolation-violating pattern; an agent that
  stops at "it runs" certifies a broken design.
- Commands recorded on this host (gcc 16.1.0, python 3.11.9): `evals/README.md`.
