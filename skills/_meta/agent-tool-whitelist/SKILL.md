---
name: agent-tool-whitelist
description: Use when an agent must decide which shell/tool operations are allowed in a low-level repo or CI environment: maintaining a whitelist of safe, deterministic operations, rejecting ad-hoc dangerous commands, and enforcing read-only-by-default. Teaches the discipline of allowed-operations gates that prevent environment-destructive or non-reproducible commands.
---
# Agent Allowed-Operations Discipline (Tool Whitelist)

## When to use

- Any agent session that will run shell commands, build tools, or
  environment-modifying operations on a shared/dev/CI machine.
- Writing or enforcing an allowed-operations policy: a whitelist of safe
  deterministic commands (read, lint, compile to temp, query version)
  and an explicit deny list (format, install, delete, network fetch,
  deploy, reset hardware).
- Reviewing a proposed command for "is this in the whitelist, and is it
  deterministic and reversible?"
- Setting up a sandbox/CI policy where agents must not modify system
  state, hardware, or non-repo files outside the approved surface.

## When not to use

- Repo-specific engineering rules (AGENTS.md content) — different skill.
- Task-specific technical decisions (what to compile, how to verify) —
  use the relevant domain skill.
- Human decision-making about deployment/security policy — the whitelist
  is a tool, not the policy owner.
- Writing generic safety prompts; this skill is about concrete,
  enforceable operation whitelists.

## What the agent often gets wrong

- Running ad-hoc "helpful" commands that mutate environment: `pip install
  -r ...`, `apt-get`, `cargo add`, `git reset --hard`, `rm -rf` — each
  changes state that other sessions/CI depend on, and none is part of the
  task's verification.
- Believing a command is safe because it "usually works", without
  checking determinism (e.g. `format` vs `check`, network fetch vs cached
  file) or reversibility (can this state be restored?).
- Using the shell to "explore" with destructive side effects (e.g.
  `Get-ChildItem` is fine, but `Remove-Item` in a script that globs
  wrong deletes the repo — the PowerShell wildcard-copy failure class).
- Assuming a build/install tool is safe just because it is standard;
  `make install`, `cargo install`, `npm install -g` all mutate the host.
- Failing to distinguish read-only verification (allowed) from
  environment mutation (denied): the agent runs the test suite, then
  "helpfully" installs the missing toolchain globally instead of
  documenting it.
- Treating "the command exited 0" as evidence of safety — a command can
  be both successful and destructive.

## How to reason correctly

1. **Name the operation class first**: read-only (safe by default),
   build-in-temp (safe with an approved workdir), environment-mutating
   (deny unless explicitly approved and reversible), network (deny unless
   pinned/cached), destructive/irreversible (always deny). Classify every
   command before running it.
2. **Maintain a concrete whitelist**: enumerate allowed commands with
   their arguments (e.g. `python tools/validate.py`, `gcc -fsyntax-only`,
   `git status/diff`, `python examples/...`); anything outside is denied.
3. **Prefer the read-only / check variant**: `gcc -fsyntax-only` over
   `gcc -o out`, `cargo check` over `cargo build --release`,
   `git diff` over `git checkout`, `--dry-run` where available.
4. **Sandbox mutation**: if the task truly requires writes (compile to
   temp, run a test binary), use the approved workdir
   (`%TEMP%`-style, `build/` gitignored) and never the repo root or
   system paths.
5. **Document denials**: when a command is needed but not whitelisted,
   record the request + reason instead of running it. The whitelist is
   reviewed, not silently stretched.
6. **Check reversibility and blast radius**: even whitelisted commands
   must be reversible and scoped; if a command could touch outside the
   repo or other sessions, it is denied.

## What to verify

- Every command the agent runs is on the allowed list (or classified
  read-only); no ad-hoc dangerous command executes.
- No environment mutation (global install, format, deploy, hardware
  reset, delete) without explicit approval and a documented reason.
- Build/verify outputs go to an approved temp/build dir, not the repo
  root or system paths.
- The whitelist itself is concrete (command + args), not prose like
  "be safe".
- Denied-but-needed operations are logged, not run.

## How to verify

```
# Host-verifiable: whitelist policy engine (executed on this host)
python examples/good/whitelist_check.py        # PASS: policy enforced
python examples/bad/whitelist_violation.py     # FAIL: dangerous cmd detected
python examples/bad/whitelist_install_hidden.py # FAIL: install slips through

# Structural check on this repo (executed):
python tools/validate.py
#   the repo's own gate runs read-only checks; see tools/ for the list
```

The policy engine is host-verifiable (python 3.11.9); the whitelist
discipline itself is a behavior rule, evaluated by adversarial cases in
`evals/README.md`.

## Where the knowledge comes from

- `codex-37653` — zsh parallelism-limit bypass: a "clever" ad-hoc
  command escalated to 86 processes and a kernel panic (empirical).
- `arxiv-2607-12507` — adversarial binary-RE: provenance-gating dropped
  unsafe agent actions from 35/40 to 0/40 (empirical).
- `meta-verification` — the "command exited 0" trap: success is not
  safety.
- `make-manual` — `.DELETE_ON_ERROR`/signal semantics as evidence that
  build tools mutate state.

## Related skills

- `meta-verification` (require; exit 0 is not proof of safety).
- `meta-routing` (recommend; route each operation to the domain skill).
- `meta-assumptions` (recommend; "this command is safe" is an
  assumption to surface).
- `build-process-signal-and-state-safety` (recommend; build tools and
  signal handling as mutation risk).
- `safe-low-level-from-scratch` (recommend; write code that needs fewer
  dangerous commands).

## Evaluation

- Synthetic: `bad/whitelist_violation.py` (rm -rf on a glob) and
  `bad/whitelist_install_hidden.py` (pip install inside a "compile"
  script) must be rejected; `good/whitelist_check.py` must pass.
- False-positive: a read-only command (`git status`, `readelf`, `python
  tools/validate.py`) must NOT be flagged; an approved build-in-temp
  write is fine.
- Historical: the PowerShell `Copy-Item` wildcard-delete incident in this
  repo's WORKLOG (empirical, 2026-08-14) — a "safe" command destroyed
  files; `codex-37653` zsh-parallelism kernel panic.
- Adversarial: a command that looks benign but mutates (install inside a
  check script, `git reset --hard` in a "diff" step); a "dry-run" flag
  that actually writes; an exit-0-but-destructive command.
- Verified facts and commands: `evals/README.md`.
