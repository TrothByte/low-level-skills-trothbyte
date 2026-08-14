// BAD example 1: forgetting the correct PreservedAnalyses.
//
// The pass replaces every `add` instruction with a new one and erases the old,
// i.e. it mutates the function's IR. It then returns PreservedAnalyses::all(),
// which tells the pass manager that NOTHING was invalidated. The analysis
// manager keeps its cached DominatorTree/LoopInfo/AliasAnalysis results, which
// are now stale; a later pass that uses them can miscompile.
//
// Correct: return PreservedAnalyses::none() (or preserve only analyses you
// provably kept up to date). all() is legal only when the pass changed nothing
// that any analysis observes - exactly the HelloWorld case in the LLVM docs.
//
// Compile (sketch, syntax only): see llvm_sketch.h header comment.
#if defined(LLVM_SKETCH_STANDALONE)
#include "llvm_sketch.h"
#else
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#endif

using namespace llvm;

namespace {

struct ForgotPreservedAnalysesPass
    : public PassInfoMixin<ForgotPreservedAnalysesPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    std::vector<Instruction *> Candidates;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (I.getOpcode() == Instruction::Add)
          Candidates.push_back(&I);

    for (Instruction *I : Candidates) {
      IRBuilder<> B(I);
      Value *Replacement = B.CreateAdd(I->getOperand(0), I->getOperand(1));
      I->replaceAllUsesWith(Replacement);
      I->eraseFromParent();
    }

    // BUG: IR changed but every analysis is declared preserved.
    return PreservedAnalyses::all();
  }
};

} // end anonymous namespace
