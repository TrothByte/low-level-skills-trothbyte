# Reading LLVM IR — Reference Rules

Source-grounded rules for reading LLVM IR. Each rule: RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.
Registry sources: `llvm-langref` (LLVM Language Reference Manual), `clang-docs`
(Clang docs), `gcc-manual` (GCC manual). "Target verification" means the command
must be run on a machine with an LLVM toolchain (not present on this host).

## 1. SSA form and value immutability

- **RULE**: LLVM IR is Static Single Assignment: every instruction defines one new
  immutable value; a value is defined exactly once and all uses must be dominated by
  the definition. Local values start with `%`, globals with `@`. Memory is never
  mutated by arithmetic — only by `load`/`store`/`alloca` and their intrinsic forms.
- **WHY AI GETS IT WRONG**: reads `%r = add i32 %x, 1` as "adds 1 to the variable x"
  or assumes a later instruction can redefine `%x`. Expects source-level mutation.
- **CORRECT REASONING**: trace pure dataflow. A C "variable" becomes a chain of SSA
  values; an update is a new value, and a store is needed to change memory. The
  verifier rejects a use not dominated by its definition.
- **EXAMPLE** (bad): claiming `%1 = add i32 %0, 1` "modifies %0, so a later use of %0
  sees the incremented value".
- **COUNTEREXAMPLE** (good): `%1 = add i32 %0, 1` defines a fresh `%1`; `%0` is
  unchanged everywhere; any later use of `%0` reads the original.
- **VERIFICATION**: `opt -S -passes=verify` — rejects malformed def-use/dominance.
- **SOURCE**: `llvm-langref` (Well-Formedness; Identifiers).

## 2. Typed values and constant syntax

- **RULE**: every value and constant carries a type: integers `iN` (`i1`, `i8`, `i32`,
  `i64`), pointer `ptr`, aggregates `{ i32, i32 }` (struct), `[N x T]` (array),
  `[N x i8]` (string buffer), vectors `<4 x i32>`. Constants are typed: `i32 5`.
  `void` is legal only for function results.
- **WHY AI GETS IT WRONG**: ignores the element type of an array constant or the
  result type of a load; mixes `[4 x i32]` (16 bytes) with `<4 x i32>` (also 16 bytes,
  but vector semantics and different operations).
- **CORRECT REASONING**: the type of a load result tells you the memory width; array
  indexing scales by the array element type. `i1` is a 1-bit integer, used for
  comparisons and branch conditions.
- **EXAMPLE** (bad): reading `%v = load i32, ptr %p` and then treating `%v` as an
  `i64` without a `sext`/`zext`.
- **COUNTEREXAMPLE** (good): noticing that `icmp slt i32 %i, %n` yields `i1`, and a
  `br i1 %cmp` consumes that `i1` directly; widening is explicit (`sext i32 %i to i64`).
- **VERIFICATION**: `opt -S -passes=verify`; `llvm-as file.ll` (parser rejects
  type mismatches).
- **SOURCE**: `llvm-langref` (Type System).

## 3. Global variables and linkage

- **RULE**: `@g = global i32 42` is mutable module memory; `@c = constant i32 42` is
  read-only and the optimizer may fold/merge it; `internal`/`private` are local
  linkage (like `static`); `declare i32 @f(i32)` is an external function. String
  literals appear as `@.str = private unnamed_addr constant [N x i8] c"..."`.
- **WHY AI GETS IT WRONG**: assumes a `constant` global is writable ("it is just a
  global"); reads `@.str` as a C `char*` instead of an array whose address is taken
  by GEP.
- **CORRECT REASONING**: `constant` = immutable (write to it is UB); `global` = may be
  written; a string constant is an array of `i8`; its address is obtained via
  `getelementptr [N x i8], ptr @.str, i64 0, i64 0`.
- **EXAMPLE** (bad): claiming `store i8 0, ptr @.str` is "just writing into a global".
- **COUNTEREXAMPLE** (good): recognizing `@.str` is `constant`, so a store is UB and
  the optimizer may assume it never happens.
- **VERIFICATION**: `opt -S -passes=verify`; inspect with `llvm-nm`/`llvm-dis`.
- **SOURCE**: `llvm-langref` (Global Variables; Linkage Types).

## 4. Basic blocks and terminators

- **RULE**: a basic block is a maximal straight-line run of instructions terminated by
  exactly one terminator: `ret`, `br` (conditional `br i1 %c, label %a, label %b` or
  unconditional `br label %l`), `switch`, `invoke`, `indirectbr`, `unreachable`. Only
  the entry block's label is required.
- **WHY AI GETS IT WRONG**: reads `unreachable` as "will crash"; misses that the
  optimizer treats it as "this path cannot happen" and assumes the surrounding branch
  condition away.
- **CORRECT REASONING**: `unreachable` is a promise to the optimizer. Code after it is
  dead; conditions guarding it can be constant-folded, deleting real checks.
- **EXAMPLE** (bad): claiming `br i1 %c, label %l1, label %l2` with `%l2` ending in
  `unreachable` "must branch to l2 sometimes, so the check stays".
- **COUNTEREXAMPLE** (good): noting that `%l2: unreachable` lets the optimizer assume
  `%c` is always true and fold the branch away.
- **VERIFICATION**: `opt -S -passes=simplifycfg file.ll` shows the branch fold.
- **SOURCE**: `llvm-langref` (Basic Blocks; Terminators; `unreachable`).

## 5. Integer arithmetic: `add`/`sub`/`mul` and `nuw`/`nsw`

- **RULE**: plain `add i32 %a, %b` wraps modulo 2^32 (two's complement). With flags,
  `add nuw` (no unsigned wrap) or `add nsw` (no signed wrap) — if the flagged wrap
  occurs, the result is **poison**. `udiv`/`sdiv`/`urem`/`srem` by zero is UB, and
  `sdiv` overflow (`INT_MIN / -1`) is UB.
- **WHY AI GETS IT WRONG**: treats `nsw` violation as "just wraps like unsigned" or as
  "immediately UB"; treats flags as documentation rather than poison contracts.
- **CORRECT REASONING**: `nsw`/`nuw` are proofs the frontend emits; the optimizer may
  fold/rewrite assuming no wrap. A violated flag yields poison, which becomes UB only
  when consumed by a triggering operation (branch, store, load address, divisor,
  callee, `noundef` positions).
- **EXAMPLE** (bad): on `%a = add nsw i32 %x, 1; %c = icmp sgt i32 %a, 0; br i1 %c`,
  claiming "when %x == INT_MAX the value wraps to INT_MIN so the branch is false".
- **COUNTEREXAMPLE** (good): recognizing the `nsw` overflow gives poison, the `icmp`
  is poison, and `br i1 poison` is undefined behavior — no reliable outcome.
- **VERIFICATION**: `opt -S -passes=instcombine` shows folding that assumes no wrap.
- **SOURCE**: `llvm-langref` ('`add`'/'`mul`'/'`sdiv`' Instructions; Poison Values).

## 6. `load` and `store`

- **RULE**: `%v = load i32, ptr %p, align 4` reads an `i32` from the address in `%p`;
  `store i32 %v, ptr %p, align 4` writes it. `volatile` prevents removal/reordering;
  `atomic` variants add an ordering. Overestimating `align` is UB.
- **WHY AI GETS IT WRONG**: ignores `align`; assumes a load of a valid address is
  always well-defined; forgets `ptr` carries no pointee type so the loaded type comes
  from the result.
- **CORRECT REASONING**: the load result type selects the access width; `align 4`
  promises 4-byte alignment (violating it is UB); a load from uninitialized `alloca`
  memory yields `undef`, not a stable "garbage" value.
- **EXAMPLE** (bad): claiming `load i32, ptr %p, align 4` reads 4 bytes regardless of
  the loaded type — `load i64, ptr %p, align 8` reads 8.
- **COUNTEREXAMPLE** (good): reading each load by its result type and required
  alignment, and checking the alignment promise is actually satisfied by the alloca/
  GEP/attribute chain.
- **VERIFICATION**: `opt -S -passes=verify`; compare `align` against the allocation.
- **SOURCE**: `llvm-langref` ('`load`'/'`store`' Instructions).

## 7. `getelementptr` (GEP) — address calculation, not dereference

- **RULE**: GEP computes an address and never touches memory. The first index offsets
  the pointer in units of the source element type; subsequent indices index into
  aggregates: array/pointer indices are multiplied by the element size, struct indices
  are **i32 field numbers** (offset taken from layout, no scaling). Struct indices must
  be `i32` constants. `inbounds` means the result stays in (or one-past) the allocated
  object — violation is **poison**; `inbounds` implies no-wrap (`nusw`).
- **WHY AI GETS IT WRONG**: "GEP dereferences / loads"; "the index is a byte offset";
  "struct index 1 adds 1 byte"; "array index 2 adds 2 bytes"; forgetting struct indices
  must be `i32` constants.
- **CORRECT REASONING**: `getelementptr i32, ptr %p, i64 2` is `p + 2*4` bytes;
  `getelementptr %struct.S, ptr %p, i64 0, i32 1` is `p + offsetof(S, field1)`. For raw
  byte offsets use element type `i8`. The result may legally point outside the object
  (unless `inbounds`) — only a later access is constrained.
- **EXAMPLE** (bad): reading `getelementptr i32, ptr %a, i64 2` as "+2 bytes" or
  `getelementptr %struct.Rect, ptr %r, i64 0, i32 1` as "+1 byte".
- **COUNTEREXAMPLE** (good): `%struct.Rect = type { %struct.Point, i32, i32 }` with
  `%struct.Point = type { i32, i32 }` — `i32 1` selects field `w` at byte offset 8
  (two `i32` fields before it); an agent who computes 8 instead of 1 is right.
- **VERIFICATION**: write the equivalent `getelementptr i8, ptr %p, i64 <bytes>` and
  confirm it matches; `opt -S -passes=instcombine` often folds GEPs to constant offsets.
- **SOURCE**: `llvm-langref` ('`getelementptr`' Instruction).

## 8. `alloca` — stack memory, not SSA

- **RULE**: `%p = alloca i32, align 4` allocates stack memory and returns a pointer;
  optionally `alloca i32, i32 %n` for variable counts. Memory is uninitialized (loading
  yields `undef`), auto-released on return. If the address escapes the function, the
  optimizer cannot promote it to SSA registers.
- **WHY AI GETS IT WRONG**: expects alloca'd values to behave like SSA registers;
  expects a load of fresh alloca memory to be 0; wonders "why all these loads/stores
  when the C code was simple".
- **CORRECT REASONING**: alloca + load/store is how mutable variables are represented
  at `-O0`. `mem2reg` promotes allocas whose addresses do not escape into phi + SSA
  values — this is the single biggest structural change between `-O0` and `-O1` IR.
- **EXAMPLE** (bad): claiming `%a = alloca i32; %v = load i32, ptr %a` is "always 0"
  because "fresh stack is zeroed".
- **COUNTEREXAMPLE** (good): recognizing the load yields `undef` (uninitialized), so
  any use is unreliable unless written first.
- **VERIFICATION**: `opt -S -passes=mem2reg` on `-O0` IR and diff the before/after.
- **SOURCE**: `llvm-langref` ('`alloca`' Instruction).

## 9. `call`

- **RULE**: `%r = call i32 @f(i32 %a)` invokes a function; `declare i32 @f(i32)` is an
  external declaration. Attributes may appear on the call and on arguments
  (`tail`, `nounwind`, `readonly`, `noundef`, `noalias`, ...). `invoke`/`resume` handle
  exceptions; `tail call`/`musttail` describe tail positions.
- **WHY AI GETS IT WRONG**: ignores call-site/argument attributes, e.g. reads
  `readnone` as "still may write through pointers", or assumes `nocapture` is visible
  in the callee body.
- **CORRECT REASONING**: function/argument attributes are contracts. `readnone` = no
  memory accessed at all; `readonly` = no writes; `nocapture` = the callee cannot make
  the pointer escape; `noundef` = no undef/poison bits allowed, else UB. The optimizer
  relies on them for CSE, load hoisting, and reordering.
- **EXAMPLE** (bad): claiming a `call i32 @f(ptr %p)` "may store through %p" when the
  call is marked `readnone` — contradiction.
- **COUNTEREXAMPLE** (good): noticing `call i32 @f(ptr noundef align 4 %p)` and reading
  both the contract (not null, 4-aligned) and its optimizer consequences.
- **VERIFICATION**: `opt -S -passes=instcombine,early-cse` reveals reordering enabled
  by attributes.
- **SOURCE**: `llvm-langref` ('`call`' Instruction; Function Attributes); `clang-docs`.

## 10. `phi` — edge-based merge, not assignment

- **RULE**: `%x = phi i32 [ %v0, %l0 ], [ %v1, %l1 ]` takes the value paired with the
  predecessor block that actually transferred control. Phis must be the first
  instructions in a block, one entry per predecessor, and each incoming value must be
  defined along the corresponding edge. In loops, phis encode induction variables.
- **WHY AI GETS IT WRONG**: reads phi as a mutable variable or "the latest assignment";
  writes a phi missing a predecessor; expects a phi's value to "change while the block
  runs".
- **CORRECT REASONING**: a phi is a dataflow merge — the value is selected when an edge
  is taken, not when the phi "executes". In a loop header, `%i = phi i32 [0, %entry],
  [%inc, %back]` is the loop counter: 0 on first entry, `%inc` on every back edge.
- **EXAMPLE** (bad): for `%i = phi i32 [ 0, %entry ], [ %inc, %for.inc ]`, claiming
  "the phi reassigns %i each iteration" or writing a phi for a block with two
  predecessors that lists only one.
- **COUNTEREXAMPLE** (good): tracing the two edges into the header and confirming each
  incoming value is defined in its predecessor; using that to identify the loop's
  induction variable and accumulator.
- **VERIFICATION**: `opt -S -passes=verify` rejects phis missing predecessors or with
  malformed incoming values; `opt -S -passes=loop-simplify` normalizes loop phis.
- **SOURCE**: `llvm-langref` ('`phi`' Instruction).

## 11. Pointer/function attributes are contracts

- **RULE**: `noalias` — during the call, memory accessed via pointers based on the
  argument/return is not accessed via non-based pointers (like C99 `restrict`;
  return-value `noalias` = allocation function). `align <n>` — the pointer is
  `n`-aligned, else poison. `dereferenceable(<n>)` — `n` bytes may be speculatively
  loaded without trapping; implies `nonnull` in address space 0 and implies `noundef`.
  `nonnull` — else poison. `noundef` — no undef/poison bits, else UB.
- **WHY AI GETS IT WRONG**: treats attributes as "just hints" or "compiler cosmetics";
  misses that violating them is UB/poison that the optimizer actively exploits (load
  hoisting, CSE, elimination of null checks).
- **CORRECT REASONING**: read attributes as preconditions with consequences:
  `dereferenceable(8)` lets a load be hoisted above a null test; `noalias` allows
  reordering stores; `align 4` allows narrower/aligned access patterns.
- **EXAMPLE** (bad): claiming a function with `dereferenceable(8)` parameter "might be
  called with a null pointer, so the null check is needed" — the check can be deleted.
- **COUNTEREXAMPLE** (good): noting that because the param is `dereferenceable(8)
  noundef`, the optimizer may eliminate the null test entirely and that a null call is
  UB in the caller.
- **VERIFICATION**: `opt -S -passes=instcombine,gvn` demonstrates checks folded away.
- **SOURCE**: `llvm-langref` (Parameter Attributes); `clang-docs`.

## 12. `undef` — per-use arbitrary value

- **RULE**: `undef` represents an arbitrary value of its type; **each use may
  independently pick any value**. `xor i32 %u, %u` is still undef (the two uses can
  differ), `add i32 undef, 1` may fold to undef. Branching on undef is UB. A store of
  undef can be deleted; a store to an undef address is UB.
- **WHY AI GETS IT WRONG**: "undef is a garbage number that stays the same"; "v*v with
  undef v is non-negative"; "undef is just a small constant".
- **CORRECT REASONING**: undef has no live range — each use observes an independently
  chosen value, so almost nothing can be proven about it. Results that must be
  consistent across uses need `freeze`.
- **EXAMPLE** (bad): `%v = load i32, ptr %p` (uninitialized); `%sq = mul i32 %v, %v`;
  claiming "%sq is always >= 0" (it is undef).
- **COUNTEREXAMPLE** (good): `%v.fr = freeze i32 %v; %sq = mul i32 %v.fr, %v.fr` —
  after freeze all uses agree, so `%sq` is some fixed value (though not a known one).
- **VERIFICATION**: `opt -S -passes=instcombine` shows undef folds; see LangRef examples.
- **SOURCE**: `llvm-langref` (Undef Values).

## 13. `poison` — propagating erroneous value, lazy UB

- **RULE**: poison is the result of an erroneous operation (`add nsw` overflow, OOB
  `inbounds` GEP, misaligned access, and others). Most instructions propagate poison
  (exception: `select`). Poison does NOT immediately cause UB; UB occurs only when a
  poison value reaches a triggering position: pointer operand of load/store, divisor
  of a division, condition of `br`/`switch`, callee of a call, or `noundef`
  parameter/return.
- **WHY AI GETS IT WRONG**: "poison = immediate UB"; "poison and undef are the same";
  "storing poison to memory is harmless"; "I can compare poison against 0 safely".
- **CORRECT REASONING**: poison is more constrained than undef (a poison may be
  replaced by any value, but it cannot be silently "refined" into something that makes
  a triggering use well-defined). A branch on poison is UB; a compare of poison is
  poison; a store of poison is UB.
- **EXAMPLE** (bad): `%p2 = getelementptr inbounds ... ; store i32 %bad, ptr %p2` where
  `%bad` is poison from an `add nsw` — claiming "the store just writes garbage".
- **COUNTEREXAMPLE** (good): recognizing that storing/branching/returning poison is UB,
  and that the only sound way to "tame" it is `freeze` before the triggering use.
- **VERIFICATION**: `opt -S -passes=instcombine` and LangRef examples; `llvm-ub`-style
  execution is not available on this host (documented as target).
- **SOURCE**: `llvm-langref` (Poison Values).

## 14. `freeze` — stop poison/undef propagation

- **RULE**: `%x = freeze i32 %v` — if `%v` is undef/poison, returns an arbitrary but
  **fixed** value; all uses of the frozen value observe the same value, and the result
  is well-defined. Different freezes may yield different values. Freeze applies
  element-wise to aggregates/vectors.
- **WHY AI GETS IT WRONG**: "freeze makes the value random" (it fixes it once);
  "freezing after a branch fixes the branch" (the branch already saw the poison);
  "freeze is a no-op".
- **CORRECT REASONING**: freeze converts a per-use-arbitrary or propagating value into
  one fixed value, enabling safe comparisons/branches:
  `%f = freeze i1 %maybe_poison; br i1 %f, ...` is well-defined (non-deterministic
  but not UB).
- **EXAMPLE** (bad): `br i1 %c, label %a, label %b` then `%c2 = freeze i1 %c` — the
  branch still has UB; freeze must precede the triggering use.
- **COUNTEREXAMPLE** (good): `%c = freeze i1 %poison; br i1 %c, ...` — well-defined
  non-deterministic jump (see LangRef).
- **VERIFICATION**: `opt -S -passes=instcombine` propagates freeze; self-review vs
  LangRef freeze semantics.
- **SOURCE**: `llvm-langref` ('`freeze`' Instruction).

## 15. Opaque pointers: what `ptr` means

- **RULE**: since LLVM 17, typed pointers are removed — every pointer is `ptr`
  (optionally `ptr addrspace(N)`). There is no pointee type. The loaded type comes
  from the `load` result; GEP scaling comes from its explicit source element type;
  pointer `bitcast`s are gone/no-ops.
- **WHY AI GETS IT WRONG**: reads old `i32*`/`i8*`/`%struct.Foo*` snippets as modern IR
  and expects pointee info on the pointer itself; writes `bitcast i32* %p to i8*`-style
  transformations.
- **CORRECT REASONING**: `load i32, ptr %p` means "read an i32 at the address %p" — the
  `i32` is the load's result type, not a property of `%p`. `getelementptr i32, ptr %p,
  i64 2` names `i32` as the source element type for scaling. Frontend type info now
  lives in the instructions, not the pointer type.
- **EXAMPLE** (bad): claiming "%p is an i32* so `store i64 %v, ptr %p` stores 4 bytes".
- **COUNTEREXAMPLE** (good): reading `store i64 %v, ptr %p, align 8` as an 8-byte store
  regardless of how `%p` was produced, because `ptr` is untyped.
- **VERIFICATION**: `clang -S -emit-llvm` output on any modern clang shows bare `ptr`;
  `opt -S -passes=verify`.
- **SOURCE**: `llvm-langref` (Pointer Type); `clang-docs`.

## 16. Reading optimized IR: what passes did

- **RULE**: `-O0` IR is source-shaped: allocas + load/store, no folding. `-O1` runs
  mem2reg (alloca → SSA + phi), instcombine (folding, flags like `nsw`/`nuw`), DCE —
  allocas vanish and phis appear at joins. `-O2`/`-O3` add inlining (callers grow),
  LICM/GVN (hoisted loads, CSE), loop unrolling, vectorization (`<4 x i32>`,
  `llvm.memcpy`/`llvm.memset` intrinsics), attribute inference (`noalias`, `readonly`,
  `noundef`), and `select` chains replacing small branches.
- **WHY AI GETS IT WRONG**: expects optimized IR to mirror the C source; flags
  optimization artifacts as bugs; conflates GCC's pipeline with LLVM's — GCC emits
  GIMPLE and RTL, never LLVM IR.
- **CORRECT REASONING**: read optimized IR structurally: (1) find loops via header
  phis + back edges; (2) trace pointer flow through GEP to loads/stores; (3) interpret
  flags/attributes as contracts; (4) recognize intrinsics (`llvm.memcpy.p0.p0.i64`,
  `llvm.assume`) as lowered library calls; (5) when surprised, diff pass-by-pass with
  `opt -S -passes=<pass>`.
- **EXAMPLE** (bad): seeing `call void @llvm.memcpy.p0.p0.i64(ptr %d, ptr %s, i64 16, i1
  false)` and claiming it is "a custom copy function with an unknown side effect".
- **COUNTEREXAMPLE** (good): recognizing the memcpy intrinsic (16-byte copy, no
  trapping, noalias semantics via its attributes) as a whole-struct assignment lowered
  by the frontend.
- **VERIFICATION**: `opt -S -passes=mem2reg file.ll -o -`, then
  `opt -S -passes='default<O2>' file.ll -o -`, and diff outputs.
- **SOURCE**: `llvm-langref` (Intrinsics; `llvm.memcpy`); `clang-docs` (optimization
  levels); `gcc-manual` (GCC's own GIMPLE/RTL pipeline — not LLVM IR).
