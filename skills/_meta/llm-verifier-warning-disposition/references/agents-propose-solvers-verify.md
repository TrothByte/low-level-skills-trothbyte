# Agents Propose, Solvers Verify — Rules

Format per rule: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE.

## 1. Role separation is the architecture, not a preference

- **RULE**: Agentic verification couples an LLM agent with a formal backend
  under a fixed division of labor: the agent proposes (spec inference, check
  selection, counterexample classification, refinement proposals), the backend
  verifies (every soundness-relevant decision).
- **WHY AI GETS IT WRONG**: single-model pipelines merge proposing and
  verifying into one step, so the model's confidence in its own narrative
  substitutes for a check.
- **CORRECT REASONING**: the backend discharges the decisions that must be
  sound; the LLM's contribution is semantic judgment that cannot be sound or
  unsound by itself. When the two disagree, the backend wins for
  soundness-relevant claims.
- **EXAMPLE** (bad): an agent "verifies" its own static-analysis hypothesis
  by writing a paragraph about why the path is unreachable.
- **COUNTEREXAMPLE** (good): the agent builds a harness; the model checker
  returns UNSAT/safe; the dismissal cites the checker.
- **VERIFICATION**: in the eval, require the verdict line to name the backend
  artifact (walker trace, solver result) — never "my analysis".
- **SOURCE**: arxiv-2605-21434 (Agentic Model Checking): "agents propose,
  solvers verify... while BMC discharges every soundness-relevant decision."

## 2. Specifications are inferred but enforced by the backend

- **RULE**: The LLM may infer specifications (top-down from caller context,
  translated deterministically into assume/assert primitives), but the
  inference is only as good as the backend enforcement of the resulting
  spec. Inference without enforcement is opinion.
- **WHY AI GETS IT WRONG**: "I wrote the spec" is treated as "I proved the
  property"; a vacuous or wrong spec passes the model's self-review.
- **CORRECT REASONING**: the deterministic translation of the spec into the
  backend's primitives is what makes the spec checkable. If the spec cannot
  be enforced by the backend, it is not a spec yet.
- **EXAMPLE** (bad): a dismissal justified by an informally stated invariant
  ("sizes are always small here") with no assume/assert encoding.
- **COUNTEREXAMPLE** (good): the invariant encoded as `assume(n <= 16)` and
  checked against the copy's `assert(n <= 32)` by the backend.
- **VERIFICATION**: encode the guard, run the check, record the outcome.
- **SOURCE**: arxiv-2605-21434 ("Specifications are inferred top-down from
  caller context in a restricted DSL that translates deterministically into
  the backend's assume/assert primitives").

## 3. Compositional checks keep the per-query cost sound

- **RULE**: Each function is checked in isolation against its spec, with
  callees replaced by postcondition-constrained stubs; refinements propagate
  to callers automatically.
- **WHY AI GETS IT WRONG**: agents treat "I read the whole codebase" as the
  strength of their analysis; unbounded context is neither necessary nor a
  correctness argument.
- **CORRECT REASONING**: compositional verification gives the same
  soundness with bounded cost: per-query state space scales with one
  function, and a stub's postcondition carries the caller-relevant contract.
- **EXAMPLE** (bad): a dismissal that requires reasoning over the entire call
  graph by hand, and silently assumes a callee's behavior.
- **COUNTEREXAMPLE** (good): the callee replaced by a postcondition
  constrained stub; the check runs on the single function's state space.
- **VERIFICATION**: model the stub postcondition in the eval fixture and check
  the caller.
- **SOURCE**: arxiv-2605-21434 ("Verification is compositional: each function
  is checked in isolation against its spec with callees replaced by
  postcondition-constrained stubs").

## 4. Modelling artifacts feed refinement; they are not suppressed

- **RULE**: When the backend's model does not match reality (a check is too
  coarse or too fine), that mismatch is a finding that drives a refinement
  loop. Suppressing the artifact to get a verdict is unsound.
- **WHY AI GETS IT WRONG**: a model that "can't model this" or "produces a
  counterexample the agent doesn't understand" is treated as a nuisance, and
  the agent papers over it with prose.
- **CORRECT REASONING**: every discrepancy between the model and the target is
  information. If the model cannot decide, the honest verdict is UNKNOWN, not
  DISMISS.
- **EXAMPLE** (bad): a warning on a path the model cannot reach, dismissed as
  "tool artifact" without refinement.
- **COUNTEREXAMPLE** (good): the path is added to the model, re-checked, and
  either retained (with witness) or dismissed (with witness).
- **VERIFICATION**: the eval includes an UNKNOWN path; the correct handling is
  escalation or refinement, never silent dismissal.
- **SOURCE**: arxiv-2605-21434 ("modelling artifacts drive a refinement loop
  rather than being suppressed").
