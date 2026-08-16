# Agent Scope: Scope Boundaries, Delegation, and Stopping

## 1. One session, one unit; record extras as notes, not as edits

- **RULE**: a session's scope is the acceptance criteria of its unit. Any
  change outside it is deferred (recorded as a note) or rejected. Scope
  creep is a token and correctness tax: out-of-scope edits are the most
  common source of unintended repo modifications. KNOWN (operational rule).
- **WHY AI GETS IT WRONG**: the agent "helps" by fixing nearby problems,
  and the diff grows beyond the assignment; review either misses the extra
  changes or blocks the intended work.
- **CORRECT REASONING**: for every prospective edit, ask "is this required
  by the unit's acceptance criteria?" If no, add it to the note list for a
  future unit.
- **EXAMPLE** (bad): while fixing one skill, the agent rewrites an
  unrelated reference file and edits registry data.
- **COUNTEREXAMPLE** (good): the extra finding is logged as "next-action"
  in the progress file, untouched by this session.
- **VERIFICATION**: `git status` / `git diff --stat` against the declared
  scope (documented command).
- **SOURCE**: repo-internal AGENTS.md (progress files); codex-37653.

## 2. Delegation needs explicit scope and verified outputs

- **RULE**: when a subagent is assigned a piece of work, the prompt must
  state the scope (which files, which domain, which constraints), the
  output contract, and the evidence requirement. The parent must verify the
  returned artifacts, not just the summary. KNOWN (operational rule for
  subagents/Agent Manager sessions).
- **WHY AI GETS IT WRONG**: a vague prompt ("look into the kernel skills")
  yields a session that wanders; its output is accepted on trust and its
  conclusions enter the parent's state unverified.
- **CORRECT REASONING**: give each subagent one bounded deliverable, ask for
  its verification records, and check the artifacts against the scope
  before integrating.
- **EXAMPLE** (bad): a subagent returning "done" with no diff and no
  recorded verification, accepted by the parent.
- **COUNTEREXAMPLE** (good): the subagent returns files + commands + outputs
  that the parent re-runs.
- **VERIFICATION**: the delegation checklist is enforced in the workflow
  fixture's log.
- **SOURCE**: meta-verification; arxiv-2607-00107.

## 3. The stopping rule runs before the context runs out

- **RULE**: when context is ending (or the task is cut short), the agent
  must, in order: (1) save intermediate results, (2) update the progress
  state (current_task, next_action), (3) update the worklog, (4) preserve
  found sources, (5) leave no unwritten knowledge in memory. This order is
  not optional decoration. KNOWN (this repo's stopping rule).
- **WHY AI GETS IT WRONG**: "I'll summarize at the end" is the default;
  interruption pre-empts the summary, losing everything.
- **CORRECT REASONING**: treat the stop as a scheduled checkpoint; the five
  steps are the resume state for the next session.
- **EXAMPLE** (bad): a session ends mid-task with the findings only in its
  final message.
- **COUNTEREXAMPLE** (good): `good/checkpoint_workflow.py` performs the
  five steps and the resume session continues at the exact point.
- **VERIFICATION**: the resume output of the good fixture is the proof.
- **SOURCE**: repo-internal AGENTS.md (stopping rule); codex-37653.

## 4. Never mark work complete silently

- **RULE**: completion requires the unit's verification evidence, recorded
  and reproducible. "Silently complete" means the claim outruns the
  records; it converts an inference into a false fact. KNOWN (meta-completion).
- **WHY AI GETS IT WRONG**: the model's confidence substitutes for the
  artifact; the summary says done, the repository has no evidence.
- **CORRECT REASONING**: completion = acceptance criteria met + evidence
  recorded. Both must be true before the word "complete" is used.
- **EXAMPLE** (bad): a skill marked complete whose evals/README records no
  runs.
- **COUNTEREXAMPLE** (good): the completion entry points at recorded
  outputs (exit codes, logs) and the updated progress file.
- **VERIFICATION**: the fixtures log a completion only with an evidence key.
- **SOURCE**: meta-completion; meta-evidence; arxiv-2607-00107.
