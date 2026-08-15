# security — Skills

Low-level engineering skills for this domain.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `formal-spec-loop-invariants` | Use when writing or reviewing formal specifications, ACSL contracts, Kani annotations, or CBMC/Frama-C proofs of C/Rust loops: loop invariants, pre/postconditions, and vacuous specifications. Prevents vacuous or wrong invariants that pass provers and prove nothing, and inheriting implementation bugs into specs. Requires checking inductiveness and implication, not just prover success. | researched | `skills/security/formal-spec-loop-invariants` |
| `side-channel-constant-time-verification` | Use when writing or reviewing cryptographic or security-critical code that handles secrets: timing-leak audits, constant-time compares, secret-indexed lookups, division-timing. Prevents early-exit memcmp, secret-derived branches and indices, and value-dependent division in C, C++, and Rust. Requires measuring timing, not just reading source. | source-backed | `skills/security/side-channel-constant-time-verification` |
| `smt-z3-sound-usage` | Use when checking facts with SMT solvers (Z3, cvc5) or reviewing "prover passed" claims: feeding axioms to Z3, reading sat/unsat/model() results, and symbolic protocol-verification confidence. Prevents unsound axioms as evidence, "prover passed" overclaims, and solver confidence as correctness. Requires validating axioms against the real system and checking counterexamples. | researched | `skills/security/smt-z3-sound-usage` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
