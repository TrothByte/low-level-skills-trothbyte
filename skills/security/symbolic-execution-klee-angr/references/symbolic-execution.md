# Symbolic Execution — KLEE and angr Reference

Depth material for `symbolic-execution-klee-angr`. SKILL.md stays
operational; this file carries the technical depth.

## Core model

Symbolic execution executes a program with *symbolic* values instead of
concrete ones. Each program point carries a path condition: the conjunction
of constraints accumulated along the path. When a conditional branches on a
symbolic value, execution *forks*: one state takes the true edge (path
condition extended with the branch condition) and one takes the false edge
(path condition extended with its negation). At a target (assert, crash
site, specific instruction), an SMT solver solves the path condition and
produces a concrete input that drives the program to that point.

State = (program counter, memory/store, path condition). Exploration
strategies (BFS, DFS, guided) trade completeness for cost.

## Path explosion

Every fork doubles (at worst) the number of live states. A chain of `n`
independent symbolic branches produces `2^n` paths. The example model in
`examples/tools/path_exploration_model.py` demonstrates this: 12 branches
over 12 independent symbolic variables yield 4096 states in the full run,
and a depth bound of 6 stops the search before the target is reached.

Mitigations:

- Size the symbolic inputs down (constrain domains with `klee_assume`).
- Prune: `avoid` addresses in angr, `--max-depth`/`--max-time` in KLEE.
- Merge states (angr state merging; KLEE's search heuristics).
- Slice the program to the statements that influence the target.
- Prefer a concolic/hybrid strategy for large inputs (SAGE model).

## KLEE workflow (source available, LLVM bitcode)

Prerequisites: LLVM/clang, KLEE built with a solver backend (STP/Z3).

```
clang -I<klee>/include -emit-llvm -c prog.c -o prog.bc
klee --max-time=60 --max-instruction-time=10 --max-memory=1000 prog.bc
klee-stats prog-klee-out
```

- `klee_make_symbolic(buf, size, "name")` declares symbolic bytes.
- `klee_assume(cond)` adds an assumption to the path condition (a
  constraint that prunes states violating it).
- `klee_assert(cond)` marks a property; on violation KLEE reports an
  error and emits a concrete test case in the `klee-out-*` directory.
- Default runtime checks include division-by-zero and overshift; the
  agent must know which checks are on for the report.
- Results: `.err` files, `testN.ktest` concrete inputs, termination
  summary ("KLEE: done: explored paths = N, generated tests = M").
- The POSIX model (`--libc=uclibc --posix-runtime`, uClibc) models
  `main(argc, argv, envp)` so symbolic argv/environment/file contents
  are possible.
- Classic tutorial target: symbolic index into a fixed array guarded by a
  too-loose `klee_assume` — KLEE finds the OOB access and the concrete
  input that triggers it.

## angr workflow (binary only)

Prerequisites: `pip install angr` (Python 3.8+). Core stack: CLE (binary
loading), VEX/Unicorn (IR lifting), claripy (symbolic values/constraints),
SimProcedures (library call modeling), SimulationManager (exploration).

```
import angr
import claripy

proj = angr.Project("target", auto_load_libs=False)
# symbolic argv[1] (16 bytes, angr appends a NUL terminator)
arg = claripy.BVS("arg", 16 * 8)
state = proj.factory.entry_state(args=["target", arg])
sm = proj.factory.simulation_manager(state)
found = sm.explore(find=crash_addr, avoid=exit_addr)
st = found.found[0]
print(st.solver.eval(arg, cast_to=bytes))   # concrete input
```

- `find` accepts an address, a list, or a callable over states; `avoid`
  prunes states that reach it.
- State plugins: `state.posix` (stdin/stdout), `state.globals`,
  `state.solver`. Symbolic stdin:
  `state.posix.files[0].content = claripy.BVS("stdin", n * 8)` (or pass
  `stdin=` when constructing the entry state).
- `SimProcedures` / `Hook` replace external calls so libc and syscalls
  are modeled instead of explored as machine code; `auto_load_libs=False`
  avoids loading the whole system libc but then library calls must be
  hooked or handled.
- CFG recovery (`proj.analyses.CFGFast`) runs before addressing targets;
  ASLR/PC-relative code and packed binaries need preprocessing.
- Concolic/hybrid mode: concrete values for a prefix, symbolic for the
  rest (SAGE-style) — useful against path explosion.

## Soundness limits

- Solver queries are only as exact as the constraint model: angr's memory
  model (default coarse-grained) can produce false positives; overly
  permissive or under-specified models miss paths (false negatives).
- KLEE relies on the bitcode faithfully reflecting the source semantics;
  undefined behavior in C means the bitcode may encode a behavior the
  source does not have.
- Neither tool proves absence of bugs; both prove reachability of
  instrumented targets under the modeled environment.
- Timeouts terminate exploration: "no error found in 60s" is not "no
  error exists."

## KLEE vs angr vs concolic vs model checking

| Approach | Input | What it produces | Proof? |
|---|---|---|---|
| KLEE | LLVM bitcode | concrete inputs to targets / asserts | no (bounded paths) |
| angr | binaries | concrete inputs reaching `find` addresses | no |
| Concolic (SAGE) | binary + concrete seed | inputs for explored paths | no |
| CBMC/Kani (BMC) | C/Rust source | proof or counterexample up to unwind bound | bounded |
| Frama-C/WP (deductive) | C + ACSL specs | proof obligations discharged by provers | yes, if sound |

Symbolic execution explores real executions guided by a solver; model
checking proves properties over a state space. Different strengths, and
the skill `formal-verification-kani-verus` covers the proving side.

## History / notable results

- KLEE (OSDI 2008): automatically generated high-coverage tests for GNU
  coreutils; found errors in coreutils utilities.
- SAGE (Microsoft Research): concolic execution over Windows file parsers
  and media decoders; hundreds of distinct bugs in shipping components.
- angr SoK (IEEE S&P 2018): survey and evaluation of symbolic execution
  implementations; documents the soundness/completeness trade-offs.
- KLEE/angr appear in CTFs (angr's `explore(find=...)` solves many
  "guess the password / magic input" challenges) and in verification
  competitions as bug finders, not provers.

## Concrete-replay gate

Every reported finding must be re-run: take the solver-generated input,
run the actual program (or binary) with it, and confirm the assert
violation or crash reproduces. Without this step the finding is a solver
artifact, not a bug.
