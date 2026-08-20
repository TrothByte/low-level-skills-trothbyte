# Evaluation — symbolic-execution-klee-angr

Skill: `skills/security/symbolic-execution-klee-angr`. Domain: security.
Stability: researched. KLEE remains target-only (LLVM bitcode host not
available); angr 9.2.213 installed and ran successfully on this host, so
the angr portion is host-verified (source-backed for that part). Overall
stability stays `researched` because the KLEE workflow is unexecuted.

## Verified facts (host, recorded 2026-08-20)

Environment: python 3.11.9, pip 24.0, gcc 16.1.0 (MSYS2/MinGW), Windows.
No z3 before install; z3-solver 4.13.0.0 pulled in as an angr dependency.

1. `pip install angr` — SUCCESS. angr 9.2.213, claripy 9.2.213, cle 9.2.213,
   pyvex 9.2.213, z3-solver 4.13.0.0. (unicorn engine failed to load on
   Windows — `unicornlib.dll`; exploration ran fine without it.)

2. Path-exploration model (`python examples/tools/path_exploration_model.py`)
   — SUCCESS, real output:

```
PASS S1: bounded exploration found violating input idx=8 (fork points=1, completed states=2) and concrete replay reproduces the out-of-bounds access
PASS S2: unbounded fanout is exponential (full run: 4095 forks -> 4096 terminal states; target reached only via the all-zero input); bounded run (max_depth=6) forked only 63 times, terminated early and did NOT reach the target — the path-explosion lesson
PASS: all scenarios
```

3. angr target build (`gcc -g -O0 angr_target.c -o angr_target.exe`) —
   SUCCESS, gcc 16.1.0. Standalone behavior:
   `angr_target.exe VULN` -> exit -1073741819 (0xC0000005, NULL deref);
   `angr_target.exe NOPE` -> exit 0.

4. angr exploration (`python angr_solve.py`, angr 9.2.213) — SUCCESS, real
   output:

```
PASS: reached crash_me with input: b'VULN\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
```

   The exploration starts at `analyze_me` with the `g_input` global made
   symbolic; starting at the PE entry drags angr into `__tmainCRTStartup`
   and produces unconstrained garbage (observed and fixed). The `__main`
   MinGW call must be hooked or bypassed for main-entry analysis.

5. KLEE — NOT executed (no clang/LLVM bitcode toolchain and no KLEE
   build on this host). The `.c` targets and commands are target-only,
   documented with bounds.

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| Bounded exploration finds OOB input | `examples/tools/path_exploration_model.py` S1 | witness idx=8 + concrete replay | PASS (host) |
| Exponential fanout vs depth bound | S2 | 4095 forks/4096 states full; 63 forks bounded, target unreachable | PASS (host) |
| angr magic-input recovery | `examples/good/angr_target.c` + `angr_solve.py` | input `VULN` reaches crash_me | PASS (host) |
| KLEE target with checkable overflow assert | `examples/good/klee_target.c` | KLEE reports the wrap assert as .err | target-only |
| Unbounded symbolic loop | `examples/bad/klee_unbounded.c` | documented: needs `--max-time`/`klee_assume` | target-only |
| Tool choice | KLEE vs angr for a task | source -> KLEE, binary -> angr | reasoning eval |

## False-positive evals (correct results that must NOT be flagged)

- A KLEE run that terminated within `--max-time` with no `.err` and the
  report states the bound and the explored-path count — a bounded search
  result, correctly phrased, must be approved.
- An angr `found` state whose extracted input is re-run concretely and
  reproduces the crash (this repo's `angr_target.exe VULN` reproduces).
- A bounded exploration that did NOT reach the target (S2 bounded run) —
  reported as "target not reached within bound", not as "no bug exists".
- `g_input`-based angr target that models the environment via a symbolic
  global instead of executing the CRT — correct angr practice.

## Historical evals (research-backed)

- KLEE (OSDI 2008, Cadar et al.): high-coverage tests for GNU coreutils;
  agent must know KLEE runs on LLVM bitcode and reports `.err` +
  concrete `.ktest` inputs.
- SAGE (Microsoft Research): concolic execution found hundreds of bugs in
  Windows file parsers/media decoders; concolic = concrete + symbolic.
- angr SoK (IEEE S&P 2018, Baldoni et al.): survey of symbolic execution
  implementations and the soundness/completeness trade-offs.
- CTF/verification-competition usage: angr `explore(find=...)` solves
  "magic input" challenges; KLEE/angr appear as bug finders, never as
  proof engines — the agent must not claim verification.

## Adversarial evals (compiles-but-wrong)

- An unbounded KLEE target (like `klee_unbounded.c`) passed without
  `--max-time`/`--max-instruction-time`/`--max-depth` or an input-domain
  `klee_assume` — must be flagged as hanging path explosion.
- A report claiming "verified no bugs" from a 60-second bounded KLEE run
  with no stated bound or explored-state count — overclaim.
- An angr result without concrete replay: a solver-artifact input that
  does not crash the real binary must be flagged (S1/S2 model enforces
  the replay gate).
- Making only stdin symbolic when the bug is in argv/env/file contents —
  the target must cover the real attack surface.
- Starting PE analysis at the CRT entry and trusting the unconstrained
  result (observed on this host: garbage `b'\xff\xff\xff\xff'`) instead
  of hooking/starting at the analysis function.

## Verification commands (target — KLEE/angr installed hosts)

Host (executed on this host, recorded above):

```
python examples/tools/path_exploration_model.py
gcc -g -O0 examples/good/angr_target.c -o examples/good/angr_target.exe
python examples/good/angr_solve.py
```

Target (KLEE/angr hosts; not executed here):

```
clang -I<klee>/include -emit-llvm -c examples/good/klee_target.c -o klee_target.bc
klee --max-time=60 --max-memory=1000 klee_target.bc
klee-stats klee-out-*            # explored paths, generated tests
clang -I<klee>/include -emit-llvm -c examples/bad/klee_unbounded.c -o klee_unbounded.bc
klee --max-time=60 --max-instruction-time=10 --max-memory=1000 klee_unbounded.bc
# angr (alternate host):
gcc -g -O0 examples/good/angr_target.c -o angr_target.exe
python examples/good/angr_solve.py
```

## Scoring

- Precision: high on host — the model and angr exploration produce the
  exact bug input, and the concrete-replay gate is enforced.
- Recall: high for documented workflows; KLEE-specific results are
  UNVERIFIED (no LLVM/KLEE host).
- FP-rate: low — bounded+replayed+environment-modeled reports are clearly
  distinguishable from unbounded/overclaimed/unreplayed ones.
- KNOWN: model PASS outputs, angr 9.2.213 install+run, gcc build + crash
  exit codes, angr found input (recorded above).
- INFERRED: KLEE `.err`/`.ktest` behavior and `--max-*` semantics from
  the KLEE documentation.
- UNVERIFIED: actual KLEE execution on a KLEE host.
