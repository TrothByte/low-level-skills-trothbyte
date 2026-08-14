// BAD example 2: mutating IR while iterating over it.
//
// eraseFromParent() destroys the instruction and the block's instruction list
// entry, so the range-for iterator over the same BasicBlock is invalidated.
// The loop then dereferences a dangling iterator - undefined behavior. The
// sketch compiles only because its eraseFromParent is a stand-in; against real
// LLVM this is a use-after-free-class bug (assertions or crashes in practice).
//
// Correct patterns:
//   - collect the instructions first, mutate after the loop (see
//     examples/good/increment_add_constants_pass.cpp); or
//   - iterate with make_early_inc_range(BB) so the iterator is advanced before
//     the body runs.
//
// Compile (sketch, syntax only): see llvm_sketch.h header comment.
#if defined(LLVM_SKETCH_STANDALONE)
#include "llvm_sketch.h"
#else
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#endif

using namespace llvm;

namespace {

struct EraseWhileIteratingPass : public PassInfoMixin<EraseWhileIteratingPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (I.getOpcode() == Instruction::Add) {
          // BUG: erasing inside the range-for invalidates the loop iterator.
          I.eraseFromParent();
        }
      }
    }
    return PreservedAnalyses::none();
  }
};

} // end anonymous namespace
