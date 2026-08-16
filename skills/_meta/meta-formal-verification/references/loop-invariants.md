# Meta-Formal-Verification: Loop Invariants — Reference Rules

Knowledge layer for `meta-formal-verification`. RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. An invariant is a triple: base, step, and implication

- **RULE**: a correct loop invariant satisfies (a) it holds before the first
  iteration (base), (b) if it holds before an iteration it holds after
  (inductive step), and (c) when the loop exits, it implies the postcondition.
  All three are required; a verifier accepting an invariant that fails (b) or
  (c) proves nothing.
- **WHY AI GETS IT WRONG**: agents write an invariant that is just a
  restatement of the loop body or a fact that is true only at the end
  (not inductive), and the verifier — or the agent's own reading — accepts
  it. This is the vacuous-invariant failure LiveFMBench measures.
- **CORRECT REASONING**: write the invariant as the state fact that is
  preserved every iteration and connects the loop variables to the
  postcondition. Then check the three conditions separately, ideally with the
  tool's proof obligations.
- **EXAMPLE** (bad): `bad/vacuous_invariant.py` in this skill uses "i >= 0"
  as the invariant of a summation loop — true at every point, implies
  nothing about `sum`.
- **COUNTEREXAMPLE** (good): `good/invariant_checker.py` uses
  `sum == i*(i+1)//2` — inductive, and at exit (i == n) implies the
  postcondition `sum == n*(n+1)//2`.
- **VERIFICATION**: `python examples/good/invariant_checker.py` records the
  base/step/implication checks; swap in the weak invariant and observe the
  step check fail.
- **SOURCE**: acsl-spec (loop invariant); arxiv-2511-06552 (only 16% of
  invariant repairs succeed — invariants are genuinely hard);
  arxiv-2605-01394.

## 2. Vacuity: a true-but-useless invariant passes the verifier

- **RULE**: a vacuous invariant (true for trivial reasons, e.g. `i >= 0`,
  `true`, or a fact about unconstrained state) is accepted by verifiers yet
  implies nothing. The test of an invariant is not "does the verifier
  accept" but "does it imply the postcondition, and is it inductive".
- **WHY AI GETS IT WRONG**: the verifier's PASS is read as certification,
  and the vacuity is invisible in the green banner. LiveFMBench reports that
  filtering vacuous/wrong invariants drops model accuracy by ~20 percentage
  points — i.e. much of what models "prove" is vacuous.
- **CORRECT REASONING**: after the verifier accepts, ask: if I remove the
  invariant, does the property fail? If the property is still provable, the
  invariant carried no weight. The implication check (1c) is the vacuity
  test.
- **EXAMPLE** (bad): Kani harness where the assert duplicates the
  precondition — PASS, but the loop's actual postcondition is unproven.
- **COUNTEREXAMPLE** (good): the invariant is the strongest fact needed;
  the postcondition fails without it (demonstrable by deleting it).
- **VERIFICATION**: delete the invariant, re-run, observe failure; restore,
  re-run, observe PASS. That pair is the vacuity test.
- **SOURCE**: arxiv-2605-01394; arxiv-2511-06552.

## 3. Encode the property, then the loop

- **RULE**: for loop-heavy code, first write the postcondition as a
  function-level `ensures`/`assert` (what must be true when the loop ends),
  then derive the invariant from it. Encoding the postcondition wrongly
  guarantees a wrong proof no matter how good the invariant is.
- **WHY AI GETS IT WRONG**: agents encode the property from the "spirit" of
  the code (off-by-one bounds, wrong signedness, ≤ vs <), then the verifier
  faithfully proves the wrong thing and the claim is worse than no proof —
  it is certified wrong.
- **CORRECT REASONING**: state the postcondition in exact notation first
  (e.g. `i == n && sum == n*(n+1)//2` for summing 1..n), check it against a
  few concrete runs, then find the inductive invariant. The postcondition is
  the contract; the invariant is the machinery.
- **EXAMPLE** (bad): a partition/binary-search loop encoded with `<=` where
  the algorithm guarantees `<`; proof passes, claim about the real algorithm
  is false.
- **COUNTEREXAMPLE** (good): the postcondition is tested against a brute
  force oracle (as `good/invariant_checker.py` does) before the formal run.
- **VERIFICATION**: run the implementation against the oracle on many inputs
  before trusting the encoded postcondition.
- **SOURCE**: acsl-spec; arxiv-2605-01394 (specification errors dominate).

## 4. Termination and the loop's exit condition are part of the proof

- **RULE**: for a deductive proof of "the loop always exits", include a
  variant (a strictly decreasing well-founded measure, e.g. `n - i`) and
  prove it decreases each iteration. Bounded model checkers instead require a
  sufficient unwind bound; an unproven loop body after the bound is
  unverified.
- **WHY AI GETS IT WRONG**: agents prove partial correctness (when the loop
  exits, the postcondition holds) and report total correctness (it also
  exits) without the termination argument — an infinite-loop claim is then
  uncertified.
- **CORRECT REASONING**: distinguish "when it exits, result is correct"
  (partial) from "it exits" (termination). State which one is proven. For
  CBMC/Kani the unwind bound is the termination claim's scope.
- **EXAMPLE** (bad): proving `sum == n*(n+1)//2` on exit while the loop
  never increments `i` — the property is provable (if it exits) but it never
  exits; the agent reports "verified".
- **COUNTEREXAMPLE** (good): the invariant includes the variant
  (`i < n` holds while iterating, `n - i` decreases), so both correctness and
  termination are covered.
- **VERIFICATION**: check the variant decreases in the step case; for bounded
  tools, state the unwind bound in the claim.
- **SOURCE**: acsl-spec; cbmc-docs (--unwind); arxiv-2607-20712.

## 5. Tools may be absent; honesty is not optional

- **RULE**: when the formal toolchain is not installed on the host (Kani,
  CBMC, Frama-C, Z3 are NOT on this host), the exact commands are documented
  in `evals/README.md` and the results marked UNVERIFIED. A host-run brute
  force validator can partially stand in (it is exhaustive over a finite
  domain), but that is bounded verification by another name and labeled as
  such.
- **WHY AI GETS IT WRONG**: agents fake the tool run (B6) or claim the
  tool's verdict from documentation. A fabricated proof is the most damaging
  failure mode in this skill — it certifies the wrong thing.
- **CORRECT REASONING**: record "Kani: NOT RUN (not installed); command is
  `cargo kani --harness X`; result UNVERIFIED" plus the brute-force partial
  result. The honest gap is the deliverable.
- **EXAMPLE** (bad): reporting "Frama-C proved the invariant" with no
  frama-c on PATH.
- **COUNTEREXAMPLE** (good): this skill's evals README lists Kani/CBMC/
  Frama-C/Z3 as UNVERIFIED with exact commands, and the brute-force
  `invariant_checker.py` provides the recorded partial evidence.
- **VERIFICATION**: `where kani; where cbmc; where frama-c; where z3` — each
  absent on this host; commands documented for a Linux/installed host.
- **SOURCE**: kani-docs; cbmc-docs; frama-c-docs; z3-docs.
