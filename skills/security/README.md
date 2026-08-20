# security — Skills

Security at this level is about constant-time, formal specs, and SMT solvers.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `binary-hardening-flags` | Use when building or reviewing binaries for security — RELRO, BIND_NOW, PIE/ASLR, stack canaries, FORTIFY, CFI, CET, PAC/BTI, shadow stack. Teaches which compiler/linker flags produce which binary properties and how to verify them instead of assuming defaults. | common | source-backed | `skills/security/binary-hardening-flags` |
| `capability-based-security` | Use when designing or reviewing access control that must avoid ambient authority — capability systems (seL4 cspace, CHERI capabilities, object capabilities), delegation, confinement, revocation, and least privilege. Teaches what makes a true capability, how to audit capability flows, and how to design unforgeable, revocable, confined grants. | unique | researched | `skills/security/capability-based-security` |
| `formal-spec-loop-invariants` | Use when writing or reviewing formal specifications, ACSL contracts, Kani annotations, or CBMC/Frama-C proofs of C/Rust loops: loop invariants, pre/postconditions, and vacuous specifications. Prevents vacuous or wrong invariants that pass provers and prove nothing, and inheriting implementation bugs into specs. Requires checking inductiveness and implication, not just prover success. | unique | researched | `skills/security/formal-spec-loop-invariants` |
| `formal-verification-kani-verus` | Use when deciding to formally verify Rust or C code — writing Kani proof harnesses, Verus proofs, CBMC/Frama-C specs, loop invariants, and interpreting results without overclaiming. Teaches what model checkers and SMT-based provers actually prove, how to write non-vacuous harnesses, and how to tell "no counterexample" from "verified". | unique | researched | `skills/security/formal-verification-kani-verus` |
| `invariant-identification` | Use when writing verification harnesses (Kani, CBMC, Frama-C) or asserting program properties. Teaches extracting real invariants from code, building inductive loop invariants, and avoiding non-inductive or vacuous assertions that pass but prove nothing. | unique | researched | `skills/security/invariant-identification` |
| `post-quantum-crypto-mlkem` | Use when selecting or implementing post-quantum cryptography — ML-KEM (Kyber), ML-DSA (Dilithium), hybrid key exchange, or reviewing constant-time rejection sampling and decapsulation-failure handling. Prevents misuse that opens chosen-ciphertext or side-channel holes. | unique | researched | `skills/security/post-quantum-crypto-mlkem` |
| `secure-boot-chain` | Use when implementing or reviewing UEFI Secure Boot, shim, MOK enrollment, kernel/module signature verification, or Android Verified Boot 2.0. Covers the chain of trust, PK/KEK/db/dbx databases, vbmeta and rollback indexes, and signature verification flows. | unique | researched | `skills/security/secure-boot-chain` |
| `side-channel-constant-time-verification` | Use when writing or reviewing cryptographic or security-critical code that handles secrets: timing-leak audits, constant-time compares, secret-indexed lookups, division-timing. Prevents early-exit memcmp, secret-derived branches and indices, and value-dependent division in C, C++, and Rust. Requires measuring timing, not just reading source. | improved | source-backed | `skills/security/side-channel-constant-time-verification` |
| `side-channel-mitigation` | Use when choosing or reviewing countermeasures against side-channel leaks — constant-time vs masking vs blinding, cache/timing/power/EM channels, speculative-execution leaks (Spectre, Meltdown, MDS), and verifying a mitigation actually closes the channel. Teaches threat-model-driven countermeasure selection and evidence-based verification. | improved | source-backed | `skills/security/side-channel-mitigation` |
| `smt-z3-sound-usage` | Use when checking facts with SMT solvers (Z3, cvc5) or reviewing "prover passed" claims: feeding axioms to Z3, reading sat/unsat/model() results, and symbolic protocol-verification confidence. Prevents unsound axioms as evidence, "prover passed" overclaims, and solver confidence as correctness. Requires validating axioms against the real system and checking counterexamples. | unique | researched | `skills/security/smt-z3-sound-usage` |
| `symbolic-execution-klee-angr` | Use when analyzing code paths with symbolic execution — KLEE for LLVM bitcode, angr for binaries, test-input generation, path coverage, or vulnerability exploration. Teaches state forking, path explosion, and soundness limits, distinct from model checking. | unique | researched | `skills/security/symbolic-execution-klee-angr` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
