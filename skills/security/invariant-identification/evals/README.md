# Evaluation — invariant-identification

Skill: `skills/security/invariant-identification`. Stability target:
`evaluated`. Current stability: `researched` — host fixture runs recorded
(rustc 1.97.1, gcc 16.1.0); Kani/CBMC/Frama-C are NOT installed on this
host, so tool verification is documented, not executed.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/non_inductive_invariant.rs` | invariant i<=n destroyed by i+=2; gated assert | prints PASS (masked), exit 0 |
| easy/negative | `bad/loop_invariant_bad_c.c` | weak invariant + gated check | prints PASS (masked), exit 0 |
| medium/positive | `good/inductive_invariant.rs` | base/step/post all asserted live | prints GOOD, exit 0 |
| hard/positive | `good/loop_invariant_c.c` | invariant preserved incl. saturation | prints GOOD, exit 0 |
| hard/negative | invariant that cannot imply post | must be rejected | see bad fixtures |

Detection rule: every claimed invariant is checked against the three
obligations; a harness whose assertion cannot fail certifies nothing.

## False-positive evals (valid proofs must NOT be flagged)

- `good/inductive_invariant.rs` — P = (processed == i) && (sum <= cap),
  asserted at entry, body, back-edge, exit; inductive and implies the post.
- `good/loop_invariant_c.c` — saturating add preserves sum <= cap on every
  branch; post asserted. No flag.
- A harness with `kani::assume` for the precondition and a reachable
  assertion is valid — do not flag assumes as "holes" when the contract
  documents them.

## Historical evals

- arxiv-2607-00107 (Illusion of Safety): AI-written C++ passed "verification"
  while violating the actual runtime properties (~2x runtime violations).
  The fixture shape is reproduced by the gated `kani_assert` in
  `bad/non_inductive_invariant.rs` — verification that cannot fail.
- arxiv-2606-20128 (Correctness Illusion): fixed-shape oracles that never
  compare to ground truth — the C bad fixture's unfalsifiable check is the
  local twin.

## Adversarial evals

- `bad/non_inductive_invariant.rs` compiles and prints "PASS: harness
  reports verified" while the invariant is false at the loop back-edge
  (i = 4 > n = 3). An agent that accepts the PASS without running the step
  obligation reproduces the failure.
- `bad/loop_invariant_bad_c.c` compiles with `-Werror` and prints PASS —
  compile-clean annotation ≠ proof.
- The good fixtures must survive ablation: breaking the cap-preserving step
  must trip the live asserts.

## Verification commands (host, ACTUAL)

```
rustc -O examples/good/inductive_invariant.rs -o /tmp/inv.exe
  exit 0
/tmp/inv.exe
  prints "GOOD: invariant holds on entry, every back-edge, and exit; sum = 1023"
  exit 0
rustc -O examples/bad/non_inductive_invariant.rs -o /tmp/invb.exe
  exit 0
/tmp/invb.exe
  prints "PASS: harness reports verified (invariant i <= n), sum = 4"
  prints "BAD: the invariant fails at the back-edge ..."   exit 0 (MASKED)
gcc -Wall -Wextra -Werror -O2 examples/good/loop_invariant_c.c -o /tmp/linv.exe
  exit 0
/tmp/linv.exe
  prints "GOOD: invariant asserted at entry, body, back-edge, and exit; sum=1023"
  exit 0
gcc -Wall -Wextra -Werror -O2 examples/bad/loop_invariant_bad_c.c -o /tmp/linvb.exe
  exit 0
/tmp/linvb.exe
  prints "PASS: invariant i < n verified, sum=4" and "BAD: ..."  exit 0 (MASKED)
```

## Verification commands (target, RESEARCHED — not run on this host)

```
# Kani (Rust harness in a crate):
cargo kani --harness check_invariant
# CBMC:
cbmc examples/good/loop_invariant_c.c --function check_loop --unwind 8 --bounds-check
# Frama-C / WP:
frama-c -wp -wp-prover why3 examples/good/loop_invariant_c.c
```

## Verified facts

- rustc 1.97.1 and gcc 16.1.0 compile all four fixtures (KNOWN).
- Good fixtures print GOOD and exit 0; both bad fixtures print their masking
  PASS and exit 0 (KNOWN, recorded above).
- The overshoot fact: with n=3 and i+=2, i visits 0,2,4 — the claimed
  invariant i<=n (or i<n) is false at the final back-edge. KNOWN (arithmetic
  on the fixture).
- Kani/CBMC/Frama-C behavior (unwinding, obligation generation, proof
  verdicts) — INFERRED from kani-docs/cbmc-docs/acsl-spec/frama-c-docs;
  UNVERIFIED on this host (tools absent).

## Scoring

- precision: an invariant is accepted only when base, step, and post
  obligations hold and the harness can fail.
- recall: preconditions, loop invariants, structure invariants, and
  postconditions are each demanded.
- FP-rate: the two good fixtures produce zero flags.
- Strongest single fact: the same sum loop is verified in
  `good/inductive_invariant.rs` (live asserts, exit 0) and "verified" in the
  bad fixture while its invariant is false — the obligation-delta is
  recorded, not assumed.
