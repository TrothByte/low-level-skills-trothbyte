---
name: invariant-identification
description: Use when writing verification harnesses (Kani, CBMC, Frama-C) or asserting program properties. Teaches extracting real invariants from code, building inductive loop invariants, and avoiding non-inductive or vacuous assertions that pass but prove nothing.
---

# Invariant Identification for Formal Verification

## When to use

- Writing Kani proof harnesses (`#[kani::proof]`), CBMC harnesses, or
  Frama-C/ACSL annotations and needing the invariants to state.
- Reviewing a "verified" function: is the claimed invariant actually true,
  actually inductive, and actually checked?
- Extracting pre/postconditions and loop invariants from existing unsafe
  Rust or C code before modifying it.
- Asserting data-structure or API invariants in security-sensitive code
  (e.g. `invariant_identification` for crypto or driver code).

## When not to use

- Testing behavior (input/output examples) without proof obligations — use
  `fuzzing-harness-kernel` or normal tests.
- Concurrency invariants (lock ordering, happens-before) — use
  `data-race-kernel-detection` / `deadlock-kernel-prevention`.
- Writing the formal proof itself inside a specific tool ecosystem beyond
  harness shapes — the tool's own docs apply (`kani-docs`, `cbmc-docs`).
- Zero-knowledge claims with no executable artifact — use
  `meta-verification-harness-validity`.

## What the agent often gets wrong

- Identifies an invariant by reading the code and asserting it, without
  checking the two conditions that make it provable: it must hold on entry
  to the loop (base) and be preserved by every iteration (step). A property
  that only holds at the top of the first iteration is not an invariant
  (B2).
- States an invariant that is vacuously true ("x is an integer") or too
  weak ("i <= n" when the loop can overshoot) — it passes the harness and
  proves nothing.
- Writes the invariant to match the code's *current* behavior instead of the
  *required* behavior: if the code is wrong, the "invariant" is a
  rationalization of the bug, not a proof (meta-rationalizations).
- Forgets the loop's exit condition: `P && !C => post` is a third obligation.
  Verifying `P` inside the loop and claiming the postcondition holds
  afterwards skips it.
- Assumes Kani/CBMC check what was written: a harness whose assertion is
  unreachable, or whose target is `#[cfg(not(kani))]`-shadowed, runs green
  and certifies nothing (meta-verification-harness-validity).
- Treats a function-local invariant as an API contract: the caller may pass
  arbitrary values, so the function must either handle them or be called
  only under a documented precondition.

## How to reason correctly

1. State the property you actually need, in terms of inputs and outputs
   ("total stays <= cap for every input sequence of length n").
2. For each loop: write the candidate invariant `P`, then check three
   obligations mechanically — P on entry, P preserved by each iteration,
   and `P && !exit` implies the desired postcondition.
3. If any obligation fails, either weaken/strengthen P or fix the code; an
   invariant you have to lie about to preserve is a bug, not a proof.
4. Distinguish the precondition (what the caller guarantees) from the
   invariant (what holds inside) and record both in the harness.
5. Make the harness executable: the assertion must be on a path the harness
   actually executes, for concrete inputs, AND the tool must be run
   (`cargo kani`, `cbmc`, `frama-c -wp`) — not assumed.
6. For induction over data structures, include the structure invariant in P
   (e.g. sortedness, balance, capacity), not just the numeric bounds.

## What to verify

- P holds at loop entry for the harness's initialization.
- P is preserved by every loop-body path (test by asserting P inside the
  body and at each back-edge).
- `P && !exit` implies the postcondition (the loop's contract matches the
  function's contract).
- The harness assertion is reachable for the given concrete inputs.
- The tool actually ran: `cargo kani` / `cbmc` output recorded, not just the
  source annotated.

## How to verify

Host compile + run of the invariant logic (no Kani/CBMC installed on this
host; fixtures carry the harness shape):

```
rustc -O examples/good/inductive_invariant.rs -o /tmp/inv.exe && /tmp/inv.exe
rustc -O examples/bad/non_inductive_invariant.rs -o /tmp/invb.exe && /tmp/invb.exe
gcc -Wall -Wextra -Werror -O2 examples/good/loop_invariant_c.c -o /tmp/linv.exe && /tmp/linv.exe
gcc -Wall -Wextra -Werror -O2 examples/bad/loop_invariant_bad_c.c -o /tmp/linvb.exe && /tmp/linvb.exe
```

Target verification (RESEARCHED; toolchain absent on this host):

```
# Kani
cd <crate with #[kani::proof] harness>
cargo kani --harness check_invariant
# CBMC
cbmc loop_invariant_c.c --function check_loop --unwind 8 --bounds-check
# Frama-C
frama-c -wp -wp-prover why3 loop_invariant_c.c
```

## Where the knowledge comes from

- `kani-docs` — proof harness, kani::assume/assert, reachable checks
- `cbmc-docs` — loop unwinding, bounds/pointer checks
- `acsl-spec` — requires/ensures/loop invariants, assigns clauses
- `frama-c-docs` — WP plugin: how loop invariants become proof obligations
- `rust-reference` — Rust semantics for the harness code
- `meta-verification-harness-validity` — reachable, target-sensitive harness
- `arxiv-2607-00107` — AI verification that "looks rigorous" without
  meaningful properties

## Related skills

- `meta-verification-harness-validity` (require) — the harness that checks
  an invariant must be able to fail when the invariant breaks
- `formal-spec-loop-invariants` (recommend) — same domain, deeper spec
  writing
- `smt-z3-sound-usage` (recommend) — when invariants are discharged by SMT
- `safe-low-level-from-scratch` (recommend) — where invariants keep unsafe
  code honest
- `kernel-exploitation-primitives` (recommend) — the invariant a security
  fix must restore (e.g. "refcount cannot wrap")

## Evaluation

- Synthetic: `bad/non_inductive_invariant.rs` and
  `bad/loop_invariant_bad_c.c` — a non-inductive or too-weak invariant whose
  harness passes; the agent must name the failing obligation (base/step/
  post) and strengthen the invariant.
- False-positive: `good/inductive_invariant.rs` and
  `good/loop_invariant_c.c` — genuinely inductive invariants with checked
  base/step/post must not be flagged.
- Historical: AI-generated C++ that "passes verification" while violating
  the actual property (arxiv-2607-00107, Illusion of Safety); the fixture
  shape is the Kani-harness twin of that failure.
- Adversarial: a harness whose `kani::assert` is gated behind `cfg(not(kani))`
  runs green on the host — compiles-but-wrong, must be detected.
- Commands recorded on this host (gcc 16.1.0, rustc 1.97.1, python 3.11.9):
  `evals/README.md`.
