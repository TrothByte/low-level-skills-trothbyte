---
name: kernel-patch-review-commit-log-independence
description: Use when reviewing kernel patches: treat the commit log as untrusted, review the diff itself, verify claimed fixes actually touch the vulnerable code, and reject commit-log claims without diff evidence. Teaches diff-first review to counter LLM reviewer bias.
---

# Kernel Patch Review: Commit-Log Independence

## When to use

- Reviewing a kernel patch whose commit message claims to fix a bug, a CVE,
  or an out-of-bounds write — the claim must be checked against the diff.
- Running or evaluating an LLM patch reviewer: is the verdict derived from the
  diff, or from the prose in the commit message?
- Triaging a flood of "looks good on first impression" patches (the current
  BPF subsystem situation): commit logs are now assumed LLM-written.
- Deciding whether a patch is safe to apply to stable/mainline trees.

## When not to use

- Writing kernel code itself — use the relevant domain skill (e.g.
  `kernel-uaccess-safety`, `kernel-rcu-memory-barriers`).
- High-level design discussions that are not patch-shaped.
- Reviewing code for runtime bugs unrelated to patches (use
  `debugging-crash-triage-discipline`).

## What the agent often gets wrong

- "The commit log says it fixes the overflow, so the patch is a fix." Sashiko,
  an LLM kernel-patch reviewer, found real problems in a patch set but
  accepted the commit log's bug-fix claims at face value — review bias from
  the log, not the code (LSFMM+BPF 2026).
- "The message is written by the same agent that wrote the code, so it is
  reliable." Kernel maintainers now assume commit logs are LLM-written and
  ignore them, reviewing the patch directly (Starovoitov, BPF subsystem).
- "A plausible narrative is the same as a verified change." A patch that
  renames a variable while claiming to close an out-of-bounds write passes a
  prose-based review and still overflows.
- "The changed lines look fine, so the patch is fine." A diff can fix one bug
  and introduce another; review what the change does, not just what it claims.

## How to reason correctly

1. Read the diff first, completely: every hunk, every changed line, in
   function context. The commit log is background noise, not evidence.
2. Extract the claimed defect from the message (e.g. "out-of-bounds write in
   parse_name") and locate the code that actually causes it.
3. Check that the diff touches that code: the changed line must be the one
   that computes the vulnerable decision (bounds check, length arithmetic,
   index expression) — not a rename, a comment, or an unrelated hunk.
4. Simulate or trace the before/after behavior with an adversarial probe
   (the exact input that triggers the defect): the after-state must close the
   defect, and must not open a new one.
5. If the message claims a fix but the diff is a no-op on the vulnerable code,
   refuse the patch and say why, citing the diff lines.
6. For severity or CVE claims, require the same evidence any bug report needs:
   reachability, a trace, and a reproducible trigger (see
   `llm-verifier-warning-disposition`).

## What to verify

- Every "fixes X" claim in the commit message maps to a changed line that
  actually addresses X.
- The changed line is the vulnerable decision point, not cosmetic code.
- The after-state passes an adversarial probe of the original defect.
- No new defect is introduced by the change (check the surrounding context).
- The review verdict cites diff lines, never commit-message prose.

## How to verify

```
# Python diff-first review simulation (runs with plain python 3.11):
python examples/good/good_diff_first_review.py
# Expected: real fix ACCEPTED with diff evidence; no-op diff REFUSED;
# refactor without a fix claim -> NO_CLAIM.

# Commit-log-credulous reviewer (the failure mode, Sashiko-style):
python examples/bad/bad_commit_log_review.py
# Expected: a no-op-diff patch is ACCEPTED purely on the message.

# C fixtures (host C sanity, gcc 16.1.0):
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_review_fixture.c
gcc -Wall -Wextra -Werror -O2 -c examples/bad/bad_plausible_commit_log.c
# Both compile (a compile is NOT a review); the diff-first review rejects
# bad_plausible_commit_log.c because the changed line is a rename, not a check.
```

## Where the knowledge comes from

- `lwn-1073583` — Reviewing kernel patches with LLMs (LSFMM+BPF 2026): Sashiko
  is "biased by the commit log"; a commit log saying the patch fixed real
  bugs was "accepted at face value". (new source, proposed)
- `lwn-1075067` — BPF in the agentic era (LSFMM+BPF 2026): Starovoitov
  ignores the commit log on the assumption it is LLM-written and reads the
  patches directly; low-quality patches "look, on first impression, really
  good". (new source, proposed)
- `kernel-coding-style` — kernel patch conventions: reviewers judge the diff.

## Related skills

- `meta-evidence` (require) — a commit message is a claim; claims need
  evidence from the diff, never self-attestation.
- `meta-rationalizations` (recommend) — "the log says it's fixed" is a
  rationalization when the diff shows otherwise.
- `llm-verifier-warning-disposition` (recommend) — severity and CVE claims in
  a commit message require the same reachability evidence as bug reports.
- `kernel-uaccess-safety` (recommend) — the class of defects commit messages
  most often overclaim fixing.

## Evaluation

Synthetic: `bad/bad_plausible_commit_log.c` (message claims a bounds fix, diff
only renames) and `bad/bad_commit_log_review.py` (message-credulous verdicts)
must be rejected by the diff-first process; `good/good_review_fixture.c` and
`good/good_diff_first_review.py` (real bounds fix with diff evidence) must be
accepted.
False-positive: a refactor with no fix claim (NO_CLAIM path) and a genuine fix
that also touches an unrelated line must NOT be refused.
Historical: the Sashiko commit-log-bias incident (lwn-1073583) and
Starovoitov's "ignore the commit log" policy (lwn-1075067) are the documented
failure and the documented countermeasure.
Adversarial: a commit message that claims a fix and includes a "diff" whose
only change is a comment or a variable rename must be caught; a message with no
fix claim but a real security fix inside the diff must still be reviewed.
Recorded output: `evals/README.md` (Python simulations and gcc fixture
compiles actually executed on this host).
