# Framekernel: Framework vs Services and the Single Address Space

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED /
UNVERIFIED.

## 1. The whole OS shares one address space; the framework is the privileged core

- **RULE**: in the framekernel architecture the entire OS resides in one
  address space (like a monolithic kernel). It is partitioned into the OS
  Framework — a privileged core (analogous to a microkernel's role) —
  and the OS Services, which are unprivileged. Inter-component communication
  is via shared memory and function calls, not IPC. KNOWN (asterinas-book;
  arxiv-2506-03876).
- **WHY AI GETS IT WRONG**: "microkernel-like security + monolithic speed"
  is misread as "microkernel architecture"; agents then model IPC-mediated
  service calls that the design does not have.
- **CORRECT REASONING**: the address space is shared (no kernel/user
  address-space split between OS parts); the partition is by privilege and
  MMU page tables. Function calls cross the framework/service boundary, not
  message passing.
- **EXAMPLE** (bad): modeling service-to-framework interaction as seL4-style
  IPC with copy-in/copy-out.
- **COUNTEREXAMPLE** (good): `examples/good/framework_service_split.rs` — a
  service calls the framework function directly; access control is a
  privilege token, not a message.
- **VERIFICATION**: `rustc -O` both fixtures; the bad fixture is rejected by
  the good model's access check.
- **SOURCE**: asterinas-book (The Framekernel Architecture) [proposed];
  arxiv-2506-03876 [proposed].

## 2. Isolation exists, but it is MMU page tables, not IPC

- **RULE**: services are isolated from each other and from the framework by
  MMU page-table mappings and privilege levels even though they coexist in
  one address space. A service bug is contained; a framework bug is not.
  KNOWN (apsys24-framekernel; asterinas-book).
- **WHY AI GETS IT WRONG**: "one address space" is equated with "everything
  can touch everything", ignoring the page-table enforcement.
- **CORRECT REASONING**: draw the page-table view per service: which frames
  it may read/write, and what happens on a foreign access (a fault). The
  isolation invariant is "service S can only reach its own frames and its
  granted shared regions".
- **EXAMPLE** (bad): `examples/bad/address_space_isolation_bad.py` — a
  service reads another service's frame with no page-table check.
- **COUNTEREXAMPLE** (good): `examples/good/address_space_isolation.py` —
  the frame lookup enforces the service's mapping; a foreign address faults.
- **VERIFICATION**: `python examples/good/address_space_isolation.py`
  (exit 0); bad variant prints the bypass.
- **SOURCE**: asterinas-book [proposed]; apsys24-framekernel [proposed].

## 3. The whole OS must be Rust

- **RULE**: the safety argument requires the entire OS (framework and
  services) to be written in Rust; memory safety inside a shared address
  space is what makes service isolation by page tables sufficient rather
  than incidental. A non-Rust component needs a documented exception or the
  model's premise fails. KNOWN (asterinas-book).
- **WHY AI GETS IT WRONG**: agents add C for "convenience" (drivers, FFI)
  without noticing they are reintroducing the class of bugs the design
  eliminates.
- **CORRECT REASONING**: any component not covered by Rust's guarantees must
  be treated as an explicit TCB extension with its own safety case.
- **EXAMPLE** (bad): a driver "just ported from Linux" in C inside the
  framework.
- **COUNTEREXAMPLE** (good): the driver is written in Rust against the
  framework's typed interfaces (the design's intended path).
- **VERIFICATION**: the component inventory in the design review lists the
  language and justification for every OS component.
- **SOURCE**: asterinas-book [proposed]; rust-reference (safety);
  rustonomicon.

## 4. Linux ABI compatibility means binaries, not Linux internals

- **RULE**: the framekernel OS (Asterinas) targets Linux ABI compatibility —
  unmodified Linux binaries run against its syscall layer. It does not port
  Linux kernel internals. These are different claims and different
  verification targets. KNOWN (arxiv-2506-03876; LWN coverage).
- **WHY AI GETS IT WRONG**: "Linux-compatible OS" is read as "Linux kernel
  in Rust"; the syscall/ELF ABI surface is conflated with the implementation.
- **CORRECT REASONING**: verify compatibility at the ABI boundary
  (ELF format, syscall numbers and semantics, errno values), not by
  matching internal subsystems.
- **EXAMPLE** (bad): claiming the framekernel "reimplements Linux VFS" as
  evidence of compatibility.
- **COUNTEREXAMPLE** (good): a test binary from a distro runs unmodified;
  the syscall layer returns Linux-compatible results.
- **VERIFICATION**: `make run` under QEMU executing standard Linux binaries
  (documented target command).
- **SOURCE**: arxiv-2506-03876 [proposed]; asterinas-book [proposed].
