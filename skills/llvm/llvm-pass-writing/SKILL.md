---
name: llvm-pass-writing
description: Use when writing, reviewing, or testing LLVM optimization passes in C++ — New Pass Manager structure, PassInfoMixin run methods, PreservedAnalyses correctness, analysis invalidation, IRBuilder usage, SSA-safe IR mutation, opt -passes= integration, and lit/FileCheck testing.
---

# Writing LLVM Passes

## When to use

- Writing a new LLVM IR transformation or analysis pass (function, loop, or module level) for `opt`, `clang -fpass-plugin`, or an out-of-tree LLVM build.
- Reviewing pass code for correctness: `PreservedAnalyses` handling, iteration safety, stale analyses, SSA validity.
- Building and testing a pass end-to-end: plugin registration, `opt -passes=name`, lit/FileCheck.
- Diagnosing "unknown pass name", verifier failures, or miscompiles caused by a pass.

## When not to use

- Reading or interpreting IR without writing a pass — use `llvm-ir-reading` (this skill requires it).
- GCC/GIMPLE or RTL optimization — use `gcc-manual`-grounded skills; GCC does not emit LLVM IR.
- Writing assembly, calling conventions, or target lowering — IR passes operate above the target.
- Clang AST or source-level refactoring — that is frontend work, not a pass.

## What the agent often gets wrong

- Returning `PreservedAnalyses::all()` after mutating IR; it is only correct when nothing changed. Mutating IR must return `PreservedAnalyses::none()` (or precisely preserve what stayed valid).
- Erasing instructions inside `for (Instruction &I : BB)` — `eraseFromParent()` invalidates the iterator.
- Holding an analysis result (`DominatorTree`, `LoopInfo`, `AAResults`) across IR mutation and using it afterwards — the cached result is stale.
- Writing legacy-pass-manager code (`runOnFunction`, `getAnalysisUsage`) — the middle-end runs the New PM only.
- Deleting instructions without RAUW or creating values whose definition does not dominate their uses — SSA violations caught only by `verify`.
- Forgetting `registerPipelineParsingCallback` / the weak `extern "C" llvmGetPassPluginInfo` export, then `opt` says the pass name is unknown or no plugin was found.
- Testing without lit/FileCheck, or anchoring CHECK lines on auto-generated SSA names.

## How to reason correctly

1. Write the New PM shape: `struct P : PassInfoMixin<P> { PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM); };`.
2. Plan the mutation strategy before writing: what changes, which analyses it affects, and the collection/mutation order.
3. Query analyses you need for decisions BEFORE mutation; after mutation re-query or rely on invalidation.
4. Return `PreservedAnalyses::none()` whenever IR changed; `all()` only for a genuinely read-only pass.
5. Keep IR well-formed: create replacements at the old instruction's position, RAUW, then erase; run `verify`.
6. Register the pass by name and exercise it with `opt -load-pass-plugin=... -passes=name`, with a lit/FileCheck test.
7. Mark uncertainty: if you cannot verify a claim about a specific LLVM version, label it INFERRED and check against that version's docs.

## What to verify

- `run()` returns `PreservedAnalyses::none()` (or a precise set) whenever the pass mutates IR.
- No erase or insert happens inside a range-for over the same block/instruction list.
- No analysis result is used after an IR mutation without re-querying it.
- Every new instruction's operands dominate the insertion point; `opt -passes=verify` stays clean.
- The plugin exports `llvmGetPassPluginInfo` and registers the pipeline name; `opt -passes=<name>` actually runs the pass.
- A lit test with FileCheck covers the transformation and rejects regressions.

## How to verify

```
# Syntax check on any host (sketch mode; LLVM not required):
g++ -std=c++17 -Wall -Wextra -Werror -c -DLLVM_SKETCH_STANDALONE \
    -I examples examples/good/increment_add_constants_pass.cpp

# Target verification (machine with an LLVM toolchain):
clang++ -std=c++17 -O0 -fPIC -fno-rtti -fno-exceptions -shared \
    examples/good/increment_add_constants_pass.cpp \
    $(llvm-config --cxxflags --ldflags --system-libs --libs core support) -o /tmp/libIncAdd.so
opt -passes=verify examples/good/increment_add_constants.ll -o /dev/null
opt -load-pass-plugin=/tmp/libIncAdd.so -passes=increment-add-constants \
    -S examples/good/increment_add_constants.ll | FileCheck examples/good/increment_add_constants.ll
llvm-lit -v skills/llvm/llvm-pass-writing/examples/good
```

clang/opt/llvm-lit are NOT installed on this host; the g++ sketch compile above
is the verified-on-this-host check, and the opt/lit commands are the documented
target verification.

## Where the knowledge comes from

- `llvm-programmers-manual` — pass writing, New Pass Manager, `PreservedAnalyses`, analyses, IRBuilder, plugin registration.
- `llvm-langref` — SSA well-formedness, dominance, instruction semantics (what a pass must preserve).
- `clang-docs` — frontend pass integration (`-fpass-plugin`) and LLVM test-suite usage.
- `gcc-manual` — negative grounding: GCC's pipeline is GIMPLE/RTL, not LLVM passes.

## Related skills

- `llvm-ir-reading` — required prerequisite: read IR correctly before transforming it.
- `c-undefined-behavior` — C UB maps to IR poison/nsw patterns a pass must not introduce.
- `compiler-ub-assumptions` — optimizer assumptions a pass must respect when creating IR.

## Evaluation

Synthetic: review the three `examples/bad/*.cpp` passes and flag the bug class in
each (forgotten PreservedAnalyses; erase-while-iterating; stale analysis result);
verify the good skeleton's patterns.
False-positive: a read-only pass returning `PreservedAnalyses::all()` and a pass
that uses `make_early_inc_range` must NOT be flagged; the good skeleton must NOT
be flagged.
Adversarial: extend the good skeleton with a new transformation and answer which
analyses must be invalidated and whether the CHECK lines survive reordering.
Commands and verification status: see `evals/README.md`.
