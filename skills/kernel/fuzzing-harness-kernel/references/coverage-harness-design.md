# Kernel Fuzzing: Coverage Feedback and Harness Design

## 1. Coverage feedback is what makes a fuzzer a fuzzer

- **RULE**: coverage-guided fuzzing (libFuzzer, AFL++, syzkaller) selects
  new inputs by the new coverage they produce. Without the feedback channel
  (KCOV in the kernel, sanitizer-coverage in user space) the run degrades
  to random testing, which misses structured bugs. KNOWN (libFuzzer/AFL
  docs; syzkaller internals).
- **WHY AI GETS IT WRONG**: agents run a harness without enabling KCOV or
  coverage instrumentation, observe "no crashes", and conclude the target
  is fuzz-clean — the "ran for 24h, no crashes" illusion (B2).
- **CORRECT REASONING**: verify the feedback loop end-to-end before
  trusting any run: KCOV enabled, the fuzzer exposes coverage in its
  dashboard, and new inputs are added to the corpus for the coverage they
  add.
- **EXAMPLE** (bad): `examples/bad/naive_fuzzer.py` — 1M random inputs, no
  coverage, bug never reached, "no crashes" reported.
- **COUNTEREXAMPLE** (good): `examples/good/coverage_fuzzer.py` — inputs
  are kept/mutated by the new coverage they reach; the injected bug is
  found within a small budget.
- **VERIFICATION**: `python examples/good/coverage_fuzzer.py` (finds bug,
  exit 0); `python examples/bad/naive_fuzzer.py` (misses it, exit 0 —
  masked).
- **SOURCE**: libfuzzer-docs; aflpp-docs; syzkaller-docs (internals)
  [proposed]; kernel-source (kcov).

## 2. LLVMFuzzerTestOneInput must be stateless per input

- **RULE**: one process runs the entry function for many inputs. Global
  state, buffers, and handles from a previous input must be reset, and
  sizes guarded (`size == 0` must not dereference `data[0]`). Violating
  this produces harness crashes and cross-input contamination. KNOWN
  (libFuzzer contract).
- **WHY AI GETS IT WRONG**: the harness is written like a `main(argc,
  argv)` one-shot, so state leaks across inputs and the crash triage
  blames the target.
- **CORRECT REASONING**: structure the entry as init-per-input: guard
  length, reset globals, allocate/free per call; add a self-test corpus.
- **EXAMPLE** (bad): `examples/bad/LLVMFuzzer_harness_bad.c` dereferences
  `data[0]` on `size==0` and never resets its parser state.
- **COUNTEREXAMPLE** (good): `examples/good/LLVMFuzzer_harness.c` guards
  size, resets a state struct per input, and frees on exit.
- **VERIFICATION**: `gcc -Wall -Wextra -Werror -O2 -c` both; the size-guard
  diff is the ablation.
- **SOURCE**: libfuzzer-docs (entry point, corpus, zero-input handling).

## 3. Stateful targets need setup and reset, not just fuzz calls

- **RULE**: a target with configuration (a mounted fs, an opened device, a
  connected socket) must be set up before fuzzing and, where state
  accumulates, reset or isolated per input; otherwise later inputs run on
  corrupted state. KNOWN (OSS-Fuzz/kernel fuzzing practice).
- **WHY AI GETS IT WRONG**: the harness calls the parse function directly,
  skipping the init that real callers perform.
- **CORRECT REASONING**: replicate the real call sequence in the harness
  (mount/open/configure then fuzz), and isolate long-lived state.
- **EXAMPLE** (bad): fuzzing a filesystem's write path on an un-mounted
  in-memory fs.
- **COUNTEREXAMPLE** (good): the harness mounts (or uses the kernel's
  test-fs API) before each fuzz iteration.
- **VERIFICATION**: the target-side commands exercise the setup path under
  KASAN (documented).
- **SOURCE**: oss-fuzz (target authoring); libfuzzer-docs.

## 4. A harness that cannot fail certifies nothing (ablation)

- **RULE**: before trusting any fuzz run, inject a known bug into the target
  and require the harness to find it (meta-verification-harness-validity:
  ablation-delta). A harness that never crashes even with a planted defect
  is not testing the target.
- **WHY AI GETS IT WRONG**: the harness is "validated" only on the clean
  target; a harness with an unreachable target path or disabled feedback
  reports green forever.
- **CORRECT REASONING**: the ablation bug must produce a crash and the
  minimized reproducer must still trigger it; only then does the harness
  certify the clean run.
- **EXAMPLE** (bad): `bad/naive_fuzzer.py` never reaches the bug even when
  it is trivially present.
- **COUNTEREXAMPLE** (good): `good/coverage_fuzzer.py` finds the planted
  bug in a bounded number of iterations.
- **VERIFICATION**: both fixtures run on the same target; coverage is the
  only difference.
- **SOURCE**: meta-verification-harness-validity (arxiv-2606-20128;
  arxiv-2607-00107).
