---
name: meta-rationalizations
description: Use during code review or self-review to catch and reject rationalizations that excuse unsafe or incorrect low-level code. Contains the "Rationalizations to Reject" list derived from trailofbits and failure modes B1-B22.
---

# Meta: Rationalizations to Reject

## When to use

- Reviewing your own generated code or someone else's C/C++/Rust/asm.
- Whenever you catch yourself justifying a pattern instead of fixing it.

## What the agent often gets wrong

- Adding `unsafe`/`unwrap()`/`#[allow]` at the first compile error instead of fixing the root cause (B1).
- Believing "the borrow checker caught nothing, so it's safe" (B2).
- Using `// SAFETY:` as an infallibility claim rather than an invariant doc (B3).
- Skipping the unsafe-relevant cluster because "unsafe is rare" (B4).
- Claiming "tests pass" as evidence of memory safety (B5).

## Rationalizations to reject (verbatim patterns)

1. "The run partially succeeded — I'll just write REPORT.md from what completed."
   → Hiding partial runs behind a successful report is a correctness bug. (B10/B18)
2. "The borrow checker proves absence of safe-code data races; it proves nothing about
   unsafe blocks, panic reachability, ABBA deadlocks, atomic-load/store sequencing, or FFI
   ABI mismatch." (B2)
3. "`// SAFETY:` documents unsafe operations, not infallibility claims." (B3)
4. "I'll re-derive the cluster list / paths inline instead of running the canonical planner."
   → Paraphrasing loses required fields. (B11)
5. "Background spawns parallelize the workers." → They do not. (B12)
6. "The fuzzer found nothing, so it's clean" → Did it actually run? `-m none` missing =
   silent death = false negative. (AD-12, issue #181)
7. "Compiler won't do that." → If UB, the license allows it. (AD-09)
8. "This ABI is probably the same." → Verify per-OS+compiler. (B14)
9. "It worked on my machine / at -O0." → Platform/opt-level bias. (AD-03/AD-05)
10. "The instruction should exist." → Check the ISA reference. (B2)

## How to reason correctly

1. When a rationalization appears, stop and name it.
2. Replace the rationalization with the fix: solve root cause, verify with the right gate.
3. If a claim cannot be verified now, mark UNVERIFIED — never let confidence substitute for evidence.

## What to verify

- No rationalization from this list appears in your final reasoning.
- Every "it's fine" has a gate behind it (compiler+run+sanitizer+asm) or an explicit UNVERIFIED label.

## When not to use

- During creative/design discussion (not review) — this list is for correctness claims, not ideation.
- When a pattern is genuinely correct — applying this list without evidence is itself a rationalization.

## How to verify

- For each rejected rationalization, confirm the underlying claim against the gate that
  matters (UB taxonomy, ABI doc, asm).
- Re-verify after the fix: the rationalization must be gone, not just moved.

## Where the knowledge comes from

- trailofbits/skills rust-review/c-review "Rationalizations to Reject";
  `roadmap/research-ingestion.yaml` (B1-B22); `registry/evals.yaml`.

## Related skills

- `meta-evidence` — replace rationalizations with evidence labels.
- `meta-verification` — the fix must pass a real gate.

## Evaluation

- The review output is scored on: number of rationalizations correctly rejected, and the
  absence of new rationalizations introduced during the fix.
