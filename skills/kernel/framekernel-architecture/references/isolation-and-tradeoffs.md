# Framekernel: Security Model and Trade-offs

## 1. The TCB is the framework; service bugs are contained by page tables

- **RULE**: because the framework is privileged, any framework bug
  compromises the whole OS; a service bug is contained by the service's
  page-table isolation. The security model therefore concentrates scrutiny
  on the framework and reduces its size — the design goal "small TCB
  without IPC overhead". KNOWN (apsys24-framekernel; arxiv-2506-03876).
- **WHY AI GETS IT WRONG**: agents treat the framework and services as
  equally trusted (or equally untrusted), misassigning which component's
  bugs are critical.
- **CORRECT REASONING**: classify every bug by which half it lands in:
  framework bug = total compromise; service bug = contained unless it can
  cross the page-table boundary (which then is a framework bug).
- **EXAMPLE** (bad): reviewing a service's memory bug as "kernel-level
  critical" without checking what the service's page tables actually grant.
- **COUNTEREXAMPLE** (good): the review traces the service's frames and
  shows the bug is contained, then separately reviews the framework API the
  service could abuse.
- **VERIFICATION**: the address-space fixture computes "damage radius" per
  component from the page-table map.
- **SOURCE**: apsys24-framekernel [proposed]; arxiv-2506-03876 [proposed].

## 2. Performance comes from calls, not IPC — and that is the risk

- **RULE**: service calls and shared-memory access within one address space
  avoid microkernel IPC (no repeated mode switches / copies), which is the
  speed benefit; the cost is that the safety burden moves to Rust's
  guarantees plus page-table discipline. KNOWN (apsys24-framekernel).
- **WHY AI GETS IT WRONG**: the performance claim is accepted without the
  safety consequence, or the safety consequence is used to dismiss the
  design without noting the Rust mitigation.
- **CORRECT REASONING**: both halves of the trade-off must be stated: speed
  (no IPC) is purchased with (a) a language guarantee and (b) page-table
  correctness. Failure of either is a whole-system failure.
- **EXAMPLE** (bad): an unsafe `transmute` in a service that bypasses the
  typed interface and reaches a framework-internal structure.
- **COUNTEREXAMPLE** (good): the service uses only the framework's typed
  calls; `unsafe` blocks are isolated at the framework boundary and
  documented.
- **VERIFICATION**: the Rust fixtures demonstrate the typed-call path vs the
  `unsafe` bypass.
- **SOURCE**: apsys24-framekernel [proposed]; rustonomicon (unsafe
  responsibilities).

## 3. Framekernel does not claim seL4's formal verification

- **RULE**: seL4 provides machine-checked correctness/security proofs of the
  kernel; the framekernel claims memory safety by construction (Rust) and a
  minimal, more-verifiable framework — a different, weaker-sounding but
  distinct guarantee. The papers do not claim whole-system formal proof.
  KNOWN (arxiv-2506-03876 positioning; seL4 verified configurations).
- **WHY AI GETS IT WRONG**: "small TCB like a microkernel" is inflated into
  "formally verified like seL4" when comparing the two.
- **CORRECT REASONING**: state the guarantee class per architecture:
  seL4 = machine-checked proof; framekernel = Rust memory safety + small
  framework + MMU isolation (each with its own verification story).
- **EXAMPLE** (bad): describing the framework as "formally verified" with no
  proof artifact.
- **COUNTEREXAMPLE** (good): the claim is "Rust memory safety plus a
  verifiable small framework", with the appropriate tooling
  (invariant-identification; kani-docs) named for what is actually
  checkable.
- **VERIFICATION**: claims in the review are tagged KNOWN/INFERRED with the
  artifact that would prove them.
- **SOURCE**: apsys24-framekernel [proposed]; sel4-docs (verified
  configurations) [proposed]; arxiv-2506-03876 [proposed].

## 4. Compare against the real alternatives explicitly

- **RULE**: an architectural claim about a framekernel is only meaningful
  relative to the alternatives: microkernel (IPC cost, cap-based isolation,
  small kernel), monolithic (speed, no isolation, C memory bugs),
  framekernel (Rust safety, MMU service isolation, no IPC). Recommending
  "the framekernel is best" without the comparison matrix is not an
  analysis. KNOWN (design literature).
- **WHY AI GETS IT WRONG**: a single dimension (speed or security) is
  optimized in isolation; the trade-off matrix is never drawn.
- **CORRECT REASONING**: for each dimension (TCB size, IPC cost, isolation
  granularity, verification class, ecosystem/ABI), score the three
  architectures and state which dimension the choice optimizes.
- **EXAMPLE** (bad): "use a framekernel because it is fast and secure"
  without stating which guarantee is given up.
- **COUNTEREXAMPLE** (good): the matrix is explicit and the decision states
  the accepted trade-off (e.g. "Linux ABI compatibility at the cost of
  needing the whole OS in Rust").
- **VERIFICATION**: the review artifact contains the matrix; the fixtures
  demonstrate the isolation/performance mechanics per architecture.
- **SOURCE**: asterinas-book [proposed]; arxiv-2506-03876 [proposed];
  kernel-uaccess-safety (monolithic boundary, for comparison).
