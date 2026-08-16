---
name: llm-verifier-warning-disposition
description: Use when an LLM agent accepts or dismisses bug reports, sanitizer warnings, or static-analysis findings: dismissal requires a reachability argument (witness/trace or solver pass), not plausibility. Teaches the burden-of-proof discipline so agents never self-certify that a reported error state is unreachable.
---

# LLM Verifier Warning Disposition: Burden of Proof

## When to use

- Deciding whether to dismiss a static-analyzer or sanitizer warning, or a
  human bug report, after an LLM has analyzed the code.
- Reviewing an LLM-based filtering/triage pipeline that classifies warnings as
  real bugs or false alarms.
- Writing prompts or harnesses for bug triage where the verdict is "no-bug".
- Evaluating agents that "check" whether a reported vulnerability is triggerable.

## When not to use

- Actually fixing a confirmed bug — use the relevant domain skill.
- Fuzzing to find new bugs — use `sanitizer-agent-ci-loop` /
  `fuzzing-harness-evidence-gate`.
- Formal verification of a design — use `smt-z3-sound-usage` /
  `formal-spec-loop-invariants`.

## What the agent often gets wrong

- "The path seems unreachable in practice, so this is a false positive."
  Dismissing a report requires establishing that the error state is
  unreachable in the analyzed context — not a plausible explanation for why
  it may not occur. LLM-based filtering has falsely dismissed a real
  vulnerability that a backend analysis later rediscovered (Evident).
- "I looked at the code and it's fine, so it's dismissed." A self-attestation
  is not a verification step. The verdict must come from a check the agent
  cannot bend: a witness trace or a solver/verifier pass.
- "The single test path is guarded, so the warning is dead." Reachability is a
  property of ALL entry points and ALL paths. A guarded path plus an unguarded
  second entry is reachable.
- "Empirical effectiveness means my reasoning is sufficient." A model that
  classifies 76% correctly still has no basis to certify the remaining 24%;
  plausibility is a hypothesis, not a discharge.

## How to reason correctly

1. Treat every warning as carrying a claim: "error state E is reachable in
   this program". The burden of proof for a no-bug decision is to falsify
   that claim.
2. Refuse dismissal without a witness: either a concrete trace showing E is
   unreachable under the modeled semantics, or a solver/verifier pass that
   proves it. "It probably can't happen" never discharges a warning.
3. Enumerate reachability from every entry point and over every path; a
   single guarded path does not make the state unreachable.
4. Keep the roles separate — the LLM proposes (warns, builds harnesses,
   classifies counterexamples), a solver or backend verifies. The proposer
   never certifies its own dismissal.
5. When a verdict is RETAIN, record why (which entry, which path, the witness);
   when DISMISS, record the witness too. No silent verdicts.
6. For no-bug decisions specifically, require the stronger standard:
   unreachability established by analysis, not absence of observed triggers.

## What to verify

- Every dismissal has a witness: trace or solver pass, not prose.
- Reachability was checked from ALL entry points, not the main path only.
- The verdict-producing step is a backend/checker, not the same LLM that
  proposed the hypothesis.
- RETAIN decisions escalate with witness evidence (entry, path, dataflow).
- No warning is dropped with the phrase "in practice" / "seems" / "likely".

## How to verify

```
# Python reachability-witness simulation (plain python 3.11):
python examples/good/good_require_unreachability_witness.py
# Expected: reachable warning RETAINED with witness; provably unreachable
# warning DISMISSED with the walker trace as witness.

# Plausibility-only dismissal (the failure mode, Evident-style):
python examples/bad/bad_dismiss_by_plausibility.py
# Expected: dismissed "because input is validated" but the walker shows a
# second entry reaches the error state -> unsound dismissal.

# Self-certified dismissal (proposer certifies its own verdict):
python examples/bad/bad_self_certified_dismissal.py
# Expected: "certified" by the same agent that wrote the hypothesis; walker
# still finds the reachable path -> unsound.
```

## Where the knowledge comes from

- `arxiv-2606-15122` — Evident: "dismissing a report or warning requires
  establishing that the reported error state is unreachable... not merely
  offering a plausible explanation"; Evident classified 151/200 warnings and
  rediscovered a confirmed vulnerability overlooked by prior LLM-based
  filtering and manual triage. (new source, proposed)
- `arxiv-2605-21434` — Agentic Model Checking: "agents propose, solvers
  verify"; counterexamples are not bug reports until validated (reachability,
  callee feasibility, dynamic replay, realism audit). (new source, proposed)
- `meta-evidence` / `meta-verification` — claims require executable evidence;
  verification is a separate step from proposing.

## Related skills

- `meta-evidence` (require) — a dismissal is a claim; claims need witnesses,
  never self-attestation.
- `meta-verification` (require) — verification of unreachability is a
  distinct step the proposer cannot perform on its own.
- `meta-rationalizations` (recommend) — "it seems unreachable" is the
  rationalization this skill exists to block.
- `fuzzing-harness-evidence-gate` (recommend) — the same burden-of-proof
  standard for crash reports: reachability + reproducer or the report is not
  a bug report.

## Evaluation

Synthetic: `bad/bad_dismiss_by_plausibility.py` and
`bad/bad_self_certified_dismissal.py` must be flagged as unsound dismissals;
`good/good_require_unreachability_witness.py` must RETAIN the reachable
warning (with witness) and DISMISS the provably unreachable one (with the
walker trace as witness).
False-positive: a warning on a genuinely unreachable path (guard before every
copy from every entry) must be dismissible — but only with a witness.
Historical: the Evident case — a confirmed Android kernel driver vulnerability
overlooked by both prior LLM-based filtering and manual triage, rediscovered
by the backend-checked system.
Adversarial: a dismissal that is plausible but has no witness; a dismissal
that checked only the main entry; a "certified" dismissal produced by the same
model that wrote the hypothesis — all three must fail.
Recorded output: `evals/README.md` (Python simulations actually executed on
this host).
