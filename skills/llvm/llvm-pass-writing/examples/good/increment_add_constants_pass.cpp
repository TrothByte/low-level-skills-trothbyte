// GOOD example: a correct New Pass Manager function pass skeleton.
//
// Transformation: for every `add` instruction whose first operand is an integer
// constant, replace the add with a new add whose constant is incremented by one.
//
// This file builds two ways:
//   1. Standalone sketch (no LLVM needed) - syntax check on any host:
//        g++ -std=c++17 -Wall -Wextra -Werror -c -DLLVM_SKETCH_STANDALONE
//            -I examples examples/good/increment_add_constants_pass.cpp
//   2. Real out-of-tree LLVM plugin (target machine with an LLVM toolchain):
//        clang++ -std=c++17 -O0 -fPIC -fno-rtti -fno-exceptions -shared
//            examples/good/increment_add_constants_pass.cpp
//            $(llvm-config --cxxflags --ldflags --system-libs --libs core support)
//            -o libIncAdd.so
// The pass body is identical in both modes; only the include block and the
// constant-increment detail differ. The lit/FileCheck test that exercises the
// real plugin is examples/good/increment_add_constants.ll.
#if defined(LLVM_SKETCH_STANDALONE)
#include "llvm_sketch.h"
#else
#include "llvm/Analysis/DominatorTree.h"
#include "llvm/Analysis/DominatorTreeAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#endif

using namespace llvm;

namespace {

// New Pass Manager shape: a CRTP mixin base + a run() that returns
// PreservedAnalyses and takes the IR unit and its analysis manager.
struct IncrementAddConstantsPass
    : public PassInfoMixin<IncrementAddConstantsPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    // TEACHING: query analyses BEFORE mutating IR. A result obtained here is
    // only valid while the IR it describes is unchanged.
    const DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);

    // TEACHING: collect candidates first, mutate second. Erasing an instruction
    // inside the range-for below would invalidate the loop iterator.
    std::vector<Instruction *> Candidates;
    for (BasicBlock &BB : F) {
      if (!DT.isReachableFromEntry(&BB))
        continue;
      for (Instruction &I : BB)
        if (I.getOpcode() == Instruction::Add)
          Candidates.push_back(&I);
    }

    for (Instruction *I : Candidates) {
      IRBuilder<> B(I);
#if defined(LLVM_SKETCH_STANDALONE)
      Value *LHS = I->getOperand(0);
#else
      auto *CI = dyn_cast<ConstantInt>(I->getOperand(0));
      Value *LHS = CI ? ConstantInt::get(CI->getType(), CI->getSExtValue() + 1)
                      : I->getOperand(0);
#endif
      // Safe replacement sequence: create, RAUW, erase. SSA stays valid because
      // the replacement is created at (and dominates) the old instruction's
      // position, so every use remains dominated by its new definition.
      Value *Replacement = B.CreateAdd(LHS, I->getOperand(1));
      I->replaceAllUsesWith(Replacement);
      I->eraseFromParent();
    }

    // TEACHING: the IR changed, so no analysis result can be assumed valid.
    // Returning PreservedAnalyses::none() invalidates everything (safe but
    // conservative). Returning all() here would leave stale analyses behind.
    return PreservedAnalyses::none();
  }
};

} // end anonymous namespace

PassPluginLibraryInfo getPassPluginInfo() {
  const auto Callback = [](PassBuilder &PB) {
    // TEACHING: register the pass by name so `opt -passes=increment-add-constants`
    // can find it. The inner lambda receives the pass name string.
    PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "increment-add-constants") {
            FPM.addPass(IncrementAddConstantsPass());
            return true;
          }
          return false;
        });
  };
  return {LLVM_PLUGIN_API_VERSION, "IncrementAddConstants", LLVM_VERSION_STRING,
          Callback};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
