# Iteration Strategy on Opaque Feedback — Rules

Format per rule: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE.

## 1. Extract, then minimize, then repair — never rewrite

- **RULE**: The iterate-on-opaque-feedback loop is: (1) extract the failing
  instruction and register state from the log tail; (2) minimize the program
  to the smallest form that preserves the failure; (3) repair minimally;
  (4) re-verify with `bpftool prog load -d`. A full rewrite is never a step.
- **WHY AI GETS IT WRONG**: an opaque dump invites a "start over" response;
  BPF is the only case Starovoitov has observed where LLMs give up rather
  than producing something.
- **CORRECT REASONING**: minimization converts an opaque dump into a tiny
  program where the offending construct is visible; the repair then targets
  exactly that construct, and re-verification is the loop's feedback.
- **EXAMPLE** (bad): an agent rewrites the whole program on a failed load and
  ships a different bug.
- **COUNTEREXAMPLE** (good): delta-debug removes the two no-op `addi`s,
  leaving `data r1; ldw r0,[r1+0]` — the missing bounds check is now
  obvious.
- **VERIFICATION**: `examples/good/good_verifier_log_iteration.py` prints
  each step's result.
- **SOURCE**: lwn-1075067 (BPF as the "give up" case); ebpf-docs.

## 2. Delta-debug minimization preserves the failure signature

- **RULE**: Remove instructions greedily; a removal is kept only if the
  failure signature (failing instruction index + message) is unchanged. The
  result is a minimal failing program.
- **WHY AI GETS IT WRONG**: without minimization, the model reasons about the
  whole program and fixates on unrelated lines.
- **CORRECT REASONING**: the failure signature is the invariant of the
  search; every kept removal must leave it identical. What survives is
  necessary for the failure.
- **EXAMPLE** (bad): trying to fix a packet-access error while a dozen
  unrelated instructions cloud the reasoning.
- **COUNTEREXAMPLE** (good): the minimal program is `[data r1, ldw r0,
  [r1+0], exit]`; the fix is exactly one inserted check.
- **VERIFICATION**: iteration step 2 in the simulation: 5 insns -> 3 insns
  with the same failing instruction.
- **SOURCE**: ebpf-docs (bounded verification loops); technique from standard
  delta-debugging applied to verifier programs.

## 3. Bisect register/pointer state to its creating instruction

- **RULE**: For the failing register, find the instruction that created its
  state. `PTR_TO_PACKET(r=0)` is created by loading `ctx->data` without any
  subsequent bounds establishment; the creator names the missing construct.
- **WHY AI GETS IT WRONG**: the message names the failing access; the model
  fixes the access instead of the missing state.
- **CORRECT REASONING**: the state is created once (e.g. the `data` load) and
  must be enriched by a bounds check before the access. Fix the state, not
  the symptom line.
- **EXAMPLE** (bad): "fixing" `ldw r0, [r2+0]` by changing the offset, when
  the real issue is `r=0` on `R2`.
- **COUNTEREXAMPLE** (good): inserting `r9 = ctx->data_end; if r2 + 8 > r9
  goto exit` before the load; re-verification passes.
- **VERIFICATION**: iteration step 3 in the simulation identifies insn 0
  (`data`) as the state creator.
- **SOURCE**: ebpf-docs: Documentation/bpf/verifier.rst (pointer types and
  ranges); kernel/bpf/verifier.c.

## 4. Verify the fix on the path the verifier sees

- **RULE**: The inserted check must be on the path the verifier analyzes —
  before the failing access, in the same basic block reachable from the
  entry. A check on another branch is invisible.
- **WHY AI GETS IT WRONG**: the model inserts the check "somewhere" and
  declares victory; the verifier still rejects.
- **CORRECT REASONING**: place the check so the failing access's register
  state is enriched on every path reaching it, then re-run the load loop.
  The loop's feedback is authoritative.
- **EXAMPLE** (bad): a bounds check after the load it should guard.
- **COUNTEREXAMPLE** (good): `data_end` comparison immediately before the
  load; `bpftool prog load` succeeds.
- **VERIFICATION**: re-verify after each fix; the loop ends only on a clean
  load.
- **SOURCE**: ebpf-docs; lwn-1075067 (feedback loops are what coding agents
  do well — "they write code, see errors, and make fixes").

## 5. Give-up is a stopping-rule violation, not a verdict

- **RULE**: "The log is too opaque; I cannot proceed" is not an acceptable
  outcome for a BPF task. Persistence on the load loop is the skill.
- **WHY AI GETS IT WRONG**: the same opaque output that frustrates humans
  causes LLMs to emit nothing — the documented BPF-specific failure.
- **CORRECT REASONING**: the loop always has a next step: extract one more
  line, minimize one more instruction, try one more fix, re-verify. Each
  iteration is cheap and deterministic.
- **EXAMPLE** (bad): `bad/bad_give_up_on_opaque_log.py` returns no fix and
  the program still fails.
- **COUNTEREXAMPLE** (good): the good simulation converges on a clean load in
  five printed steps.
- **VERIFICATION**: the eval rejects the give-up script as the negative
  fixture.
- **SOURCE**: lwn-1075067 ("writing BPF code is the only case he has
  observed where LLMs will give up rather than producing something").
