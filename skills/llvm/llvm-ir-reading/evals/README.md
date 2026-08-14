# Evaluation — llvm-ir-reading

Skill: `skills/llvm/llvm-ir-reading`. Stability target: `evaluated`.
clang/opt/llvm-as are NOT installed on this host; all executable commands are
documented as target verification for a machine with an LLVM toolchain. Semantic
facts in the reference were self-reviewed against the LLVM Language Reference
Manual (`llvm-langref`); see the verified-facts table below.

## Synthetic evals

Each case: READ the IR -> EXPLAIN (what each instruction means, byte offsets,
poison/undef status) -> VERIFY (claim must be derivable from IR text + LangRef).

- **easy/positive**: read `examples/good/array_index.ll` — identify the loop phis,
  the induction value, the GEP byte offsets, and the `nsw` flags. Must explain
  `sext i32 %i to i64` without calling it a bug.
- **easy/negative**: read `examples/bad/gep_misread.ll` — must compute byte offsets
  (struct field 1 = offset 8; `i32` index 2 = +8 bytes), NOT "+1 byte"/"+2 bytes".
- **medium/negative**: read `examples/bad/poison_vs_undef.ll` — must state that
  `add nsw` overflow gives poison, the `br i1` on the poison compare is UB, and that
  `mul i32 %v, %v` with undef `%v` is undef (not provably `v*v`, not provably >= 0).
- **hard/negative**: read `examples/bad/phi_misuse.ll` — must reconstruct the loop
  state (two phis), list both predecessor edges per phi, and explain that a phi
  missing a predecessor would be rejected by the verifier.
- **adversarial**: read `examples/good/optimized.ll` — must interpret `noalias`,
  `align 4`, `dereferenceable(8)`, `noundef` as contracts, recognize the aggregate
  load/store and the `llvm.memcpy` intrinsic, and state what the optimizer may now
  assume (e.g., speculatable load, reordering across the two pointer accesses).

## False-positive evals (correct IR must NOT be flagged)

- Plain `add i32 %a, %b` with no flags used in a wrapped-check pattern — no poison,
  no UB; do NOT flag.
- `getelementptr inbounds i32, ptr %p, i64 0` (zero offset) on a valid pointer —
  always inbounds; do NOT flag.
- A phi that lists exactly the block's predecessors with correctly-defined incoming
  values — well-formed; do NOT flag.
- `%c = freeze i1 %maybe_poison; br i1 %c, ...` — freeze before the branch makes it
  well-defined (non-deterministic but not UB); do NOT flag.
- `load i32, ptr %p, align 1` — alignment 1 is always safe; do NOT flag.

## Verification commands (target; run on a machine with an LLVM toolchain)

```
clang -O1 -S -emit-llvm examples/bad/gep_misread.c -o /tmp/gep_misread.ll
clang -O1 -S -emit-llvm examples/bad/poison_vs_undef.c -o /tmp/poison_vs_undef.ll
clang -O1 -S -emit-llvm examples/bad/phi_misuse.c -o /tmp/phi_misuse.ll
clang -O1 -S -emit-llvm examples/good/array_index.c -o /tmp/array_index.ll
clang -O1 -S -emit-llvm examples/good/struct_offset.c -o /tmp/struct_offset.ll
clang -O2 -S -emit-llvm examples/good/optimized.c -o /tmp/optimized.ll
for f in /tmp/*.ll; do opt -S -passes=verify "$f" -o /dev/null; done
opt -S -passes=mem2reg /tmp/gep_misread.ll -o -          # O0->O1 style transformation
opt -S -passes='default<O2>' /tmp/array_index.ll -o -     # full pipeline effect
```

Expected results when run:
- `opt -passes=verify` exits 0 for all example `.ll` files (well-formed: phis cover
  all predecessors, definitions dominate uses).
- `clang -O1` regenerated IR matches the example structure (modulo value names).
- `examples/good/optimized.ll` attribute set may differ slightly by clang version;
  re-read attributes from the actual output before relying on them.

## Verified facts

Status legend: VERIFIED = self-review against `llvm-langref` text on this host;
TARGET = requires an LLVM toolchain, documented as target verification.

| Fact | Status | Source |
|---|---|---|
| Every instruction defines one new immutable SSA value; uses dominated by defs | VERIFIED | llvm-langref (Well-Formedness) |
| `add`/`mul` with `nsw`/`nuw`: violation yields poison | VERIFIED | llvm-langref ('add'/'mul') |
| `sdiv` by zero and `sdiv` overflow are UB (not poison) | VERIFIED | llvm-langref ('sdiv') |
| GEP computes addresses, never dereferences; struct indices are i32 field numbers; other indices scale by allocation size (rounded to ABI alignment) | VERIFIED | llvm-langref ('getelementptr') |
| `inbounds` violation yields poison; all-zero-index GEP always inbounds | VERIFIED | llvm-langref ('getelementptr') |
| phi must be first in block, one entry per predecessor, value defined on that edge | VERIFIED | llvm-langref ('phi') |
| `undef`: each use may independently pick any value | VERIFIED | llvm-langref (Undef Values) |
| poison propagates through most instructions; UB only at triggering uses (load/store pointer, divisor, br condition, callee, noundef positions) | VERIFIED | llvm-langref (Poison Values) |
| `freeze` returns an arbitrary but fixed value; all uses observe the same value | VERIFIED | llvm-langref ('freeze') |
| `alloca` memory is uninitialized; load yields undef; auto-released on return | VERIFIED | llvm-langref ('alloca') |
| `noalias` ~ C99 restrict (modified locations); return-value noalias = allocation function | VERIFIED | llvm-langref (Parameter Attributes) |
| `align <n>` violated -> poison; `dereferenceable(<n>)` implies nonnull + noundef; `noundef` violated -> UB | VERIFIED | llvm-langref (Parameter Attributes) |
| Opaque pointers: all pointers are `ptr`; load type from result; GEP source element type drives scaling | VERIFIED | llvm-langref (Pointer Type) |
| `clang -O1` runs mem2reg (alloca->SSA+phi); -O2 adds inlining, LICM, vectorization, attributes | TARGET | clang-docs; llvm-langref |
| GCC emits GIMPLE/RTL, not LLVM IR | TARGET | gcc-manual |
| Example `.ll` files parse and verify with `opt -passes=verify` | TARGET | llvm-langref |

## Scoring (for routing eval)

- precision: every claimed byte offset, poison/undef classification, and phi edge
  must match LangRef semantics.
- recall: each bad example's trap (GEP offset, poison-branch, phi-as-variable) must
  be caught.
- FP-rate: the good examples and the false-positive list must yield zero flags.
