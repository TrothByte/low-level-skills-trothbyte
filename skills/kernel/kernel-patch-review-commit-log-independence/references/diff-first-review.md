# Diff-First Review — Rules

Format per rule: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE.

## 1. The diff is the object of review

- **RULE**: Review the complete set of changed lines — hunks, context,
  function boundaries — as the primary object. Everything else (message,
  author, urgency, praise) is secondary and must never substitute.
- **WHY AI GETS IT WRONG**: LLM reviewers anchor on the summary and skim the
  code, so a well-written message can carry a sloppy or no-op patch through.
- **CORRECT REASONING**: every verdict must name the changed line it rests on.
  "ACCEPT because hunk 2 adds the bounds guard" is a review; "ACCEPT because
  the message says it fixes a bug" is a summary.
- **EXAMPLE** (bad): verdict "looks correct" for a patch whose only change is
  in an unrelated error path.
- **COUNTEREXAMPLE** (good): verdict citing `+ if (n > dst_cap) return;` at
  the exact call site of the vulnerable copy.
- **VERIFICATION**: delete the message and re-review; the verdict must be
  unchanged. Enforce this in review prompts.
- **SOURCE**: lwn-1073583; lwn-1075067.

## 2. Map the claimed fix to the vulnerable code

- **RULE**: For every claim in the message ("fixes out-of-bounds write in
  parse_name"), identify the code that produces the defect, then require the
  diff to change that code or to add a guard on the path to it.
- **WHY AI GETS IT WRONG**: a message naming a function ("parse_name") and a
  defect ("OOB") is not the same as a diff changing `parse_name`'s vulnerable
  line; the model blurs the two.
- **CORRECT REASONING**: three-part check — (a) locate the defect site from
  the pre-image, (b) require the changed lines to intersect that site or the
  path into it, (c) confirm the change is semantic (guard, arithmetic,
  bounds), not cosmetic (rename, comment, reorder).
- **EXAMPLE** (bad): `n_copy` renamed to `how_many` while `memcpy(dst, src, n)`
  still copies an unbounded `n`.
- **COUNTEREXAMPLE** (good): a `len > dst_cap` guard added immediately before
  the copy, with the copy now provably bounded.
- **VERIFICATION**: `git show` the patch, grep the vulnerable expression in
  the pre/post image, and confirm the guard dominates the copy on all paths.
- **SOURCE**: lwn-1073583 (Sashiko accepted the log's claims instead of
  checking the changes); lwn-1075067.

## 3. Verify with an adversarial probe, not an eyeball

- **RULE**: The after-state must be exercised with the triggering input (or a
  structural simulation of it). A fix that cannot be shown to close the defect
  has not been shown at all.
- **WHY AI GETS IT WRONG**: correctness reasoning about a patch stays verbal;
  a verbal "the check is there" passes a verbal review even when the check
  compares the wrong operands or sits on the wrong path.
- **CORRECT REASONING**: choose the boundary probe (capacity, capacity+1,
  index == count) and evaluate before/after. If the after-state is safe where
  the before-state overflowed, the claim is verified; otherwise the patch
  fails on the probe.
- **EXAMPLE** (bad): a "guard" `if (n < dst_cap)` — off by one — accepts the
  probe `n == dst_cap` and still overflows.
- **COUNTEREXAMPLE** (good): `if (n > dst_cap) return;` — the probe
  `n == dst_cap` is safe, `n == dst_cap + 1` is rejected.
- **VERIFICATION**: run the probe simulation in
  `examples/good/good_diff_first_review.py`; on target, build and run the
  regression test that reproduces the original bug.
- **SOURCE**: lwn-1073583; meta-evidence (claims require executable evidence).

## 4. A fix that introduces a new bug is still a bad patch

- **RULE**: Review the change's consequences on surrounding code, not just the
  claimed fix. The diff's non-claimed effects (changed control flow, dropped
  checks, altered return paths) are part of the review.
- **WHY AI GETS IT WRONG**: the message frames the patch as a single-purpose
  fix, and the reviewer adopts that framing, ignoring side effects.
- **CORRECT REASONING**: ask what else changed and whether any previously-safe
  path became unsafe (e.g. an early return that skips cleanup, a widened
  condition that admits a new value).
- **EXAMPLE** (bad): the new bounds guard returns early without freeing a
  resource the old path always released — a leak introduced by the fix.
- **COUNTEREXAMPLE** (good): the guard returns the same error code the old
  error path used, preserving resource cleanup.
- **VERIFICATION**: compare control-flow changes hunk by hunk; run the
  existing tests that cover the touched paths.
- **SOURCE**: lwn-1073583 (LLM reviews find problems the message did not
  mention — the reverse bias also exists); lwn-1075067.
