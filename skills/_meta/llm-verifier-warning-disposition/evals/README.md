# Evaluation — llm-verifier-warning-disposition

Skill: `skills/_meta/llm-verifier-warning-disposition`.
Stability: `researched` (source-backed grounding: arxiv-2606-15122,
arxiv-2605-21434 — both abstracts fetched and verified 2026-08-17). The
burden-of-proof mechanism is demonstrated with a self-contained Python 3.11
simulation actually executed on this host; no static-analyzer or BMC toolchain
is required for the core claims. Mark: SIMULATED — models reachability
reasoning, not a real analyzer/SMT backend.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| negative (plausibility) | `bad/bad_dismiss_by_plausibility.py` | plausible dismissal WITHOUT witness; ground truth shows reachability via 2nd entry -> UNSOUND | RUN on host |
| negative (self-certify) | `bad/bad_self_certified_dismissal.py` | proposer certifies its own dismissal -> UNSOUND (roles must be separated) | RUN on host |
| positive (retain) | `good/good_require_unreachability_witness.py` | reachable warning RETAINED with witness (parse_async trace) | RUN on host |
| positive (dismiss-with-witness) | `good/good_require_unreachability_witness.py` | provably unreachable warning DISMISSED with walker trace as witness | RUN on host |

## False-positive evals (correct code must NOT be flagged)

- A warning whose error state is provably unreachable from EVERY entry (guard
  before every copy, as in `parse_trusted`) must be dismissible — but only
  with the walker trace as witness.
- A guarded main path must NOT be treated as proof of unreachability; the
  check must walk all entries.
- An UNKNOWN case (the model cannot decide) must be escalated or refined, not
  silently dismissed (per arxiv-2605-21434, modelling artifacts feed
  refinement).

## Historical evals

- The Evident case (arxiv-2606-15122): a confirmed Android kernel driver
  vulnerability was overlooked by BOTH prior LLM-based filtering and manual
  triage, and rediscovered by Evident's backend-checked pipeline. This is the
  historical incident motivating the "unreachability, not plausibility" rule.
- Evident calibration from the same paper: 151/200 warnings (76%) correctly
  classified, 111 false alarms discharged with NO confirmed bug discharged,
  remaining cases conservatively retained — the no-bug decisions were only
  sound because a backend, not plausibility, discharged them.

## Adversarial evals

- A dismissal that is plausible but has no witness must fail (fixture 1).
- A dismissal that checked only the main entry (missing `parse_async`) must
  fail.
- A "certified" dismissal produced by the same model that wrote the
  hypothesis must fail (fixture 2) — separation of proposing and verifying is
  the requirement.
- A counterexample presented as a finished bug report without validation
  (reachability / feasibility / replay) must be returned for validation, not
  accepted or dropped.

## Verification commands

```
python examples/good/good_require_unreachability_witness.py
python examples/bad/bad_dismiss_by_plausibility.py
python examples/bad/bad_self_certified_dismissal.py
```

Target (documented-as-target, not executed here): a real static analyzer
(coccinelle/smatch on Linux) feeding warnings to a BMC backend (CBMC/Kani) per
arxiv-2605-21434's BMC-Agent design; the no-bug gate is the backend's
harness-relative unreachability result.

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| python 3.11.9 runs all three fixtures with expected verdicts | VERIFIED (executed 2026-08-17) | output below |
| reachability walker RETAINS via parse_async (n unbounded) and DISMISSES parse_trusted-only with witness | VERIFIED (executed) | output below |
| plausible dismissal and self-certified dismissal both contradicted by ground-truth walker | VERIFIED (executed) | output below |
| "dismissing a report... requires establishing that the reported error state is unreachable" | KNOWN (abstract fetched) | arxiv-2606-15122 |
| Evident: 151/200, 111 FAs discharged, no confirmed bug discharged; rediscovered overlooked vuln | KNOWN (abstract fetched) | arxiv-2606-15122 |
| "agents propose, solvers verify"; counterexamples need validation pipeline | KNOWN (abstract fetched) | arxiv-2605-21434 |

### Host run (python 3.11.9, executed 2026-08-17)

`python examples/good/good_require_unreachability_witness.py`:

```
analyzing overflow-at-copy warnings (buffer cap 32)
  entry parse_http:    n = read() -> [0, inf); guard n<=16 -> [0,16]; copy n.hi=16 <= 32: safe
  entry parse_async:   n = read() -> [0, inf); copy n.hi=inf > 32: OVERFLOW REACHABLE
  entry parse_trusted: n = read() -> [0, inf); guard n<=8 -> [0,8]; copy n.hi=8 <= 32: safe
  -> RETAIN  (error state reachable via: parse_async)
  ...
  -> DISMISS (error state unreachable from every entry)
     witness: walker traces above show n.hi <= cap on all paths
```

`python examples/bad/bad_dismiss_by_plausibility.py` and
`python examples/bad/bad_self_certified_dismissal.py`: both produce a DISMISS
verdict while the ground-truth walker reports `entry 'parse_async' reaches
copy with n.hi=inf > cap=32 -> OVERFLOW REACHABLE`, i.e. the dismissal is
unsound in both cases (see full transcript in the Skill-4 directory).

## Scoring (for routing eval)

- recall: plausible-only and self-certified dismissals detected as unsound.
- precision: witness-backed DISMISS of genuinely unreachable warnings is
  allowed.
- FP-rate: no false flags on the parse_trusted-only case (correctly
  dismissed, with witness).
