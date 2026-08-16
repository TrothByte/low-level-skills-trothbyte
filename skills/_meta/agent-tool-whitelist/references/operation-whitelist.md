# Agent Allowed-Operations Discipline — Reference Rules

Knowledge layer for `agent-tool-whitelist`. Format: RULE → WHY AI GETS
IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION →
SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED. The policy
engine fixtures were executed on this host (python 3.11.9). Relative
paths assume the skill directory as CWD.

## 1. Classify every operation: read-only, build-in-temp, mutate, network, destructive

- **RULE**: before running any command, classify it into an operation
  class. Read-only (inspect, query, lint, diff) is safe by default.
  Build-in-temp (compile to an approved workdir) is safe with scope.
  Environment-mutating (global install, format, deploy, hardware reset)
  and destructive/irreversible operations (delete, overwrite, reset) are
  denied unless explicitly approved, scoped, and reversible.
- **WHY AI GETS IT WRONG**: agents run "helpful" commands (install,
  format, reset) because they treat the absence of an explicit
  prohibition as permission, and they never name the operation class.
- **CORRECT REASONING**: the classification is the first decision; a
  command that mutates anything outside the approved surface is denied
  regardless of how "standard" it is.
- **EXAMPLE** (bad): `examples/bad/whitelist_install_hidden.py` — a
  "compile" script that silently runs `pip install`.
- **COUNTEREXAMPLE** (good): `examples/good/whitelist_check.py` — a
  policy engine that classifies and rejects mutation.
- **VERIFICATION**: executed on this host (policy engine tests).
- **SOURCE**: arxiv-2607-12507 (provenance-gating dropped unsafe actions
  to 0/40); meta-verification (exit 0 is not proof).

## 2. The whitelist is concrete (command + args), not prose

- **RULE**: a useful whitelist enumerates exact allowed commands with
  their arguments (`python tools/validate.py`, `gcc -fsyntax-only`,
  `git status`, `python examples/good/*.py`) and everything else is
  denied by default. Prose policies ("be careful", "use safe commands")
  are not enforceable and fail under adversarial review.
- **WHY AI GETS IT WRONG**: agents write "avoid dangerous commands" and
  then treat the specific dangerous command as an unmentioned exception.
- **CORRECT REASONING**: default-deny with an explicit allowlist;
  unlisted commands are logged as denials and escalate to a reviewer.
- **EXAMPLE** (bad): `examples/bad/whitelist_violation.py` runs an
  unlisted `rm -rf`-style operation on a glob.
- **COUNTEREXAMPLE** (good): `examples/good/whitelist_check.py` matches
  commands against an explicit table.
- **VERIFICATION**: policy engine tests executed.
- **SOURCE**: codex-37653 (ad-hoc command escalation); make-manual
  (build tools mutate state).

## 3. Prefer the read-only / check variant of every command

- **RULE**: when a tool has a non-mutating variant, use it:
  `gcc -fsyntax-only` over full compile, `cargo check` over
  `cargo build`, `git diff` over `git checkout`, `--dry-run` over the
  real operation. The check variant provides the same evidence with zero
  blast radius.
- **WHY AI GETS IT WRONG**: agents run the full form because it is the
  "real" command, ignoring that the evidence goal is usually the check.
- **CORRECT REASONING**: the verification goal decides the variant; if a
  check variant produces the needed evidence, the mutating variant is
  unnecessary and therefore denied.
- **EXAMPLE** (bad): `git reset --hard` to "see" the repo state instead
  of `git status`/`git diff`.
- **COUNTEREXAMPLE** (good): the fixture uses only read-only commands for
  inspection.
- **VERIFICATION**: structural (fixture review); policy engine executed.
- **SOURCE**: meta-verification (evidence over action).

## 4. Approved writes go to a temp/build dir, never the repo root or system paths

- **RULE**: writes (compiled binaries, test logs, temporary files) are
  scoped to an approved workdir (`%TEMP%`, gitignored `build/`). Writes
  to the repo root, home dir, or system paths are denied — a stray binary
  or a glob hitting the wrong directory is a classic destructive failure.
- **WHY AI GETS IT WRONG**: agents compile to the repo root "for
  convenience", leaving artifacts that break the repo and, with a bad
  glob, destroy source files (the PowerShell `Copy-Item` wildcard-class).
- **CORRECT REASONING**: every write names an approved directory; the
  default is read-only.
- **EXAMPLE** (bad): `gcc -o out.exe foo.c` in the repo root; a later
  `Remove-Item *` cleanup deletes sources.
- **COUNTEREXAMPLE** (good): outputs under an approved temp/build dir.
- **VERIFICATION**: empirical incident in this repo (WORKLOG
  2026-08-14, PowerShell wildcard-copy data loss).
- **SOURCE**: empirical (repo WORKLOG); make-manual (build artifacts and
  state).

## 5. Document denials instead of silently stretching the policy

- **RULE**: when a needed operation is not whitelisted, the agent
  records the request and the reason and asks for approval. Silently
  adding an exception ("I'll just run it once") erodes the policy's
  protective value and is itself a failure mode.
- **WHY AI GETS IT WRONG**: agents treat the whitelist as a formality and
  the specific task as justification for a one-off exception.
- **CORRECT REASONING**: the whitelist is the contract with the
  environment; exceptions are explicit, reviewed, and logged — never
  silent.
- **EXAMPLE** (bad): running `pip install` "just this once" to make a
  test pass.
- **COUNTEREXAMPLE** (good): logging "denied: pip install (not
  whitelisted); request approval".
- **VERIFICATION**: structural review; policy engine executes denials.
- **SOURCE**: arxiv-2607-12507 (provenance-gating effect); meta-routing.

## 6. Success is not safety; exit 0 does not clear a command

- **RULE**: a command can exit 0 and still be destructive or
  non-deterministic. Safety is determined by the operation class and
  scope, not by the return code.
- **WHY AI GETS IT WRONG**: agents use "it ran without error" as the
  safety signal and never revisit the command's side effects.
- **CORRECT REASONING**: evaluate side effects before running; the exit
  code only tells you whether the command finished, not what it changed.
- **EXAMPLE** (bad): a silent `Remove-Item -Recurse` that exits 0 after
  deleting files.
- **COUNTEREXAMPLE** (good): classification precedes execution; a
  destructive command is never reached.
- **VERIFICATION**: policy engine rejects by class, not by exit code
  (executed).
- **SOURCE**: meta-verification (harness-validity: pass ≠ proof).

## Quick reference table

| Operation class | Default | Example |
|---|---|---|
| read-only | allow | `git status`, `readelf`, `python tools/validate.py` |
| build-in-temp | allow (scoped) | `gcc -o build/out foo.c` |
| environment-mutate | deny | `pip install`, `make install`, `git reset --hard` |
| network | deny | `apt-get`, `cargo add`, unvetted fetch |
| destructive | deny | `rm -rf`, `Remove-Item`, format/reset |
