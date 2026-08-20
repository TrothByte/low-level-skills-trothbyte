---
name: symbolic-execution-klee-angr
description: Use when analyzing code paths with symbolic execution — KLEE for LLVM bitcode, angr for binaries, test-input generation, path coverage, or vulnerability exploration. Teaches state forking, path explosion, and soundness limits, distinct from model checking.
---

# Symbolic Execution with KLEE and angr

## When to use

- Analyzing code paths by solving for inputs, not just running them:
  KLEE on LLVM bitcode (`clang -emit-llvm`) when you have the source,
  angr on binaries when you only have the executable.
- Generating concrete test inputs that reach a specific branch, assert,
  or crash site (coverage-driven input generation).
- Finding assertion violations / memory errors in C programs with KLEE
  (`klee_assert`, `klee_make_symbolic`), or recovering the exact input
  that triggers a binary crash with angr (`explore(find=addr)`).
- Checking whether a guarded check is actually reachable with an
  overflowing or out-of-bounds value.
- Producing seeds for a fuzzer (`fuzzing-harness-evidence-gate`) from
  solver-generated inputs.
- Reasoning about concolic execution (SAGE-style concrete+symbolic).

## When not to use

- Claiming a program is bug-free: symbolic execution explores a bounded
  set of real execution paths, it is not a proof of absence. For proofs
  use `formal-verification-kani-verus` (bounded model checking) or
  deductive verification.
- The property is "no bug for all inputs" — model checkers and SMT-based
  provers are the right tool; symbolic execution reports counterexamples,
  not proofs.
- Huge symbolic inputs (megabytes of symbolic bytes) — the solver and the
  fork tree will not scale; slice or shrink the symbolic surface first.
- Heavy obfuscation / packed binaries — control-flow recovery fails
  before exploration starts.
- Debugging a crash you can already reproduce with gdb — start with
  `debugging-crash-triage-discipline`.
- Concurrency or race properties — symbolic execution of single-threaded
  paths does not model interleavings.
- Replacing a fuzzer entirely: for broad coverage of large inputs, fuzzing
  is usually cheaper; symbolic execution complements it with targeted
  "hard branch" inputs.

## What the agent often gets wrong

- Claiming "verified" after a KLEE/angr run: exploration covered a bounded
  set of paths; "no error found" is a search result, not a proof (rarely
  is the state space exhausted and reported as such).
- Running unbounded searches: unbounded forking explodes or hangs; the
  agent blames the tool instead of adding `--max-time` /
  `--max-instruction-time` / `--max-depth` / `avoid`.
- Making the wrong inputs symbolic: only stdin is symbolic while the bug
  lives in argv, the environment, or a file — the bug is never reached.
- Writing targets the tool never exercises: assertions after unreachable
  code, `klee_make_symbolic` of the wrong variable or the wrong size.
- Confusing the two tools: giving KLEE (bitcode) commands for a binary-only
  analysis and angr (binaries) commands when source exists.
- Ignoring solver-path interactions: nonlinear arithmetic and constraint
  blow-up make solver queries slow; bit-vector-friendly formulation helps.
- Trusting angr's default libc hooking without verification — a missed
  syscall/SimProcedure is a silent false negative.
- Not cross-checking results: a KLEE `.err` or an angr `found` state must
  be re-run concretely with the extracted input to confirm the bug
  actually reproduces.

## How to reason correctly

1. Choose the tool by input: have source → KLEE (bitcode) or CBMC; binary
   only → angr.
2. Define the target precisely: the property (assert, crash, specific
   branch). Instrument accordingly: `klee_assert` for KLEE, `find` +
   `avoid` addresses for angr.
3. Bound the search up front: time, instruction count, depth, number of
   states — and report the bounds honestly with the results.
4. Verify each found state concretely: extract the input, run the program
   with it, confirm the crash/assert reproduces. This is the evidence gate.
5. Model the environment: KLEE's POSIX model / angr `SimProcedures` for
   libc and syscalls, instead of letting them escape the analysis.
6. Report coverage and number of states explored; state the boundedness
   and soundness caveats in the report.

## What to verify

- The analysis terminated within the declared bounds and the states
  explored were reported.
- Every reported bug reproduces when run concretely with the generated
  input (this is the `fuzzing-harness-evidence-gate` rule).
- The symbolic inputs match the real attack surface: stdin/file/argv/env
  are covered, not just the convenient one.
- Library and syscall handling is modeled (KLEE POSIX / angr
  SimProcedures), not silently assumed.
- Boundedness and soundness caveats are stated in the report.

## How to verify

Host-runnable model (this repository, python 3.11, no external deps):

```
python examples/tools/path_exploration_model.py
```

The model is an interval-based interpreter that forks on branches, tracks
path constraints, bounds depth, and prints a concrete witness input for
each reached target or violated assert. Scenario 1: bounded exploration
finds the out-of-bounds input. Scenario 2: an exponential (2^12) fanout
is shown bounded (terminates, target unreachable) and full (4096 states,
target reached) — the path-explosion lesson.

Target commands (KLEE / angr hosts — documented, not run on this host):

```
# KLEE: build bitcode, run, read stats and errors
clang -I<klee>/include -emit-llvm -c prog.c -o prog.bc
klee --max-time=60 --max-memory=1000 prog.bc
klee-stats prog-klee-out
# angr: python examples/good/angr_solve.py after
gcc -g -O0 examples/good/angr_target.c -o angr_target.exe
```

## Where the knowledge comes from

- KLEE documentation (https://klee.github.io/docs/)
- angr documentation (https://docs.angr.io/)
- KLEE paper: Unassisted and Automatic Generation of High-Coverage Tests for Complex Systems Programs (OSDI 2008)
- angr paper: SoK: State-of-the-Art Symbolic Execution (IEEE S&P 2018)
- SAGE / concolic execution (Microsoft Research)

## Related skills

- `smt-z3-sound-usage` (recommend) — the solver discharging path
  constraints; its soundness limits bound the results
- `formal-verification-kani-verus` (recommend) — bounded model checking
  vs. symbolic execution: what each proves
- `invariant-identification` (recommend) — the properties to express as
  `klee_assert` / angr assertions
- `fuzzing-harness-evidence-gate` (recommend) — the concrete-replay gate
  every found state must pass
- `fuzzing-harness-kernel` (recommend) — seed generation that complements
  solver-produced inputs
- `meta-verification` (recommend) — honest verification discipline for
  search results, not proofs
- `c-undefined-behavior` (recommend) — the UB the asserts target (signed
  overflow, OOB, UB semantics)

## Evaluation

Synthetic: run the path-exploration model (both scenarios must print
PASS); classify a task as KLEE vs angr; detect a target whose assert is
after unreachable code. Adversarial: a "clean" symbolic-execution report
with no bounds declared or no concrete replay — must be flagged; an
unbounded loop target passed without `--max-time`; a found state whose
input does not reproduce the bug when run concretely. Historical: KLEE's
OSDI 2008 results (high-coverage tests for GNU coreutils), SAGE's Windows
file-parsing bugs, the angr SoK (IEEE S&P 2018) survey, and the KLEE/angr
CTF and verification-competition findings. FP: a bounded, replayed, and
environment-modeled report with stated caveats must NOT be flagged.
