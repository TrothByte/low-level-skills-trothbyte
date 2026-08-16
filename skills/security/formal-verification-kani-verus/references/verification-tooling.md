# Formal Verification Tooling — Reference

Sources: `kani-docs`, `cbmc-docs`, `frama-c-docs`, `acsl-spec`, `z3-docs`,
`smt-lib`, `arxiv-2605-01394`, `arxiv-2511-06552`, `arxiv-2607-20712`.
The "harness style" logic checks below compile with rustc on this host; Kani
and Verus toolchains are documented as target commands, not run here.

## 1. Kani proof harnesses

- **RULE**: a Kani harness is a `#[kani::proof]` fn that calls the code under
  proof with symbolic inputs from `kani::any()`; `kani::assert!` states the
  property; `kani::assume!` states preconditions. Kani checks panics, OOB,
  arithmetic overflow, and the asserts by default.
- **WHY AI GETS IT WRONG**: a harness with no `kani::assert` (or with only
  literal inputs) is vacuous — "no counterexample" then proves nothing (B7).
- **CORRECT REASONING**: the harness must cover the input space (symbolic)
  and assert the intended property. `assume!` must be weaker than the real
  contract, never stronger.
- **EXAMPLE**: `#[kani::proof] fn check_non_negative() { let x:
  u32 = kani::any(); let y = wrapping_add(x, 5); kani::assert!(y >= 5); }` —
  the assert mentions the symbolic input.
- **COUNTEREXAMPLE**: `#[kani::proof] fn vacuous() { let x = 5;
  let y = x + 1; }` — no assert, one literal input, passes trivially.
- **VERIFICATION**: `rustc` logic stub + `cargo kani --harness ...`
  (target).
- **SOURCE**: `kani-docs` (proof harness, kani::any/assume/assert).

## 2. Bounded vs unbounded proofs

- **RULE**: Kani and CBMC unroll loops up to a bound; the result is *bounded*
  verification. A full proof requires arguing the bound covers all reachable
  states (or using induction / a theorem prover).
- **WHY AI GETS IT WRONG**: reporting "verified" for a `--unwind 5` run
  without mentioning the bound (A10).
- **CORRECT REASONING**: document the bound and the termination argument;
  use Verus (SMT/induction) when the property must hold for all inputs.
- **EXAMPLE**: CBMC `--unwind 10` proves the property for up to 10 loop
  iterations; a loop with a `len` cap ≤ 10 is fully covered.
- **COUNTEREXAMPLE**: claiming a general proof from `--unwind 5` on a
  loop whose bound is data-dependent.
- **VERIFICATION**: read the tool output for the declared bound; `verus
  --verify` for the unbounded claim.
- **SOURCE**: `cbmc-docs` (--unwind); `kani-docs` (unwind attribute).

## 3. Verus: spec/proof separation

- **RULE**: Verus splits the world into `spec fn` (logical functions used in
  specs), `fn` (executable code), and `proof fn` (logical reasoning). A proof
  must be connected: the `proof` establishes `ensures`/`invariant`s that the
  caller can rely on.
- **WHY AI GETS IT WRONG**: proving a property in a proof fn that the
  executable fn never uses — the proof is disconnected (B2).
- **CORRECT REASONING**: state `requires`/`ensures` on the executable
  function; the proof fn (or inline `invariant`) establishes them; the
  property the caller sees is the `ensures`.
- **EXAMPLE**: `fn inc(x: u32) -> (r: u32) ensures r > x { proof { ... } x + 1 }`.
- **COUNTEREXAMPLE**: `proof fn p() { assert(1 == 1); }` with `inc` having no
  `ensures` — nothing is proven about `inc`.
- **VERIFICATION**: `verus --verify file.rs` (target); rustc cannot check
  Verus syntax — documented.
- **SOURCE**: Verus docs (proposed new source); `z3-docs`/`smt-lib`
  (underlying solver).

## 4. Vacuous proofs and invariant quality

- **RULE**: a proof is vacuous if the assumptions/inputs make it true for
  reasons unrelated to the code — no `kani::assert` on the property, or an
  over-strong `assume!`. Invariants must be inductive (hold before and after
  each iteration) and strong enough to imply the postcondition.
- **WHY AI GETS IT WRONG**: LiveFMBench shows LLM-generated invariants are
  often vacuous or wrong (~20% accuracy drop after filtering); only ~16% of
  invariant repairs succeed (arxiv-2511-06552) (B7/A10).
- **CORRECT REASONING**: check three things: (1) the assertion names the
  property; (2) the inputs are symbolic; (3) the invariant is inductive.
- **EXAMPLE**: loop `invariant i <= n` on a 0..n loop — inductive and strong
  enough.
- **COUNTEREXAMPLE**: `invariant true` — inductive but proves nothing.
- **VERIFICATION**: rustc stub + `verus --verify`/`cargo kani`; review the
  counterexample when a proof fails.
- **SOURCE**: `arxiv-2605-01394`; `arxiv-2511-06552`; `arxiv-2607-20712`.

## 5. Tool success vs property truth

- **RULE**: a model checker returning "no bug found" means no counterexample
  within its bounds and model; it does not automatically mean the property
  holds in all contexts (ProVerif confidence study: tool output is
  context-sensitive).
- **WHY AI GETS IT WRONG**: treating tool output as ground truth (B7).
- **CORRECT REASONING**: the proof is only as strong as the model: the
  harness, the bounds, the assumptions, and the abstraction. State what was
  and was not proven.
- **EXAMPLE**: "Kani: no overflow in `wrapping_add` calls under
  `assume!(x < 1000)`" — precise, bounded claim.
- **COUNTEREXAMPLE**: "our crypto is verified (Kani passed)" without stating
  the model.
- **VERIFICATION**: record the exact tool output + bound in CI logs.
- **SOURCE**: `arxiv-2607-20712` (ProVerif confidence study).
