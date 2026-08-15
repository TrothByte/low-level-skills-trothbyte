# Evaluation — reverse-engineering-ghidra-agent-automation

Skill: `skills/reverse-engineering/reverse-engineering-ghidra-agent-automation`.
Stability target: `evaluated`. **RESEARCHED skill**: PyGhidra/Ghidra are NOT
installed on this host (Windows). The examples are target code; the
verification commands below are the exact commands that must be run on a
machine with Ghidra + pyghidra. API names (`getImageBase`, `Memory.setByte`)
must be checked against the installed Ghidra version.

## Synthetic evals

| Case | Fixture | Expected | Command (target machine) |
|---|---|---|---|
| easy/positive | `good/analysis_loop.py` | triage report: image base, functions, sizes, callees | `pyghidra analysis_loop.py <program> report.json` |
| medium/positive | `good/rebase_and_patch.py` | patch at base+0xA000 verified by read-back | `pyghidra rebase_and_patch.py <program> 0xA000 0xEA` |
| medium/negative | `bad/type_guess.py` | single-observation types rejected | review: `movzbl`→char and `movsd`→string need cross-checks |
| hard/negative | `bad/confident_verdict.py` | identity claim rejected; rebase and patch failures caught | `pyghidra confident_verdict.py <program>` → wrong identity + wrong offset |

## False-positive evals (correct results must not be flagged)

- `movsd` in FP context is a scalar double load, NOT a string — flagging it
  as a string-move is itself the bug (`bad/type_guess.py` demonstrates the
  wrong direction).
- A correct patch at `base.add(0xA000)` must not be "corrected" to the raw
  offset `$0000`.
- A verdict that downgrades to INFERRED because no falsifier ran is CORRECT
  behavior, not "lack of confidence".
- A triage report with no annotations is a valid first-turn artifact.

## Historical evals

- Quesma Ghidra-MCP (2026, single-case transcript in the asm survey): Claude
  on a 6502 ROM asserted "Centipede" (actually River Raid); could not rebase
  `$0000` vs `$A000`; could not write the `DEY`→`NOP` byte patch — a human
  did it. `bad/confident_verdict.py` reproduces all three classes. The exact
  transcript is not recoverable — marked UNVERIFIED beyond the survey record.
- Calibration: the case is the canonical "confident-but-wrong" pattern;
  an agent producing any of the three failure classes fails the eval.

## Adversarial evals

- A ROM whose reset-vector bytes match another game: the agent must run a
  falsification check (hash/header) or downgrade to INFERRED.
- A patch target inside a write-protected region: read-back must fail and the
  agent must report it, not "patched".
- A turn where the program's base differs from the previous turn: the diff
  must surface the base change.
- Type recovery given only one `movzbl` use: must be marked INFERRED, not
  asserted.

## Verification commands (target machine — NOT run on this host)

```
pyghidra examples/good/analysis_loop.py target.bin report.json
pyghidra examples/good/rebase_and_patch.py target.bin 0xA000 0xEA
pyghidra examples/bad/confident_verdict.py target.bin
  # expected review result: false identity + patch at wrong offset + no read-back

# headless batch alternative:
analyzeHeadless /tmp/proj /tmp/projname -import target.bin \
  -scriptPath examples/good -postScript analysis_loop.py out.json
```

## Verified facts (on this host)

- Python 3.11.9 compiles all example scripts (`python -m py_compile`): the
  fixture code is syntactically valid Python. Full API behavior is UNVERIFIED
  here — it requires Ghidra.
- `pyghidra` import: not available on this host (module absent) — the
  `from . import pyghidra`-style imports in the examples are target-only.

## Scoring (for routing eval)

- precision: each flagged issue maps to a reference rule (1-6) and is
  demonstrable from the script output.
- recall: false identity, wrong rebase, unverified patch, and
  single-observation type claims are all represented in the fixtures.
- FP-rate: correct rebases, correct FP-type reads, and INFERRED downgrades
  produce zero flags.

## Toolchain status (honest)

- Ghidra, PyGhidra, `analyzeHeadless`: NOT installed. `pyghidra` module
  import verified absent on this host. The skill is researched, not
  source-backed; the commands above are the exact verification plan for a
  Ghidra host.
- Ghidra API surface varies by release; the examples use the documented
  (pyghidra-docs/ghidra-api) entry points and note version checks.
