---
name: llvm-ir-reading
description: Use when reading, reviewing, or debugging LLVM IR (.ll files or opt output) — understanding SSA form, GEP offsets, phi nodes, poison/undef/freeze semantics, opaque pointers, function attributes, or explaining why an optimization pass changed the IR. Covers clang -S -emit-llvm and opt workflows.
---

# Reading LLVM IR

## When to use

- Reading a `.ll` file or `opt`/`llvm-dis` output to understand what a function does.
- Explaining why a pass transformed IR (mem2reg, instcombine, inlining, loop passes).
- Checking whether an IR snippet is well-formed and what a specific instruction means.
- Writing or reviewing a pass in `llvm-pass-writing`; the first step is reading IR correctly.
- Reasoning about poison/undef/freeze effects that the optimizer relies on.

## When not to use

- x86/AArch64 assembly — use `asm-x86-64-*` / `abi-layout-reasoning`.
- Writing LLVM passes — use `llvm-pass-writing` (requires this skill).
- GCC internals: GCC emits GIMPLE and RTL, NOT LLVM IR — do not apply these rules to GCC dumps.
- High-level C semantics without the IR in hand — use `c-undefined-behavior`.

## What the agent often gets wrong

- "getelementptr adds bytes": GEP indices are in elements/fields; `getelementptr i32, ptr %p, i64 2` adds 8 bytes, not 2.
- "Struct GEP index is a byte offset": a struct index is a field number; the byte offset comes from layout.
- "`add nsw` overflow wraps like hardware": overflow produces **poison**, not a wrapped value, and branching on poison is UB.
- "`undef` is a random but consistent value": each use may independently see a different value.
- "Poison immediately means UB": poison is lazy — UB happens only at a triggering use (branch, store, load pointer, divisor, callee, `noundef` positions).
- "phi is a mutable variable / assignment": phi selects one incoming value per predecessor edge and must be first in its block.
- Reading typed-pointer-style IR (`i32*`) into modern opaque-pointer IR: in current LLVM every pointer is `ptr`.
- Treating function attributes (`noalias`, `dereferenceable`, `align`) as hints instead of contracts the optimizer exploits.

## How to reason correctly

1. Trace SSA dataflow: every instruction defines one new immutable value; memory is only touched via `load`/`store`/`alloca`.
2. For every `getelementptr`: name the element type, then compute byte offset = struct layout offsets + index * element size. `inbounds` violation or `nuw`/`nsw` violation means poison.
3. Read loops as cycles: identify the `phi` nodes at the header, the induction value, and the back edge.
4. For every flag/attribute, ask: what is the contract, and what is the optimizer allowed to assume if I break it?
5. Distinguish `undef` (per-use arbitrary), `poison` (propagating erroneous value, UB only at triggering uses), and `freeze` (arbitrary but fixed).
6. When reading optimized IR, reconstruct what passes did instead of expecting source-shaped structure.
7. Mark uncertainty: if you cannot derive a fact from the IR text alone, label it INFERRED and verify with `opt`.

## What to verify

- Every phi in a block lists exactly the block's predecessors, each with a value defined on that edge.
- Every GEP byte offset you claim matches the type layout and the index semantics.
- Every `nsw`/`nuw`/`inbounds` claim is justified; overflow/out-of-bounds means poison, not wrapped math.
- Any claim "this branch is safe" holds even when the compared value can be poison or undef.
- Your reading of an `.ll` file matches what clang/opt actually emit for the given C source and pass list.

## How to verify

```
clang -O1 -S -emit-llvm file.c -o file.ll          # generate IR from C
opt -S -passes=verify file.ll -o /dev/null         # verifier: well-formedness, phi/dominance
opt -S -passes=mem2reg file.ll -o -                # observe mem2reg: allocas -> SSA + phi
opt -S -passes=instcombine,simplifycfg file.ll -o - # observe folding, unreachable deletion
opt -S -passes='default<O2>' file.ll -o -          # full -O2 pipeline on IR
llvm-as file.ll -o /dev/null                       # parse + verify assembly
```

clang/opt/llvm-as are not installed on this host; commands are documented as the target
verification for a machine with an LLVM toolchain.

## Where the knowledge comes from

- `llvm-langref` — LLVM Language Reference Manual (SSA, types, GEP, phi, poison/undef/freeze, attributes, load/store, alloca).
- `clang-docs` — Clang documentation (IR generation, attributes emitted by clang, opaque-pointer transition).
- `gcc-manual` — GCC manual (GCC's own optimization pipeline is GIMPLE/RTL, not LLVM IR; optimizer-assumption mindset).

## Related skills

- `llvm-pass-writing` — requires this skill.
- `compiler-ub-assumptions` — recommends this skill; poison/undef are the IR-side vocabulary of UB exploitation.
- `c-undefined-behavior` — C UB classes map to IR poison/nsw/unreachable patterns.
- `abi-layout-reasoning` — struct layout feeds GEP byte-offset reasoning.
- `asm-optimizer-artifacts` — what the optimizer finally emits as assembly, the far end of IR reading.

## Evaluation

Synthetic: GEP offset trap (i32 vs i8 element, struct field number vs byte), poison-branch UB,
undef-per-use, phi with missing predecessor, loop induction reading, optimized-IR attribute reading.
False-positive: correct in-bounds GEP, well-formed phis, non-nsw arithmetic, freeze before branch must
NOT be flagged. Adversarial: IR that is verifier-clean but semantically UB (nsw overflow stored or
branched); IR where the agent must distinguish poison from undef. Commands and status: see `evals/README.md`.
