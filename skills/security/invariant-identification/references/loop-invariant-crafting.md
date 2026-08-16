# Invariant Identification: Crafting Loop and Data-Structure Invariants

## 1. Write the loop invariant from the loop's contract, backward

- **RULE**: given a loop with exit condition C and goal G, a correct
  invariant P satisfies `P && !C => G`. The practical method is to write
  P as a "loop-relative" property: what is true about the progress
  variables after k iterations (e.g. "total == sum of the first i
  processed elements"), then verify the three obligations. KNOWN
  (partial correctness).
- **WHY AI GETS IT WRONG**: agents copy textbook invariants instead of
  deriving the one that couples the loop variable to the accumulated state;
  the invariant and the goal never connect.
- **CORRECT REASONING**: connect every loop variable to the accumulation.
  If the invariant cannot express the output, it cannot prove the post.
- **EXAMPLE** (bad): `bad/loop_invariant_bad_c.c` claims `i < N` only; the
  exit post `total == expected` is not implied.
- **COUNTEREXAMPLE** (good): `good/loop_invariant_c.c` uses
  `processed == i && total == partial_sum_i && total <= cap`.
- **VERIFICATION**: host asserts on entry/body/exit; bad fixture passes
  only because its check is gated (masked).
- **SOURCE**: acsl-spec; frama-c-docs; cbmc-docs.

## 2. Structure invariants belong in the invariant

- **RULE**: for data structures (sorted arrays, balanced trees, bounded
  buffers, non-empty invariants), the structural property must be part of P
  and preserved by every mutating operation. Verifying numeric bounds alone
  leaves structure bugs unproven. KNOWN (Dafny/Kani/CBMC practice).
- **WHY AI GETS IT WRONG**: the agent checks `len <= capacity` and calls
  the buffer verified, while the head/tail indices can still cross.
- **CORRECT REASONING**: list the structural axioms (0 <= head < capacity,
  entries between head and tail are live, etc.) and preserve each one in
  the operations.
- **EXAMPLE** (bad): a ring buffer "verified" for capacity but not for
  head/tail ordering.
- **COUNTEREXAMPLE** (good): the buffer invariant includes
  `entries_live == (tail - head) mod capacity` and is checked after each
  op.
- **VERIFICATION**: mutate one structural axiom and confirm the invariant
  assert trips.
- **SOURCE**: kani-docs; cbmc-docs; acsl-spec.

## 3. Invariants describe required behavior, not current (buggy) behavior

- **RULE**: if the code has a bug, "the invariant that matches the code"
  is a description of the bug. The invariant must express the specification;
  when P fails to hold, the code is wrong — not the invariant. This is the
  proof-domain twin of meta-rationalizations.
- **WHY AI GETS IT WRONG**: agents infer invariants from the code they are
  about to verify, so a wrong function gets a "correct" invariant that
  rubber-stamps the wrongness.
- **CORRECT REASONING**: derive P from the specification (or from a
  reference implementation), then run the checker on the code. Discrepancy
  means fix the code.
- **EXAMPLE** (bad): a `refcount` helper whose "invariant" is
  "refcnt wraps to 0 sometimes" — mirroring the bug
  (cf. kernel-exploitation-primitives).
- **COUNTEREXAMPLE** (good): the invariant "refcnt >= 1 while the object is
  live" fails on the buggy code and forces the fix.
- **VERIFICATION**: the fixtures' ablations (break the target, invariant
  trips) are the executable form.
- **SOURCE**: meta-rationalizations; arxiv-2607-00107; kani-docs.

## 4. Data-structure induction: put the structure property in the proof

- **RULE**: recursive structures (lists, trees) need inductive invariants
  over their shape (e.g. sortedness of every subtree), not just over depth
  counters. The induction hypothesis is the structural property applied to
  the children. KNOWN (formal-methods practice; Kani supports recursive
  reasoning with bounded unwind).
- **WHY AI GETS IT WRONG**: agents check per-node properties and claim the
  whole structure verified; a subtree-local invariant is not global.
- **CORRECT REASONING**: state the property recursively (all descendant
  nodes satisfy Q), prove it for the base shape, and show each operation
  preserves it for the whole shape.
- **EXAMPLE** (bad): a BST "verified" because every node compares correctly
  with its immediate parent — misses a left-subtree ordering violation.
- **COUNTEREXAMPLE** (good): the invariant is "for every node, all keys in
  the left subtree < key < all keys in the right subtree", asserted after
  each mutation.
- **VERIFICATION**: mutation fixture that breaks left-subtree ordering trips
  the assert.
- **SOURCE**: kani-docs; cbmc-docs (recursion/unwind); acsl-spec.
