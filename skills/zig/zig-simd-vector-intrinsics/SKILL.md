---
name: zig-simd-vector-intrinsics
description: Use when writing or reviewing Zig SIMD: @Vector, element-wise ops, @splat/@shuffle/@select/@reduce, std.simd, and LLVM-vector lowering. Prevents runtime vector indexing, vector-array coercion mistakes, missing overflow guards, and scalar-vector mixing. Version-pinned to Zig 0.15-0.17.
---

# Zig SIMD / Vector Intrinsics

## When to use

- Writing data-parallel code with `@Vector(N, T)` (element-wise arithmetic, comparisons,
  reductions, shuffles).
- Porting a NEON/SSE kernel to portable Zig vectors, or reviewing generated SIMD code.
- Reasoning about when vectors lower to single SIMD instructions vs element-wise fallback.
- Debugging vector overflow, indexing, and coercion errors.

## When not to use

- Autovectorization reasoning on other compilers — see `vectorization-reasoning` and
  `simd-vectorization-cross-layer`.
- Hand-written intrinsics for a specific CPU — prefer portable `@Vector`; if you need the
  instruction, use `zig-inline-asm-and-abi`.
- Scalar hot-loop tuning without measuring — see `performance-measurement-discipline`.

## What the agent often gets wrong

- Indexing a vector with a runtime value — 0.16 forbids runtime vector indexes; coerce to
  an array first (`const arr: [N]T = vec;`), then index.
- Using `@ptrCast` between vector and array memory — vectors have no defined byte layout;
  `@bitCast`/coercion is the supported route; `@ptrCast` is Illegal Behavior.
- Mixing scalars with vectors without `@splat` — "prohibited" per the langref.
- Forgetting integer overflow is Illegal Behavior in Debug/ReleaseSafe even for vectors:
  a per-lane `+` panics on overflow (u8 counters every 255 iterations — the NEON-style
  overflow guard), and `@reduce(.Add)` is *wrapping* — it silently wraps instead of
  trapping.
- Claiming `@reduce(.Add)` detects overflow, or "fixing" a Debug panic by switching the
  build to ReleaseFast, which hides the wrap.
- Assuming any vector length compiles to one instruction — lengths beyond the native SIMD
  size split into multiple instructions; unsupported ops degrade to element-wise loops.
- Assuming `std.simd` function names are stable — the namespace is version-fluctuating;
  pin the version and read std/simd.zig.

## How to reason correctly

1. Build vectors with `@Vector(N, T)` where N is comptime-known; `@splat(x)` broadcasts a
   scalar to a vector/array.
2. For element access, prefer indexing with comptime-known indexes (compile-time) or
   coerce to `[N]T`/`*[N]T` for runtime indexes (required on 0.16+).
3. Reduce with `@reduce(op, v)` — `.And`/`.Or`/`.Xor` for bools, `.Min`/`.Max`/`.Add`/
   `.Mul` for floats and ints; integer `.Add`/`.Mul` reductions are wrapping (langref,
   KNOWN). Reorder with `@shuffle`, choose with `@select`.
4. Decide overflow policy explicitly per lane: `+` traps (Debug/ReleaseSafe), `+%` wraps,
   `+|` saturates; `-`, `*` likewise. Add the guard where wrap is not acceptable
   (counter accumulation), mirroring the NEON overflow-guard requirement.
5. Convert between vectors and arrays with coercion (0.16 removed in-memory coercion via
   `@ptrCast`; use `@bitCast` or implicit coercion).
6. Treat `std.simd` helpers as version-sensitive (INFERRED surface; check std/simd.zig
   for the pinned version).

## What to verify

- Every vector index is comptime-known, or the vector was coerced to an array first.
- No `@ptrCast` between vectors and arrays; only `@bitCast`/coercion.
- Scalars enter vectors only via `@splat`.
- Integer accumulation lanes have an explicit overflow policy; `@reduce(.Add)`'s wrapping
  is not used as an overflow check.
- The build is tested in Debug (safety checks on), not only ReleaseFast.
- Code compiles and passes under the pinned Zig version.

## How to verify

```
zig test examples/good/vector_add.zig
zig test examples/good/reduce_shuffle.zig
zig test examples/good/vector_overflow_safe.zig
zig test examples/bad/vector_runtime_index.zig   # fails on 0.16+: runtime vector index
zig test examples/bad/vector_overflow.zig        # Debug: panic on overflow
zig build test                                    # project test step
```

Researched — zig not installed on this host; commands are the recorded verification plan.

## Where the knowledge comes from

- zig-langref §Vectors (element types, element-wise ops, `@splat`/`@reduce`/indexing,
  native-size lowering, relationship with arrays), §Builtin Functions (@Vector, @splat,
  @shuffle, @select, @reduce), §Illegal Behavior (Integer Overflow).
- zig-release-notes 0.16.0 (Forbid Runtime Vector Indexes; Vectors and Arrays No Longer
  Support In-Memory Coercion; Loop Vectorization Disabled regression).
- zig-std-source (std/simd.zig).
- simd-vectorization-cross-layer (existing skill — cross-layer SIMD reasoning).

## Related skills

- `simd-vectorization-cross-layer` — portable SIMD patterns across languages.
- `zig-comptime-metaprogramming` — comptime-known vector lengths and reification.
- `zig-version-migration` — runtime vector indexes and coercion changes in 0.16.
- `zig-inline-asm-and-abi` — when an exact SIMD instruction is required.
- `vectorization-reasoning` — autovectorizer behavior contrast.

## Evaluation

- Synthetic: runtime vector index, `@ptrCast` vector↔array, scalar/vector mixing, missing
  overflow guard, wrapping `@reduce(.Add)` used as a check — must be caught; good
  add/reduce/shuffle/overflow-safe examples must pass.
- False-positive: deliberate `+%` wrapping with a documented guard, `@bitCast` between
  vector and array, comptime-indexed `vec[i]` — must NOT be flagged.
- Historical: 0.16.0 "Forbid Runtime Vector Indexes" and "No In-Memory Coercion" are the
  regression targets.
- Adversarial: a u8 counter sum that passes ReleaseFast but wraps silently — the overflow
  guard must be added, not the build mode changed (mirror of the NEON counter-overflow
  case from asm-aarch64-neon-simd-safety).
- Commands and recorded results: `evals/README.md`.
