// BAD example 3: using a stale analysis result after mutating IR.
//
// Analysis results are cached per IR unit and are only valid for the exact IR
// they were computed on. Here the DominatorTree is queried BEFORE the pass
// inserts new instructions. After the insertion the cached DT describes the
// pre-mutation CFG, so isReachableFromEntry() answers against stale structure.
// A later pass that trusts the result can miscompile.
//
// Correct: query analyses after all mutation is complete (or re-query after
// mutating), so the analysis manager recomputes or returns a fresh result.
//
// Compile (sketch, syntax only): see llvm_sketch.h header comment.
#if defined(LLVM_SKETCH_STANDALONE)
#include "llvm_sketch.h"
#else
#include "llvm/Analysis/DominatorTree.h"
#include "llvm/Analysis/DominatorTreeAnalysis.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#endif

using namespace llvm;

namespace {

struct StaleAnalysisPass : public PassInfoMixin<StaleAnalysisPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    // DT is computed for the pre-mutation CFG...
    const DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);

    for (BasicBlock &BB : F) {
      if (!BB.empty()) {
        // ...and here the CFG is mutated (a new instruction is inserted).
        IRBuilder<> B(&BB.front());
        B.CreateAdd(nullptr, nullptr);
      }
      // BUG: DT is stale after the mutation above; it must be re-queried.
      if (DT.isReachableFromEntry(&BB)) {
      }
    }

    return PreservedAnalyses::none();
  }
};

} // end anonymous namespace
