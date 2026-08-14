# Evaluation — llvm-pass-writing

Skill: `skills/llvm/llvm-pass-writing`. Stability target: `evaluated`.
LLVM/clang/opt/llvm-lit are NOT installed on this host. All four C++ pass
examples compile standalone in sketch mode with g++ and were syntax-verified here
(status: VERIFIED-syntax). The real plugin build, `opt -load-pass-plugin`,
`-passes=`, and `llvm-lit` are documented as target verification.

## Synthetic evals

Each case: READ the pass source -> IDENTIFY what it does -> FLAG any
correctness bug -> STATE the fix. All claims must be grounded in the LLVM docs
(`llvm-programmers-manual`).

- **easy/positive**: read `examples/good/increment_add_constants_pass.cpp` —
  must confirm: analyses queried before mutation; collect-then-mutate (no erase
  inside the range-for); RAUW before erase; `PreservedAnalyses::none()` returned
  because IR changed; correct plugin registration in `llvmGetPassPluginInfo`.
- **easy/negative**: read `examples/bad/forgot_preserved_analyses.cpp` — must
  flag `return PreservedAnalyses::all();` after RAUW+erase (stale analyses;
  fix = `none()` or precise `preserve<>`).
- **medium/negative**: read `examples/bad/erase_while_iterating.cpp` — must flag
  `I.eraseFromParent()` inside `for (Instruction &I : BB)` (iterator
  invalidation; fix = collect first or `make_early_inc_range`).
- **hard/negative**: read `examples/bad/stale_analysis.cpp` — must flag using the
  DominatorTree obtained before inserting new instructions (stale result; fix =
  query after mutation).
- **adversarial**: extend the good skeleton with a transformation that deletes
  blocks. Must answer: which analyses are invalidated by block removal
  (DT, LoopInfo, CFG-based analyses), which `PreservedAnalyses` to return, and
  what `opt -passes=verify` would reject if the CFG is left inconsistent.

## False-positive evals (correct passes must NOT be flagged)

- A read-only pass (e.g. HelloWorld printing function names) returning
  `PreservedAnalyses::all()` — correct, do NOT flag.
- A pass that collects instructions first and mutates in a second loop — correct,
  do NOT flag.
- A pass using `llvm::make_early_inc_range(BB)` to erase during iteration — the
  intended safe idiom, do NOT flag.
- A pass that queries the DominatorTree AFTER all mutation is done — fresh result,
  do NOT flag.
- The good skeleton file itself — zero flags expected.

## Verification commands (this host — already executed)

```
g++ -std=c++17 -Wall -Wextra -Werror -c -DLLVM_SKETCH_STANDALONE -I examples examples/good/increment_add_constants_pass.cpp
g++ -std=c++17 -Wall -Wextra -Werror -c -DLLVM_SKETCH_STANDALONE -I examples examples/bad/forgot_preserved_analyses.cpp
g++ -std=c++17 -Wall -Wextra -Werror -c -DLLVM_SKETCH_STANDALONE -I examples examples/bad/erase_while_iterating.cpp
g++ -std=c++17 -Wall -Wextra -Werror -c -DLLVM_SKETCH_STANDALONE -I examples examples/bad/stale_analysis.cpp
```

All four compiled cleanly (exit 0, no warnings) on 2026-08-14.

## Verification commands (target; run on a machine with an LLVM toolchain)

```
clang++ -std=c++17 -O0 -fPIC -fno-rtti -fno-exceptions -shared \
    examples/good/increment_add_constants_pass.cpp \
    $(llvm-config --cxxflags --ldflags --system-libs --libs core support) -o /tmp/libIncAdd.so
opt -passes=verify examples/good/increment_add_constants.ll -o /dev/null
opt -load-pass-plugin=/tmp/libIncAdd.so -passes=increment-add-constants \
    -S examples/good/increment_add_constants.ll | FileCheck examples/good/increment_add_constants.ll
llvm-lit -v examples/good
```

Expected when run:
- The plugin loads and `increment-add-constants` is recognized (no
  `unknown pass name`).
- `add i32 5, %y` becomes `add i32 6, %y`; the FileCheck `add i32 6, %y` matches.
- `opt -passes=verify` stays clean before and after the pass.

## Verified facts

Status legend: VERIFIED = self-review against the LLVM docs fetched from llvm.org
(`llvm-programmers-manual`, `NewPassManager.html`, `WritingAnLLVMNewPMPass.html`)
plus syntax compile on this host; TARGET = requires an LLVM toolchain.

| Fact | Status | Source |
|---|---|---|
| New PM function pass = `PassInfoMixin<PassT>` + `PreservedAnalyses run(Function&, FunctionAnalysisManager&)` | VERIFIED | llvm-programmers-manual |
| `PreservedAnalyses::all()` is correct only if the pass changed nothing any analysis observes | VERIFIED | llvm-programmers-manual |
| `PreservedAnalyses::none()` invalidates all analyses; `preserve<T>()`/`preserveSet` give precision | VERIFIED | llvm-programmers-manual |
| Analysis results are cached per IR unit; pass manager calls `invalidate()` with the returned `PreservedAnalyses` | VERIFIED | llvm-programmers-manual |
| A cached analysis result is stale once the IR it describes is mutated | VERIFIED | llvm-programmers-manual |
| Erasing an instruction inside a range-for over the same block invalidates the iterator | VERIFIED | llvm-programmers-manual |
| Safe delete sequence is RAUW (`replaceAllUsesWith`) then `eraseFromParent` | VERIFIED | llvm-programmers-manual |
| IRBuilder inserts at its insertion point; every use must be dominated by its new definition | VERIFIED | llvm-programmers-manual; llvm-langref |
| `opt -load-pass-plugin=... -passes=name` loads and runs plugin-registered passes; `-p` aliases `-passes` | VERIFIED | llvm-programmers-manual |
| Plugin exports `extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo()` | VERIFIED | llvm-programmers-manual |
| The middle-end uses the New PM; the legacy PM is deprecated for it (exact removal version is release-dependent) | VERIFIED (deprecation); INFERRED (version) | llvm-programmers-manual |
| The four example sketches compile with g++ `-std=c++17 -Wall -Wextra -Werror -c` | VERIFIED (this host) | executed 2026-08-14 |
| The real plugin builds, runs under opt, and passes the FileCheck test | TARGET | documented |
| `llvm-lit` executes RUN lines and checks exit codes | TARGET | llvm-programmers-manual (testing) |

## Scoring (for routing eval)

- precision: every `PreservedAnalyses`, iteration-safety, and staleness claim must
  match the LLVM docs; no over-flagging of `all()` in a read-only pass.
- recall: all three bad examples' bug classes must be caught.
- FP-rate: the false-positive list and the good skeleton must yield zero flags.
