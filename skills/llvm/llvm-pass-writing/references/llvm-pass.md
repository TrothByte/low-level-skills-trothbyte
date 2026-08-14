# Writing LLVM Passes — Reference Rules

Source-grounded rules for writing New Pass Manager passes in C++. Each rule:
RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE (bad) →
COUNTEREXAMPLE (good) → VERIFICATION → SOURCE.
Registry sources: `llvm-programmers-manual` (pass writing, New PM), `llvm-langref`
(IR semantics, SSA well-formedness), `clang-docs` (frontend integration).
Facts are version-aware for LLVM 17+; "target verification" means a command to
run on a machine with an LLVM toolchain (not present on this host).

## 1. Legacy vs New Pass Manager

- **RULE**: write New Pass Manager passes. The middle-end optimization pipeline
  runs on the New PM (`Module → Function → Loop` IR hierarchy); the legacy PM is
  deprecated for it. Legacy-style APIs (`runOnFunction`, `getAnalysisUsage`,
  `getAnalysis<T>()`) must not be used for new IR optimization passes.
- **WHY AI GETS IT WRONG**: older tutorials and most web snippets show the legacy
  PM (`bool runOnFunction(Function &)`); agents copy `getAnalysisUsage` or expect
  `-enable-new-pm=0` to exist. Mixing the two APIs fails to compile or silently
  registers nothing.
- **CORRECT REASONING**: a New PM pass is a plain struct with a CRTP mixin base
  and a `run()` returning `PreservedAnalyses`. The pass manager type must match
  the IR unit: `FunctionPassManager` holds function passes, wrapped by
  `createModuleToFunctionPassAdaptor` in a module pipeline. `opt` runs the New PM
  pipeline only; there is no legacy `-passes=` path anymore.
- **EXAMPLE** (bad): `struct FooPass { bool runOnFunction(Function &) { return true; } };`
  — no `PassInfoMixin`, no `PreservedAnalyses`, nothing `opt` can run.
- **COUNTEREXAMPLE** (good):
  `struct FooPass : public PassInfoMixin<FooPass> { PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM); };`
- **VERIFICATION**: `opt -passes=foo` runs the pass; legacy artifacts are absent
  from the source. `opt --print-passes` lists registered New PM pass names.
- **SOURCE**: `llvm-programmers-manual` (pass writing; New PM status). INFERRED:
  exact legacy-PM removal version varies by LLVM release — check `NewPassManager.html`
  for the current status.

## 2. Pass structure: `PassInfoMixin` and `run()`

- **RULE**: a function pass inherits `public PassInfoMixin<PassT>` and defines
  `PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM)`. The CRTP
  argument is the pass's own type. `run()` receives the IR unit and its analysis
  manager and must return a `PreservedAnalyses`. Newer LLVM versions add
  `OptionalPassInfoMixin`/`RequiredPassInfoMixin` on top of `PassInfoMixin`
  (version-dependent; `PassInfoMixin` remains the base).
- **WHY AI GETS IT WRONG**: writes `PassInfoMixin<SomethingElse>`, forgets the
  `run()` signature (wrong parameter types), or makes `run` return `bool`.
- **CORRECT REASONING**: the mixin base only adds boilerplate (pass name,
  `isRequired`); the contract is the `run()` signature. `F` is the current
  function, `FAM` gives access to cached function-level analyses. The pass
  manager runs `run()` on every function in the module (skipping `optnone`
  functions unless the pass is required).
- **EXAMPLE** (bad): `PreservedAnalyses run(Module &M, FunctionAnalysisManager &)` —
  wrong IR unit for a function pass; `opt -passes=foo` fails with a nesting error.
- **COUNTEREXAMPLE** (good): `PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM)`
  with the pass body inside, returning `PreservedAnalyses::none()` after mutation.
- **VERIFICATION**: compiles against LLVM headers; `opt -passes=foo` runs it.
  Sketch compile on this host:
  `g++ -std=c++17 -Wall -Wextra -Werror -c -DLLVM_SKETCH_STANDALONE -I examples examples/good/increment_add_constants_pass.cpp`.
- **SOURCE**: `llvm-programmers-manual` (pass writing). INFERRED: the exact mixin
  spelling (`OptionalPassInfoMixin` vs `PassInfoMixin`) depends on LLVM version.

## 3. `PreservedAnalyses`: say `none()` when you mutate

- **RULE**: `run()` reports which analysis results remain valid. `PreservedAnalyses::all()`
  is correct ONLY if the pass changed nothing that any analysis observes.
  `PreservedAnalyses::none()` invalidates every analysis (safe but conservative).
  Precision is `PreservedAnalyses PA; PA.preserve<DominatorTreeAnalysis>(); return PA;`
  or `PA.preserveSet<CFGAnalyses>();`. The pass manager calls the analysis
  manager's `invalidate()` with the returned set; anything not preserved is
  discarded and recomputed on the next `getResult`.
- **WHY AI GETS IT WRONG**: treats `PreservedAnalyses` as a "did I run OK?"
  boolean; returns `all()` after creating/erasing instructions; or returns `none()`
  from a pure analysis pass (wasteful but correct). The classic miscompile: IR
  changed, `all()` returned, a later pass reads the stale DominatorTree.
- **CORRECT REASONING**: the return value is the pass's *invalidation contract*.
  Any IR change invalidates at least the analyses that depend on the changed
  structure (CFG analyses, DT, AA, LoopInfo). If in doubt, return `none()`.
- **EXAMPLE** (bad): `examples/bad/forgot_preserved_analyses.cpp` — RAUWs and
  erases instructions, then `return PreservedAnalyses::all();`.
- **COUNTEREXAMPLE** (good): `examples/good/increment_add_constants_pass.cpp` —
  mutates IR, `return PreservedAnalyses::none();`. A pure analysis pass that
  changes nothing may return `PreservedAnalyses::all()` (HelloWorld pattern).
- **VERIFICATION**: run the pass with a following analysis-using pass under
  `opt -passes='foo,gvn'` and compare; `-debug-pass-manager` prints what is
  invalidated/recomputed. Sketch compile is syntax-only.
- **SOURCE**: `llvm-programmers-manual` (New PM, `PreservedAnalyses`);
  `NewPassManager.html` semantics via `llvm-programmers-manual`. INFERRED:
  analysis-type names (e.g. `DominatorTreeAnalysis` vs newer `DominatorAnalysis`)
  vary by LLVM version.

## 4. Analysis managers: caching, results, and staleness

- **RULE**: `FAM.getResult<DominatorTreeAnalysis>(F)` returns the (cached or
  freshly computed) `DominatorTree &`; `FAM.getResult<LoopAnalysis>(F)` returns
  `LoopInfo &`; `FAM.getResult<AAManager>(F)` returns `AAResults &`. Results are
  cached per IR unit and invalidated via the pass's returned `PreservedAnalyses`.
  A result is valid only for the exact IR it was computed on; mutating the IR
  makes every previously obtained result stale.
- **WHY AI GETS IT WRONG**: holds `const DominatorTree &DT` across IR mutations
  and keeps using it; calls `FAM.getResult` expecting a fresh value each time
  (it is cached); queries analyses for the wrong IR unit.
- **CORRECT REASONING**: plan the pass as query → mutate → (optionally re-query).
  Query analyses you need for decisions BEFORE mutation; after mutation either
  re-query (the analysis manager recomputes on demand) or rely on invalidation.
  Never mix a pre-mutation result with post-mutation IR.
- **EXAMPLE** (bad): `examples/bad/stale_analysis.cpp` — gets `DT`, inserts
  instructions, then calls `DT.isReachableFromEntry(&BB)`.
- **COUNTEREXAMPLE** (good): `examples/good/increment_add_constants_pass.cpp` —
  uses `DT` only while collecting candidates, before any instruction is created.
- **VERIFICATION**: run under `opt -passes='foo,verify'`; use `-debug-pass-manager`
  to watch invalidation/recomputation of `domtree` after the pass. Sketch compile
  is syntax-only.
- **SOURCE**: `llvm-programmers-manual` (analyses, New PM analysis manager);
  `llvm-langref` (dominator and SSA well-formedness).

## 5. Mutating IR while iterating

- **RULE**: never erase from (or insert into) a container while range-for-iterating
  the same container. `I.eraseFromParent()` destroys the instruction and the
  iterator; the next loop iteration dereferences a dangling iterator — UB. Collect
  the target instructions first and mutate in a second loop, or iterate with
  `llvm::make_early_inc_range(BB)`.
- **WHY AI GETS IT WRONG**: writes `for (Instruction &I : BB) { ...; I.eraseFromParent(); }`
  from memory; the sketch compiles, the real LLVM run crashes or corrupts.
- **CORRECT REASONING**: a range-for holds one iterator; erasing the element it
  points to invalidates it. Deferred mutation (collect pointers, then act) keeps
  the loop stable and is the pattern in real LLVM passes.
- **EXAMPLE** (bad): `examples/bad/erase_while_iterating.cpp` — `I.eraseFromParent()`
  inside `for (Instruction &I : BB)`.
- **COUNTEREXAMPLE** (good): `examples/good/increment_add_constants_pass.cpp` —
  pushes `&I` into `Candidates`, then loops over `Candidates` to mutate.
- **VERIFICATION**: run the bad pass under `opt` (assertions/crash); run the good
  pass with `opt -passes=...` followed by `verify`. Sketch compile is syntax-only.
- **SOURCE**: `llvm-programmers-manual` (`make_early_inc_range`, STL-iteration
  guidance).

## 6. `IRBuilder`: insertion points and creation

- **RULE**: `IRBuilder<> B(&SomeInstruction);` sets the insertion point BEFORE that
  instruction; `IRBuilder<> B(BB);` inserts at the end of the block;
  `B.SetInsertPoint(OtherInst)` moves it later. Every `B.CreateX(...)` emits one
  instruction at the current point. The insertion point must dominate all uses of
  the created value, and the created value must be reachable from its operands.
- **WHY AI GETS IT WRONG**: creates instructions without setting a valid insertion
  point (verifier error "Instruction does not dominate all uses"); inserts after a
  terminator; assumes the builder inserts at "the end of the function".
- **CORRECT REASONING**: the builder is a cursor. Create the replacement at the
  OLD instruction's position so the new value dominates every use of the old one,
  then RAUW. A value inserted in a dead/unreachable block or after its uses is
  invalid SSA.
- **EXAMPLE** (bad): `IRBuilder<> B(&F.getEntryBlock().back()); B.CreateAdd(x, y);`
  where the block's back is a `ret` terminator — the add is inserted after the
  terminator (malformed IR).
- **COUNTEREXAMPLE** (good): `IRBuilder<> B(I); Value *R = B.CreateAdd(LHS, RHS);`
  with `I` the instruction being replaced, then `I->replaceAllUsesWith(R)`.
- **VERIFICATION**: `opt -passes=foo,verify` on transformed IR; any insertion
  mistake is reported by the verifier.
- **SOURCE**: `llvm-programmers-manual` (IRBuilder); `llvm-langref`
  (well-formed IR, dominance).

## 7. SSA invalidation when modifying IR

- **RULE**: IR is SSA: each value is defined once and every use must be dominated
  by its definition. Changing an instruction's operands, erasing it, or creating
  new values can break def-use chains. The safe delete sequence is
  `V->replaceAllUsesWith(Replacement)` (RAUW) then `V->eraseFromParent()`; changing
  one operand uses `I->setOperand(i, NewVal)`. After any modification, run the
  verifier.
- **WHY AI GETS IT WRONG**: erases an instruction while its uses remain (dangling
  uses); creates a value whose definition does not dominate its uses; forgets that
  `getResult` results and SSA values must both stay consistent.
- **CORRECT REASONING**: every transformation is a rewrite of the def-use graph.
  RAUW first redirects all uses to a value that is defined (ideally at the same
  position), erase second removes the now-dead definition. `verify` is the
  oracle: `opt -passes=verify` rejects dominance/def-use violations.
- **EXAMPLE** (bad): `I->eraseFromParent();` without RAUW while `%r` still has
  users — verifier error "Instruction does not dominate all uses" or dangling use.
- **COUNTEREXAMPLE** (good): `examples/good/increment_add_constants_pass.cpp` —
  `Replacement = B.CreateAdd(...); I->replaceAllUsesWith(Replacement); I->eraseFromParent();`.
- **VERIFICATION**: `opt -passes=foo,verify` exits nonzero on a broken
  transformation; the good example keeps `opt -passes=verify` clean.
- **SOURCE**: `llvm-langref` (SSA form, well-formedness); `llvm-programmers-manual`
  (replacing/deleting instructions).

## 8. Integrating with `opt -passes=`: plugins and registration

- **RULE**: for out-of-tree use, export a `PassPluginLibraryInfo` via
  `extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo()`,
  and inside it register the pass name with
  `PB.registerPipelineParsingCallback([](StringRef Name, FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>) { if (Name == "my-pass") { FPM.addPass(MyPass()); return true; } return false; });`.
  Then `opt -load-pass-plugin=libMyPass.so -passes=my-pass` runs it.
- **WHY AI GETS IT WRONG**: forgets `registerPipelineParsingCallback` (pass name
  never registered → `opt: unknown pass name 'my-pass'`); misses the weak `extern "C"`
  symbol so `opt` reports no plugin found; registers a function pass into a module
  pass manager slot (nesting error: `unknown function pass`).
- **CORRECT REASONING**: `PassPluginLibraryInfo` carries the API version, names,
  and a callback that `PassBuilder` invokes once at load. The pipeline-parsing
  callback maps the `-passes=` name to `FPM.addPass(...)` and returns `true` to
  claim the name. `LLVM_PLUGIN_API_VERSION` and `LLVM_VERSION_STRING` come from
  `llvm/Passes/PassPlugin.h`. For in-tree builds, register in `PassRegistry.def`
  or use `add_llvm_pass_plugin`.
- **EXAMPLE** (bad): a plugin that defines the pass but omits
  `registerPipelineParsingCallback` — `opt -passes=my-pass` fails with
  `unknown pass name`.
- **COUNTEREXAMPLE** (good): `examples/good/increment_add_constants_pass.cpp`
  `llvmGetPassPluginInfo()` + `getPassPluginInfo()` callback; tested by
  `examples/good/increment_add_constants.ll`.
- **VERIFICATION** (target): build the plugin with clang++ `-shared -fPIC`, then
  `opt -load-pass-plugin=libIncAdd.so -passes=increment-add-constants -S in.ll`.
  Sketch compile on this host is syntax-only.
- **SOURCE**: `llvm-programmers-manual` (pass writing, plugin registration);
  `clang-docs` (frontend use of `-fpass-plugin` and pass plugins).

## 9. Testing with lit and FileCheck

- **RULE**: every transformation pass needs a lit test. A test file is IR with
  `; RUN:` lines executed by lit and `; CHECK:`/`; CHECK-NOT:`/`; CHECK-LABEL:`
  patterns matched by FileCheck on the command output. Run with
  `llvm-lit path/to/test` or `ninja check-llvm`.
- **WHY AI GETS IT WRONG**: tests only "compiles" or manual `opt` runs; CHECK lines
  anchored on unstable SSA names; no RUN line at all; forgot `-S`/`-o /dev/null`
  flags.
- **CORRECT REASONING**: the RUN line invokes `opt -passes=my-pass -S %s` and pipes
  to `FileCheck %s`; CHECK-LABEL anchors on function headers, CHECK matches
  substrings in order, CHECK-NOT asserts absence. Anchor on structure, not on
  auto-generated value names (`%a` after RAUW is not guaranteed). Always include a
  `verify` pass in the pipeline or a separate `opt -passes=verify` RUN.
- **EXAMPLE** (bad): `; CHECK: %a = add i32 6, %y` — the RAUW replacement gets an
  auto-generated name, so the test fails or is brittle.
- **COUNTEREXAMPLE** (good): `examples/good/increment_add_constants.ll` —
  `; CHECK-LABEL: define i32 @f(i32 %y)` then `; CHECK: add i32 6, %y`, plus a
  `opt -passes=verify` RUN.
- **VERIFICATION** (target): `llvm-lit -v examples/good/increment_add_constants.ll`.
  On this host lit/opt are absent; the test is documented as target verification.
- **SOURCE**: `llvm-programmers-manual` (pass testing); `clang-docs` (test suite
  usage). INFERRED: exact lit invocation flags follow `llvm.org/docs/TestingGuide.html`.
