# seL4 / SDDF: Capability Model for Drivers

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED /
UNVERIFIED.

## 1. Access exists only through capabilities — there is no global address space

- **RULE**: in seL4, every kernel object (memory frame, thread, notification,
  IRQ handler, endpoint) is reachable only through a capability held by the
  invoking thread. There is no `ioremap`-equivalent: device memory must be
  mapped from a frame capability granted to the component, and an access to
  any other address faults. KNOWN (seL4 manual).
- **WHY AI GETS IT WRONG**: agents transfer the monolithic-kernel mental
  model ("pointer to MMIO = permission to touch it") and write drivers that
  dereference globally-declared device addresses. In seL4 the pointer is
  meaningless without the mapping capability.
- **CORRECT REASONING**: capability = authority. Start from "what caps does
  this protection domain hold", then every code access must resolve to one
  of them. A compile that uses a stub `mmap_device(addr)` hides the missing
  capability; only the system description / seL4 error proves the grant.
- **EXAMPLE** (bad): a driver dereferences a device register via a global
  pointer (`volatile uint32_t *reg = (void*)0x3F000000;`) with no mapping
  capability in its CNode. Structurally a fault on seL4.
- **COUNTEREXAMPLE** (good): the component's protection domain is granted the
  MMIO region in the system description; the driver calls the mapping
  syscall with the region capability and checks the returned error before
  first use.
- **VERIFICATION**: inspect the Microkit system description for the region
  grant; run under QEMU and confirm the un-granted variant faults while the
  granted one does not (target command documented in evals; NOT run here).
- **SOURCE**: sel4-docs (Manual: capabilities, mapping) [proposed source].

## 2. Errors from seL4 calls are the only oracle for missing capabilities

- **RULE**: seL4 syscalls return an error code (`seL4_InvalidArgument`,
  `seL4_InvalidCapability`, `seL4_FailedLookup`) when a capability is absent
  or malformed. A driver that ignores return values cannot detect that its
  isolation is broken. KNOWN (seL4 API doc).
- **WHY AI GETS IT WRONG**: kernel drivers historically check no return from
  `request_irq`-style calls; agents copy that habit, so a missing
  notification binding or a failed mapping is silently accepted.
- **CORRECT REASONING**: treat every failed seL4 call as the runtime proof of
  a configuration bug, and fix the configuration, not the check.
- **EXAMPLE** (bad): `seL4_Wait(ntfn, NULL)` on a notification that was never
  bound to the IRQ — waits forever, no error visible, driver hangs.
- **COUNTEREXAMPLE** (good): bind then verify: the driver checks
  `seL4_IRQHandler_SetNotification(irq_cap, ntfn)` return and treats nonzero
  as a fatal config error before entering the wait loop.
- **VERIFICATION**: host stub compiles both variants; on target, the bad
  variant hangs, the good variant receives IRQ notifications (documented).
- **SOURCE**: sel4-docs (API doc: error types, notifications, IRQ) [proposed].

## 3. IRQs are notifications, not interrupt handlers

- **RULE**: on seL4 the driver does not install a handler. It allocates a
  notification object, binds the IRQ to it with the IRQ capability, and
  blocks in a receive loop; the kernel delivers the IRQ as a notification
  on that object. KNOWN (seL4 manual, IRQ control).
- **WHY AI GETS IT WRONG**: the "register a callback" pattern is deeply
  learned; agents invent `irq_install(fn)` or wait on the wrong object.
- **CORRECT REASONING**: the interrupt is a kernel-mediated message: IRQ cap
  → notification → wait loop. Every step is a capability operation.
- **EXAMPLE** (bad): `seL4_Wait(ntfn,...)` where `ntfn` exists but no IRQ was
  ever bound to it — the driver never wakes (see rule 2).
- **COUNTEREXAMPLE** (good): `examples/good/seL4_driver_stub.c` — alloc ntfn,
  bind IRQ cap to ntfn, check error, loop `seL4_Recv`.
- **VERIFICATION**: run the stub logic in the host harness; on target, toggle
  the device and confirm the driver wakes exactly per IRQ (documented).
- **SOURCE**: sel4-docs (Manual: notifications; IRQ control API) [proposed].
