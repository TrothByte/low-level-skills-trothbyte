---
name: framekernel-architecture
description: Use when reviewing or designing framekernel OSes (e.g. Asterinas) or comparing kernel architectures. Teaches the framework/service split in one address space, Rust-only requirement, MMU-based service isolation, and Linux ABI compatibility trade-offs.
---

# Framekernel Architecture (Asterinas model)

## When to use

- Reviewing or contributing to a framekernel OS (e.g. Asterinas) or reading
  the framekernel design documents.
- Comparing kernel architectures (monolithic / microkernel / framekernel /
  unikernel) in a design review.
- Deciding where a new subsystem belongs in a framekernel: privileged
  framework (core, memory, scheduling) vs unprivileged OS service.
- Reasoning about the security model: what the MMU isolates, what the TCB
  contains, and what a single Rust-bug in a service can and cannot reach.

## When not to use

- seL4/Microkit capability-based microkernels — use
  `sel4-sddf-driver-framework` (different isolation model: IPC + caps).
- Linux internals, scheduling, memory management subsystems — use
  `kernel-scheduler-mm-vfs-internals` and `kernel-uaccess-safety`.
- Writing device drivers in a monolithic kernel — use
  `kernel-driver-char-device-lifecycle`.
- Choosing a kernel for a product (that is an architectural decision, not a
  code-review question).

## What the agent often gets wrong

- Calls a framekernel a microkernel. The defining property is the opposite:
  the entire OS runs in ONE address space (like a monolithic kernel); the
  "framework" is not an IPC-separated kernel, it is the privileged core
  within that address space.
- Concludes "single address space = no isolation". Services are isolated
  from each other by MMU page tables even though they share the address
  space; the isolation boundary is privilege + page tables, not IPC.
- Misses the Rust-only constraint. The design's safety argument (memory
  safety inside a shared address space) depends on the whole OS being
  written in Rust; proposing C for a component breaks the model's core
  premise.
- Confuses "Linux ABI compatible" with "Linux kernel". Asterinas runs
  Linux *binaries* (ABI/syscall compatibility) without porting Linux
  internals; the two are different claims.
- Thinks the TCB is only the framework. In the framekernel model the
  framework is privileged; a framework bug compromises the whole system,
  while a service bug is contained by its page-table isolation — the TCB
  boundary is the framework, and that asymmetry is the security point.
- Assumes service-to-service and syscall paths go through IPC like a
  microkernel. They are function calls / shared-memory operations within
  the address space, which is the performance win — and also the reason
  Rust is mandatory.
- Claims formal verification guarantees like seL4. The framekernel papers
  claim memory safety by construction (Rust) plus a small, verifiable
  framework; they do not claim the same whole-system formal proof seL4
  achieved.

## How to reason correctly

1. Establish which component is being discussed: user application, OS
   service (unprivileged), or the framework (privileged core).
2. Map the isolation for that component: what its page tables permit, what
   privilege level it runs at, and what a bug in it can damage (service bug
   = contained; framework bug = total).
3. Trace the path a system call takes end-to-end (app → framework →
   services → back) and note where it is a call vs an MMU transition.
4. Check the language assumption: is every OS-side component the design
   assigns to this architecture written in Rust? A C component requires a
   documented justification or it breaks the safety argument.
5. Compare against the alternatives explicitly: microkernel (IPC cost +
   isolation via caps), monolithic (no isolation, speed), framekernel
   (Rust-safety + MMU service isolation + no IPC), and state which
   trade-off the design actually buys.
6. For review: ask "what is the minimum trusted component, and is the
   boundary enforced (page tables) or just assumed?"

## What to verify

- The component under review is placed on the correct side of the
  framework/service boundary, and the syscall path stays within the design.
- Every OS component is Rust (or carries a documented, accepted exception).
- Service isolation is actually enforced by page tables in the design
  (system description / memory layout), not just stated in prose.
- A claimed "microkernel-like security" claim is checked against the actual
  model (framework in TCB, services isolated).
- ABI-compatibility claims are syscall/ELF level, not "Linux internals".

## How to verify

Host-side (logic models; no framekernel build on this host):

```
rustc -O examples/good/framework_service_split.rs -o /tmp/f1.exe && /tmp/f1.exe
rustc -O examples/bad/framework_service_bad.rs -o /tmp/f2.exe && /tmp/f2.exe
python examples/good/address_space_isolation.py
python examples/bad/address_space_isolation_bad.py
```

Target verification (RESEARCHED; Asterinas build requires a Linux/QEMU
setup, not available here):

```
# per the Asterinas book quickstart:
git clone https://github.com/asterinas/asterinas
cd asterinas
make build
make run                       # boots under QEMU; test Linux binaries
# to validate isolation, add a page-table probe in a service and observe
# the fault on a foreign frame (documented in the book's internals section)
```

## Where the knowledge comes from

- `asterinas-book` — "The Framekernel Architecture": framework vs services,
  single address space, Rust requirement
- `arxiv-2506-03876` — "Asterinas: A Linux ABI-Compatible, Rust-Based
  Framekernel OS": TCB minimization, isolation, ABI compatibility
- `apsys24-framekernel` — "Framekernel: A Safe and Efficient Kernel
  Architecture via Rust-based Virtualization": the original design and
  trade-off argument
- `rust-reference`, `rustonomicon` — the memory-safety guarantees the
  design relies on
- `kernel-uaccess-safety` — user/kernel boundary in the monolithic model,
  for comparison
- `sel4-docs` — the microkernel model this architecture is compared against

## Related skills

- `sel4-sddf-driver-framework` (conflict) — capability/microkernel
  isolation vs in-address-space isolation; compare, do not blend
- `kernel-uaccess-safety` (recommend) — the monolithic boundary this design
  replaces
- `kernel-exploitation-primitives` (recommend) — what a framework bug vs a
  service bug can reach in this model
- `invariant-identification` (recommend) — state the isolation invariant
  ("service can only reach its own frames") that the design must enforce
- `rust-unsafe-reasoning` (recommend) — the unsafe code at the framework
  boundary needs the strongest scrutiny

## Evaluation

- Synthetic: misplacing a component across the framework/service boundary,
  claiming "microkernel isolation" for the framework, proposing a C
  component without justification, and claiming full formal verification —
  each must be corrected against the design model.
- False-positive: correct framework/service placement, MMU-enforced service
  isolation, and Linux-ABI (not internals) claims must NOT be flagged.
- Historical: the framekernel papers position the design against seL4
  (microkernel, formally verified) and Linux (monolithic); the agent must
  reproduce the comparison, including what framekernel does NOT claim.
- Adversarial: `bad/framework_service_bad.rs` and
  `bad/address_space_isolation_bad.py` compile/run while violating the
  isolation model — an agent that accepts them certifies the broken
  boundary.
- Commands recorded on this host: `evals/README.md`.
