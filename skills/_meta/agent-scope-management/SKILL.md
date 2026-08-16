---
name: agent-scope-management
description: Use when an agent session spans many edits, spawns subagents, or loses context. Teaches persisting state to files (progress/WORKLOG), single-responsibility scope boundaries, stopping rules, and why conclusions not written down do not exist.
---

# Agent Scope & Session Management

## When to use

- A session will make many edits across several files or run for a long
  time; context or token budget is finite.
- Fanning out work to subagents or Agent Manager sessions and needing each
  to stay inside its assigned scope.
- A run is interrupted (context exhaustion, crash, user switch) and must be
  resumable without re-reading everything.
- Working in a repo with a resume protocol (`AGENTS.md`,
  `roadmap/progress.yaml`, `WORKLOG.md`) and a completed-work rule.

## When not to use

- A single, small, self-contained change with no state to preserve.
- Pure reasoning with no side effects and no resumption need.
- Choosing what to work on next — use `meta-routing`.

## What the agent often gets wrong

- Keeps the whole task state in its context window and calls the work
  "done" without writing any checkpoint. When the session ends or the
  context is compacted, the intermediate results are gone — the stopping
  rule (save intermediate results first) is inverted.
- Lets a session grow unboundedly: adding unrelated fixes, "helpful"
  refactors, and re-reasoning from scratch. Scope creep burns tokens and
  risks touching files outside the assignment (B2).
- Marks work complete silently, or "claims" completion without the
  verification records, violating the never-silently-complete rule
  (`meta-completion`).
- Delegates to a subagent with a vague prompt, then accepts its output
  without a scope/evidence check — the subagent's scope boundaries are
  never stated or verified.
- On context exhaustion, stops without writing `progress.yaml` /
  `WORKLOG.md` updates, so the next session starts from zero — the exact
  failure this skill exists to prevent.
- Assumes that because the model "remembers" the task in this session, the
  memory will survive compaction or a fresh session. Memory is not a
  store; files are (A10).
- Writes progress files at the END of the work instead of after each unit,
  losing everything between the last write and the crash.

## How to reason correctly

1. Define the unit of work (one skill, one feature, one fix) and its
   acceptance criteria before starting.
2. Decide the minimal durable state: which facts, decisions, and file
   locations must survive a restart. Write them as files early, not late.
3. Checkpoint after every completed unit (per the repo's progress
   protocol), before starting the next unit — never batch all updates at
   the end.
4. Enforce scope: if a change is not required by the unit's acceptance
   criteria, record it as a note for later instead of doing it now.
5. For delegation, state the subagent's scope, outputs, and evidence
   requirements explicitly; verify the returned work against them.
6. Apply the stopping rule: (1) save intermediate results, (2) update
   progress state, (3) update the worklog, (4) preserve found sources, (5)
   never leave unwritten knowledge in memory.
7. Before claiming completion, replay the verification records — an
   unrecorded conclusion does not exist.

## What to verify

- The resume state file exists and would let a fresh session resume
  exactly where this session is (current task, next action, sources).
- Every completed unit has a progress update with a timestamp; no unit is
  marked complete without its verification evidence.
- The session did not modify anything outside its declared scope (diff
  review against the assignment).
- Delegated work was returned with its own evidence, and the evidence was
  checked, not just the summary.
- The worklog records what was done, what was learned, and what is next —
  not just what was attempted.

## How to verify

Host-side (Python simulation of the checkpoint discipline; no special
toolchain):

```
python examples/good/checkpoint_workflow.py
python examples/bad/no_checkpoint_workflow.py
```

Repository protocol (the repo itself is the verification target):

```
git status                      # confirm no out-of-scope files were touched
git diff --stat                 # confirm the diff matches the assignment
# resume protocol: open roadmap/progress.yaml and WORKLOG.md and confirm
# the state a fresh session would load matches this session's position
python tools/validate.py        # run repo validators before committing
```

## Where the knowledge comes from

- `arxiv-2607-00107` — Illusion of Safety: verification that looks
  rigorous while nothing is recorded; un-checkpointed completion is the
  same illusion for process state
- `codex-37653` — long-task reliability: state held only in context is
  lost; persisted state is the only resumable state
- `perry-ai-code` — overconfidence without evidence; a "done" claim needs
  recorded verification
- `meta-evidence`, `meta-completion`, `meta-verification` — claims need
  evidence; completion needs verification; the stop rule is the process
  twin of the evidence rules
- repo-internal resume protocol (`AGENTS.md`, `roadmap/progress.yaml`,
  `WORKLOG.md`) — the operational form of this skill

## Related skills

- `meta-completion` (require) — never mark work complete without evidence;
  this skill supplies the process discipline
- `meta-evidence` (require) — the recorded evidence is the only state that
  survives
- `meta-verification` (recommend) — verification records must be part of
  the checkpoint
- `meta-routing` (recommend) — decide what belongs to which unit before
  scoping sessions
- `meta-rationalizations` (recommend) — "I'll remember this" is a
  rationalization; files are the memory

## Evaluation

- Synthetic: `bad/no_checkpoint_workflow.py` loses work on a simulated
  interruption; `good/checkpoint_workflow.py` resumes exactly. The agent
  must produce the resume-state file as the artifact.
- False-positive: a single-unit session that completes without extra
  checkpoint files must NOT be flagged for "not checkpointing".
- Historical: codex-37653 (long-task failure from in-context-only state)
  and claude-code#82057 (completion claimed without reproducible
  verification) — shapes reproduced by the fixtures.
- Adversarial: the bad workflow prints "all units complete" after losing
  unit 2 — an agent that accepts the summary without checking the resume
  state reproduces the failure.
- Commands recorded on this host (python 3.11.9): `evals/README.md`.
