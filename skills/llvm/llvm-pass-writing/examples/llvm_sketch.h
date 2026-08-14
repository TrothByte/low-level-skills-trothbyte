// llvm_sketch.h - TEACHING SKETCH of the LLVM New Pass Manager API surface.
//
// Purpose: the pass examples in this skill compile standalone with only the C++
// standard library, so their syntax can be checked on any host (this one has no
// LLVM toolchain). The names mirror llvm/*.h exactly; the behavior is a no-op
// model. The same source compiles against a real LLVM by OMITTING
// LLVM_SKETCH_STANDALONE (see examples/good/increment_add_constants_pass.cpp).
//
// Standalone syntax check (from the skill root):
//   g++ -std=c++17 -Wall -Wextra -Werror -c -DLLVM_SKETCH_STANDALONE
//       -I examples examples/good/increment_add_constants_pass.cpp
#ifndef LLVM_SKETCH_H
#define LLVM_SKETCH_H

#include <algorithm>
#include <string>
#include <vector>

#ifndef LLVM_ATTRIBUTE_WEAK
#define LLVM_ATTRIBUTE_WEAK __attribute__((weak))
#endif
#ifndef LLVM_PLUGIN_API_VERSION
#define LLVM_PLUGIN_API_VERSION 0
#endif
#ifndef LLVM_VERSION_STRING
#define LLVM_VERSION_STRING "sketch"
#endif

namespace llvm {

struct Value {};

struct BasicBlock;

struct Instruction : Value {
  enum Opcode { Add, Other };
  Opcode Op = Other;
  std::string Name;
  BasicBlock *Parent = nullptr;
  std::vector<Value *> Operands;

  Opcode getOpcode() const { return Op; }
  const std::string &getName() const { return Name; }
  Value *getOperand(unsigned i) const {
    return i < Operands.size() ? Operands[i] : nullptr;
  }
  void replaceAllUsesWith(Value *) {}
  void eraseFromParent();
};

struct BasicBlock {
  std::string Name;
  std::vector<Instruction> Instrs;
  auto begin() { return Instrs.begin(); }
  auto end() { return Instrs.end(); }
  bool empty() const { return Instrs.empty(); }
  Instruction &front() { return Instrs.front(); }
  Instruction &addInstruction(Instruction::Opcode Op, const std::string &Name = "") {
    Instrs.emplace_back();
    Instruction &I = Instrs.back();
    I.Op = Op;
    I.Name = Name;
    I.Parent = this;
    return I;
  }
};

inline void Instruction::eraseFromParent() {
  if (!Parent) return;
  auto &V = Parent->Instrs;
  auto It = std::find_if(V.begin(), V.end(),
                         [this](const Instruction &I) { return &I == this; });
  if (It != V.end()) {
    V.erase(It);
    Parent = nullptr;
  }
}

struct Function {
  std::string Name;
  std::vector<BasicBlock> Blocks;
  auto begin() { return Blocks.begin(); }
  auto end() { return Blocks.end(); }
  const std::string &getName() const { return Name; }
  BasicBlock &front() { return Blocks.front(); }
};

struct DominatorTree {
  bool isReachableFromEntry(const BasicBlock *) const { return true; }
};

struct DominatorTreeAnalysis {
  using Result = DominatorTree;
};

struct LoopAnalysis {
  struct LoopInfo {
    unsigned getNumLoops() const { return 0; }
  };
  using Result = LoopInfo;
};

struct AAManager {
  struct AAResults {};
  using Result = AAResults;
};

struct PreservedAnalyses {
  static PreservedAnalyses all() { return PreservedAnalyses{}; }
  static PreservedAnalyses none() { return PreservedAnalyses{}; }
  template <typename AnalysisT> void preserve() {}
};

struct FunctionAnalysisManager {
  template <typename AnalysisT>
  typename AnalysisT::Result &getResult(Function &) {
    static typename AnalysisT::Result R;
    return R;
  }
  template <typename AnalysisT> void invalidate(Function &) {}
};

template <typename FolderTy = void, typename InserterTy = void>
struct IRBuilder {
  template <typename T> explicit IRBuilder(T *) {}
  Value *CreateAdd(Value *LHS, Value *RHS, const char *Name = "") {
    (void)LHS; (void)RHS; (void)Name;
    return new Instruction();
  }
  Value *Create(Value *, const char *Name = "") {
    (void)Name;
    return new Instruction();
  }
};

template <typename PassT> struct PassInfoMixin {};

struct StringRef {
  std::string S;
  StringRef(const char *s) : S(s) {}
  bool operator==(const char *s) const { return S == s; }
};

template <typename T> struct ArrayRef {};

struct FunctionPassManager {
  template <typename PassT> void addPass(PassT) {}
};

struct PassBuilder {
  struct PipelineElement {};
  template <typename CallbackT> void registerPipelineParsingCallback(CallbackT) {}
};

struct PassPluginLibraryInfo {
  const unsigned APIVersion;
  const char *PluginName;
  const char *PluginVersion;
  void (*RegisterPassBuilderCallbacks)(PassBuilder &);
};

} // namespace llvm

#endif
