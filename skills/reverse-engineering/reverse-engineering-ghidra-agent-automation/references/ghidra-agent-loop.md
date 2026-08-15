# Agent/Ghidra Automation Loop — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Loop shape: PyGhidra in-process vs headless

- **RULE**: pick the loop shape by job size. PyGhidra `open_program` keeps a
  FlatProgramAPI session for interactive multi-turn analysis of one program;
  `analyzeHeadless` runs batch scripts per program. Both must persist
  state between turns, or the agent re-derives everything each turn.
- **WHY AI GETS IT WRONG**: writes one-shot scripts that re-open and
  re-analyze the program every turn, losing annotations and recomputing the
  same facts, or spawns a fresh headless run and then claims to "see" earlier
  results.
- **CORRECT REASONING**: a turn must start from the previous turn's saved
  state: the Ghidra project, or a serialized report that the agent diffs
  against. Cost per turn is dominated by re-analysis; the loop exists to make
  each turn incremental.
- **EXAMPLE** (bad): triage, annotate, and re-parse the whole binary inside a
  single script, then exit — next turn starts from zero.
- **COUNTEREXAMPLE** (good): `analysis_loop.py` writes a triage baseline
  (functions, entry points, sizes, callees) and re-diffs after every change.
- **VERIFICATION**: target command `pyghidra analysis_loop.py <program>
  report.json`; the report is the persistent artifact.
- **SOURCE**: pyghidra-docs; ghidra-api.

## 2. Triage → annotate → type-recovery → diff

- **RULE**: the pipeline order is fixed: triage ranks functions (size,
  callee fan-in, cross-references); annotation names/comment them; type
  recovery classifies widths/extensions and, when present, matches DWARF;
  diff compares each step against the triage baseline.
- **WHY AI GETS IT WRONG**: jumps straight to "decompile the interesting
  function" and types the whole program from a few instructions, or annotates
  before knowing which functions matter.
- **CORRECT REASONING**: triage decides what is worth annotating; annotation
  changes what decompilation shows; type recovery depends on both. The diff
  step makes every change reviewable and revertible.
- **EXAMPLE** (bad): decompiling the largest function first and claiming
  it is the entry point without cross-reference data.
- **COUNTEREXAMPLE** (good): triage ranks by fan-in; only then annotate and
  recover types; the diff between turns is part of the report.
- **VERIFICATION**: `analysis_loop.py` emits the triage baseline the diff
  step consumes.
- **SOURCE**: ghidra-api; pyghidra-docs.

## 3. Rebase before address arithmetic ($0000 vs $A000)

- **RULE**: Ghidra program addresses are absolute (image base + offset). Any
  offset arithmetic must use the program's image base, fixed before the
  arithmetic, and re-checked when the base changes. A ROM whose image base is
  `$A000` has its first mapped byte at `$A000`, not `$0000`.
- **WHY AI GETS IT WRONG**: treats the raw file offset as the program address;
  the Quesma case's rebase failure (`$0000` vs `$A000`) is exactly this.
- **CORRECT REASONING**: `abs = base.add(offset)`. Report the base with every
  computed address so a wrong base is visible in the diff.
- **EXAMPLE** (bad): `confident_verdict.py` patches `program.getMinAddress()`
  (the file-start `$0000`) instead of `base + $A000`.
- **COUNTEREXAMPLE** (good): `rebase_and_patch.py` computes
  `base.add(offset)` and prints base, offset, and absolute address.
- **VERIFICATION**: target command `pyghidra rebase_and_patch.py <program>
  0xA000 0xEA`; the printed `base + 0xa000` must be the intended address.
- **SOURCE**: ghidra-api (Address, getImageBase); historical Quesma case.

## 4. Identity verdicts need a falsification check

- **RULE**: asserting what a program IS (a game, a packer, a protocol)
  requires at least one falsification check on record: known hashes, header
  fields, a signature set that includes near-misses, structural markers. A
  single 4-byte signature is not an identity.
- **WHY AI GETS IT WRONG**: pattern-matches one signature to one known name
  and emits a fabricated confidence (Quesma: "Centipede" for River Raid).
- **CORRECT REASONING**: `jmp $A000` at the reset vector matches many 6502
  ROMs; identity must survive the falsification attempt (other game's hash
  differs; header/region checks). If the falsifier was not run, the verdict
  is INFERRED.
- **EXAMPLE** (bad): `confident_verdict.py` — 4-byte signature →
  "Centipede (confidence 0.95)".
- **COUNTEREXAMPLE** (good): "reset-vector jmp matches ROMs X, Y, Z; hash
  check narrows it to River Raid (VERIFIED) or remains INFERRED."
- **VERIFICATION**: run the bad script against a non-Centipede ROM and observe
  the false identity; the good flow must fail or downgrade the verdict.
- **SOURCE**: ghidra-api; pyghidra-docs; historical Quesma case (UNVERIFIED
  beyond the survey record).

## 5. Byte patches are verified by read-back

- **RULE**: after `Memory.setByte(addr, value)`, read the byte back and
  compare. Report success only on equality. Unverified patch claims are the
  failure mode (Quesma: the human wrote the DEY→NOP patch; the model could
  not).
- **WHY AI GETS IT WRONG**: calls the setter and prints "patched" without
  checking; write-protected or mis-addressed memory fails silently.
- **CORRECT REASONING**: `setByte` returns void; only `getByte` tells you the
  outcome. Patch → read-back → compare → report.
- **EXAMPLE** (bad): `confident_verdict.py` prints "PATCH: applied
  (unverified)".
- **COUNTEREXAMPLE** (good): `rebase_and_patch.py` raises if the read-back
  differs, else prints the read-back byte.
- **VERIFICATION**: target command on the good script; a wrong offset must
  raise or be caught by read-back.
- **SOURCE**: ghidra-api (Memory.setByte/getByte).

## 6. Types and verdicts carry gate markers

- **RULE**: every recovered type and every program verdict is labeled
  VERIFIED / KNOWN / INFERRED / UNVERIFIED. One instruction-width
  observation is INFERRED at best; `movsd` in FP context is a scalar double
  load, not a string.
- **WHY AI GETS IT WRONG**: converts a single observation into a "high
  confidence" fact (bad/type_guess.py: `movzbl` → "char", `movsd` →
  "string").
- **CORRECT REASONING**: type recovery cross-checks every observation
  (multiple uses, DWARF, call sites) — see `binary-analysis-type-recovery`.
  The gate marker prevents the confident-but-wrong pattern from being
  actionable.
- **EXAMPLE** (bad): "param0 is char (high confidence)" from one `movzbl`.
- **COUNTEREXAMPLE** (good): "param0: movzbl reads → uint8_t at least;
  cross-check with DWARF — INFERRED until confirmed."
- **VERIFICATION**: code review of `bad/type_guess.py` vs the cross-check
  list in `binary-analysis-type-recovery`.
- **SOURCE**: pyghidra-docs; ghidra-api.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Loop shape | PyGhidra `open_program` for turns; `analyzeHeadless` for batch; persist state |
| Pipeline | triage → annotate → type-recovery → diff (baseline before changes) |
| Rebase | Ghidra addresses are absolute; `base.add(offset)`, never raw file offset |
| Identity | one signature is not identity; run a falsification check or mark INFERRED |
| Patches | write → read-back → compare → then report |
| Type claims | single-observation types are INFERRED; `movsd` = double load |
