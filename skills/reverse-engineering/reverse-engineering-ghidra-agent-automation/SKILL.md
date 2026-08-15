---
name: reverse-engineering-ghidra-agent-automation
description: Use when an agent drives Ghidra (PyGhidra daemon/RPC or headless) in a triage-annotate-type-recovery-diff loop, or when an automated verdict about a binary is being trusted. Prevents confident-but-wrong identity claims, rebase errors ($0000 vs $A000), and unverified byte patches.
---

# Reverse Engineering: Agent/Ghidra Automation Loop

## When to use

- An agent automates Ghidra: open a program, triage functions, annotate,
  recover types, diff before/after.
- A verdict must be produced from a binary (identity, packing, obfuscation,
  patch target) and reported with a confidence level.
- Writing a byte patch via script and needing read-back verification.
- Deciding between PyGhidra in-process (`open_program`) and the headless
  analyzer (`analyzeHeadless`) for a batch job.

## When not to use

- Interactive manual RE in the GUI — the loop discipline applies to scripts.
- Decoding raw shellcode bytes — use `reverse-engineering-shellcode-analysis`.
- Recovering types from objdump output without Ghidra — use
  `binary-analysis-type-recovery`.
- Building exploits — analysis discipline only.

## What the agent often gets wrong

- Confident-but-wrong identity: the 2026 Quesma Ghidra-MCP case — Claude
  asserted a 6502 ROM was "Centipede" when it was River Raid. A single
  4-byte signature at the reset vector matches many ROMs; identity needs
  header fields, known hashes, and cross-checks, not one match.
- Rebase errors: patching at raw file offset `$0000` instead of the loaded
  address `$A000`; Ghidra addresses are absolute (image base + offset), and
  the agent must fix the base before any address arithmetic.
- Unverified patches: writing a byte (e.g. `DEY`→`NOP`) and reporting success
  without reading it back (Quesma: the human wrote the patch, the model
  could not).
- Using `movsd`/`movzbl` mnemonics as type evidence without cross-checks
  (`movsd` in FP context is a double load, not a string; one `movzbl` use
  does not type a parameter).
- One-shot scripts instead of a loop: no triage baseline, no diff after
  annotation/type changes, so each run silently diverges from the last.

## How to reason correctly

1. Choose the loop shape: PyGhidra in-process (`open_program`) for
   interactive single-program turns; `analyzeHeadless` for batch. Both must
   persist state between turns (project/tree or re-parse), or the agent
   re-does work every turn.
2. Run the pipeline in order: TRIAGE (rank functions by size, fan-in,
   cross-refs) → ANNOTATE (names/comments via labels) → TYPE-RECOVERY
   (widths + extension mnemonics + DWARF when present) → DIFF (re-compare
   the baseline report; every change is reviewed).
3. Fix and state the image base BEFORE any address arithmetic; compute
   absolute = base + offset, never raw file offset.
4. Gate every patch: write → read back → compare → then report.
5. Gate every identity/verdict: enumerate what would falsify it; if a
   counter-signature or hash check was not run, the verdict is INFERRED,
   not VERIFIED.
6. Mark every recovered type/verdict as KNOWN / INFERRED / UNVERIFIED;
   confident wording without a gate marker is the failure mode.

## What to verify

- The image base used in every address computation matches the program's
  actual base (report `program.getImageBase()`).
- A patch is verified by read-back (the good example does this; the bad one
  does not).
- An identity verdict has at least one falsification check (hash, header,
  known-signature set) on record.
- The triage report exists before annotation/type changes, and the final diff
  against it is part of the output.
- No `movsd`→"string" or single-use `movzbl`→"char" type claims without
  cross-checks.

## How to verify

On this host (Windows): PyGhidra/Ghidra are NOT installed. Target commands:

```
pyghidra examples/good/analysis_loop.py <program> report.json
pyghidra examples/good/rebase_and_patch.py <program> 0xA000 0xEA
python examples/bad/confident_verdict.py <program>     # must be rejected
# headless alternative:
analyzeHeadless <proj> <projname> -import <program> -scriptPath <dir> -postScript ...
```

API specifics (`getImageBase`, `Memory.setByte`, `getListing`) must be
verified against the installed Ghidra version (pyghidra-docs / ghidra-api).

## Where the knowledge comes from

- `pyghidra-docs` — `open_program`, FlatProgramAPI, headless scripting.
- `ghidra-api` — Program, Memory, Listing, FunctionManager interfaces.
- Historical: Quesma Ghidra-MCP (2026) — "Centipede" vs River Raid,
  rebase `$0000` vs `$A000`, DEY→NOP patch done by a human (single-case
  transcript; recorded in the survey, marked UNVERIFIED beyond it).
- Empirical: no Ghidra on this host — the examples are target code; the
  verification commands above are the plan for the target machine.

## Related skills

- `reverse-engineering-shellcode-analysis` — byte-accurate reading inside the
  same loop
- `binary-analysis-type-recovery` — the type-recovery cross-checks the loop
  must apply
- `binary-disassembly-decompilation-fidelity` — gate decompiler output before
  trusting it
- `meta-evidence` — how verdicts get gated (INFERRED vs VERIFIED)

## Evaluation

Synthetic: given a program and the triage output, recover types only with the
cross-checks; `bad/type_guess.py` (single-use `movzbl`→char, `movsd`→string)
must be corrected or rejected.
False-positive: `movsd` as a double load in FP code must NOT be flagged as a
string; a correct rebase at base+offset must not be "fixed".
Historical: Quesma Ghidra-MCP — reproduce all three failure classes from
`bad/confident_verdict.py` (identity, rebase, unverified patch) and require
the gated versions.
Adversarial: a ROM whose 4-byte signature matches another game must fail the
identity claim; a patch written without read-back must be caught; a rebase
that uses the raw file offset must be caught.
Commands and verified facts: `evals/README.md`.
