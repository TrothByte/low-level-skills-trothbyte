# Agent Scope: State Persistence and Checkpointing

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED /
UNVERIFIED.

## 1. Memory is not a store; files are

- **RULE**: an agent's context window is volatile: compaction, session end,
  and crash all discard it. Any fact, decision, or file location that the
  next unit (or next session) needs must be written to a durable artifact
  before it can be relied on. KNOWN (operational rule of long-running
  agents; codex-37653).
- **WHY AI GETS IT WRONG**: "I remember this task" is treated as state; when
  the window is cut, the memory is gone and the work is silently restarted
  or claimed without its evidence.
- **CORRECT REASONING**: before starting a unit, ask "what must survive a
  restart?"; write exactly that. After each unit, update the progress
  artifact (this repo: roadmap/progress.yaml, WORKLOG.md).
- **EXAMPLE** (bad): `examples/bad/no_checkpoint_workflow.py` — the agent
  keeps all results in memory, is interrupted after unit 2, and loses it.
- **COUNTEREXAMPLE** (good): `examples/good/checkpoint_workflow.py` — a
  JSON state file is updated after every unit; a fresh "session" resumes
  exactly at unit 3.
- **VERIFICATION**: `python examples/good/checkpoint_workflow.py` (resume
  exact); `python examples/bad/no_checkpoint_workflow.py` (work lost,
  prints the loss).
- **SOURCE**: codex-37653 (in-context-only state loss); arxiv-2607-00107
  (unrecorded claims).

## 2. Checkpoint after each unit, not at the end

- **RULE**: progress updates are written after every completed unit so a
  crash between units loses at most one unit's work. Batching all updates
  at the end re-creates the single point of failure the discipline exists
  to remove. KNOWN (this repo's progress protocol; operational rule).
- **WHY AI GETS IT WRONG**: agents write the summary when the work is
  "done" — precisely when the accumulated in-context state is largest and
  the loss is greatest.
- **CORRECT REASONING**: treat the update as part of the unit's completion
  criterion, not as closing ceremony.
- **EXAMPLE** (bad): a 5-unit task whose progress file is written only at
  the end; a crash in unit 4 loses all five.
- **COUNTEREXAMPLE** (good): 5 update events, one per unit; crash in unit 4
  loses only unit 4.
- **VERIFICATION**: the fixtures record the update timestamps per unit.
- **SOURCE**: codex-37653; repo-internal AGENTS.md (resume protocol).

## 3. Conclusions without recorded evidence do not exist

- **RULE**: a "done" or "correct" claim is actionable only if the artifact
  that would falsify it is on disk (a recorded run, an exit code, a diff).
  Claims that live only in the reply are untestable and therefore not
  knowledge. KNOWN (meta-evidence; arxiv-2607-00107).
- **WHY AI GETS IT WRONG**: the final message asserts completion and the
  model "knows" it did the work; nobody checks whether the records were
  written before the session ends.
- **CORRECT REASONING**: before marking complete, replay the verification
  commands and store their outputs in the evals/artifact; the checkpoint
  is the evidence, not the summary.
- **EXAMPLE** (bad): a unit marked complete with no recorded run, only a
  summary sentence.
- **COUNTEREXAMPLE** (good): the checkpoint entry links the recorded
  outputs (commands + exit codes).
- **VERIFICATION**: `good/checkpoint_workflow.py` stores per-unit evidence
  keys that the resume step re-validates.
- **SOURCE**: meta-evidence; meta-completion; arxiv-2607-00107.
