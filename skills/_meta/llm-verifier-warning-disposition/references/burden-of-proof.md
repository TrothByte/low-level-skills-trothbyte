# Burden of Proof for Warning Disposition — Rules

Format per rule: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE.

## 1. Dismissal requires establishing unreachability, not plausibility

- **RULE**: No-bug decisions require showing that the reported error state is
  unreachable in the program context being analyzed. A plausible explanation
  of why the bug "may not occur" is not a discharge.
- **WHY AI GETS IT WRONG**: LLMs are rewarded for fluent rationales; a
  well-written "this can't happen because the only caller validates the input"
  is a plausible story, and the model treats narrative coherence as proof. The
  Evident paper documents the consequence: LLM-based filtering falsely
  dismissed a real vulnerability (later rediscovered by a backend-checked
  system).
- **CORRECT REASONING**: the warning carries a positive claim (error state E
  is reachable). A no-bug verdict falsifies that claim. Falsification requires
  a witness: a trace over the modeled semantics showing every path to E is
  blocked, or a solver/verifier pass returning safe. Absence of observed
  triggers proves nothing.
- **EXAMPLE** (bad): dismissing a buffer-overflow warning in `copy(buf, src,
  n)` with "the main parser validates n <= 16" while a second entry point
  calls `copy` with an unvalidated `n`.
- **COUNTEREXAMPLE** (good): before dismissing, the reachability walker
  enumerates all entries; the second entry reaches `copy` with `n` unbounded,
  so the warning is retained with a witness.
- **VERIFICATION**: run the reachability walker over all entries
  (`examples/good/good_require_unreachability_witness.py`) and require a
  witness for every DISMISS.
- **SOURCE**: arxiv-2606-15122 (Evident), abstract: "dismissing a report or
  warning requires establishing that the reported error state is unreachable
  in the program context being analyzed, not merely offering a plausible
  explanation for why it may not occur."

## 2. The proposer never certifies its own dismissal

- **RULE**: The agent that formulates the bug hypothesis must not be the
  authority that discharges it. Verification is a separate step with its own
  semantics ("agents propose, solvers verify").
- **WHY AI GETS IT WRONG**: in a single agent loop, the same model writes "I
  checked all paths" and believes it; there is no independent check to
  disagree.
- **CORRECT REASONING**: separate the roles. The LLM proposes candidate bugs,
  builds analysis harnesses, classifies counterexamples, and proposes
  refinements; a backend (model checker, solver, or an executable reachability
  analysis) discharges every soundness-relevant decision. A self-attestation
  is not a verification step.
- **EXAMPLE** (bad): "I have reviewed the code and the warning is a false
  positive" produced by the same run that proposed the hypothesis.
- **COUNTEREXAMPLE** (good): the dismissal is produced by the walker/solver,
  and the LLM only narrates the witness.
- **VERIFICATION**: require the verdict to cite a check that is not the
  proposing model's prose (trace output, solver result).
- **SOURCE**: arxiv-2605-21434 (Agentic Model Checking): "the principle
  agents propose, solvers verify: agents handle tasks requiring semantic
  judgment... while BMC discharges every soundness-relevant decision."

## 3. Reachability is a property of all entries, not the main path

- **RULE**: An error state is reachable if ANY entry point has a path to it.
  Guarding the main entry does not make the state unreachable.
- **WHY AI GETS IT WRONG**: agents model "the" program as the primary
  call path and reason about that one; secondary entries, callbacks, and
  async entry points are forgotten.
- **CORRECT REASONING**: enumerate entry points and explore each; the state is
  reachable unless every entry is blocked. The witness for reachability is a
  concrete entry + path.
- **EXAMPLE** (bad): `parse_http` guards the copy; `parse_async` (second
  entry) does not — the overflow is reachable but "looks guarded".
- **COUNTEREXAMPLE** (good): the walker checks both entries and retains the
  warning with the `parse_async` trace.
- **VERIFICATION**: `analyze_fn` over every entry in the model
  (`examples/good/good_require_unreachability_witness.py`).
- **SOURCE**: arxiv-2606-15122 (Evident evaluates 200 real warnings from two
  detectors and re-checks reachability harness-relative).

## 4. Counterexamples are not bug reports until validated

- **RULE**: A candidate bug report (from an LLM or a fuzzer) is a hypothesis.
  It becomes a bug report after a validation pipeline: reachability, callee
  feasibility, dynamic replay, realism audit.
- **WHY AI GETS IT WRONG**: the "finding" is treated as the conclusion, and
  the no-bug side is treated as needing excuses instead of the reverse.
- **CORRECT REASONING**: apply the validation pipeline symmetrically to both
  acceptance and dismissal. Active in-tree crashes are distinguished from
  latent public-API failures; modelling artifacts drive a refinement loop
  rather than being suppressed.
- **EXAMPLE** (bad): a crash reported only on a path with an infeasible
  callee, accepted as a real bug without replay.
- **COUNTEREXAMPLE** (good): the counterexample passes reachability + replay
  before it is escalated, or is refined instead of dropped.
- **VERIFICATION**: model the validation steps in the eval; both accept and
  dismiss paths must record them.
- **SOURCE**: arxiv-2605-21434 ("Counterexamples are not bug reports: they
  pass through a validation pipeline (reachability, callee feasibility,
  dynamic replay, realism audit)"). 
