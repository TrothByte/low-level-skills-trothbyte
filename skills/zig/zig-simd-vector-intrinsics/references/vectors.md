# Zig SIMD / Vector Intrinsics — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED.

## 1. @Vector and element-wise operations

- **RULE**: `@Vector(len, Elem)` creates a vector of bools, integers, floats, or
  pointers; len is comptime-known. Arithmetic, bitwise, comparison and boolean-not
  operators are element-wise. Scalars may not mix with vectors — use `@splat`.
- **WHY AI GETS IT WRONG**: writes `vec + 1` and expects broadcast; or uses `and`/`or`
  on bool vectors (prohibited because they affect control flow).
- **CORRECT REASONING**: `@splat` broadcasts a scalar into the vector's result type;
  indexing extracts a scalar; `@reduce` collapses. Shorter vectors than the native SIMD
  size typically compile to single instructions; longer ones split; unsupported ops
  degrade element-wise.
- **EXAMPLE** (bad):
  ```zig
  const v: @Vector(4, i32) = .{ 1, 2, 3, 4 };
  const r = v + 1; // error: operator + on scalar and vector
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const v: @Vector(4, i32) = .{ 1, 2, 3, 4 };
  const r = v + @as(@Vector(4, i32), @splat(1)); // {2,3,4,5}
  ```
- **VERIFICATION**: `zig test examples/good/vector_add.zig`.
- **SOURCE**: zig-langref §Vectors, §Builtin Functions (@Vector, @splat).

## 2. No runtime vector indexes (0.16+)

- **RULE**: 0.16.0 forbids indexing a vector with a runtime-known index. Coerce the vector
  to an array first: `const array: [N]T = vector;` then index the array.
- **WHY AI GETS IT WRONG**: writes `for (0..N) |i| { _ = vec[i]; }` from pre-0.16 memory —
  a compile error on 0.16+.
- **CORRECT REASONING**: vectors have no defined byte/ordering semantics for runtime
  element selection; the array coercion makes the access well-defined. Comptime-known
  indexes remain fine.
- **EXAMPLE** (bad, fails on 0.16+):
  ```zig
  fn sum(v: @Vector(4, i32)) i32 {
      var total: i32 = 0;
      for (0..4) |i| total += v[i]; // runtime index into a vector
      return total;
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn sum(v: @Vector(4, i32)) i32 {
      const arr: [4]i32 = v;
      var total: i32 = 0;
      for (arr) |e| total += e;
      return total;
  }
  ```
- **VERIFICATION**: `zig test examples/bad/vector_runtime_index.zig` fails on 0.16+;
  the good version passes.
- **SOURCE**: zig-release-notes 0.16.0 (Forbid Runtime Vector Indexes); zig-langref §Vectors.

## 3. Vectors vs arrays: @bitCast and coercion, not @ptrCast

- **RULE**: vectors and arrays have well-defined bit layouts, so `@bitCast` between them
  (and implicit coercion) is legal. Vectors do NOT have a defined byte layout, so
  `@ptrCast` between vector memory and array memory is Illegal Behavior. 0.16 removed
  in-memory coercion (`@ptrCast`) between vectors and arrays — use coercion/`@bitCast`.
- **WHY AI GETS IT WRONG**: writes `const bytes: []const u8 = @ptrCast(&vec);` to inspect
  a vector's bytes.
- **CORRECT REASONING**: convert explicitly: coerce to an array, or `@bitCast` to the
  array type, then view bytes.
- **EXAMPLE** (bad):
  ```zig
  const v: @Vector(4, f32) = .{ 1, 2, 3, 4 };
  const bytes = @as([*]const u8, @ptrCast(&v)); // Illegal Behavior: vector has no byte layout
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const v: @Vector(4, f32) = .{ 1, 2, 3, 4 };
  const arr: [4]f32 = v;                     // coercion
  const bytes: [16]u8 = @bitCast(arr);       // defined bit layout
  ```
- **VERIFICATION**: `zig test` — the bad pattern is rejected (0.16+); the good one passes.
- **SOURCE**: zig-langref §Vectors (Relationship with Arrays); zig-release-notes 0.16.0
  (Vectors and Arrays No Longer Support In-Memory Coercion).

## 4. Overflow in vectors is Illegal Behavior; reductions are wrapping

- **RULE**: integer overflow on vector `+`/`-`/`*` is Illegal Behavior — safety-checked in
  Debug and ReleaseSafe. `+%`/`-%`/`*%` wrap, `+|`/`-|`/`*|` saturate. `@reduce(.Add)` and
  `@reduce(.Mul)` on integral types are WRAPPING (langref, KNOWN).
- **WHY AI GETS IT WRONG**: sums a `u8` vector counter without a guard and "fixes" the
  Debug panic by compiling in ReleaseFast — silently wrapping every 256 iterations
  (the NEON counter-overflow failure class); or treats `@reduce(.Add)` as an overflow
  detector.
- **CORRECT REASONING**: choose per-lane semantics explicitly. For a saturating byte
  accumulator use `+|`; for wrapping (documented) use `+%`; for trapping semantics keep
  `+` and stay within range. Never rely on a wrapping reduction to catch overflow.
- **EXAMPLE** (bad):
  ```zig
  fn dot4(a: @Vector(4, u8), b: @Vector(4, u8)) u8 {
      return @reduce(.Add, a * b); // wraps silently; overflow hidden
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn dot4_safe(a: @Vector(4, u8), b: @Vector(4, u8)) u16 {
      // widen before accumulating: no per-lane overflow
      const aa: @Vector(4, u16) = a;
      const bb: @Vector(4, u16) = b;
      return @reduce(.Add, aa * bb);
  }
  ```
- **VERIFICATION**: `zig test examples/bad/vector_overflow.zig` panics in Debug;
  `zig test examples/good/vector_overflow_safe.zig` passes and returns exact values.
- **SOURCE**: zig-langref §Vectors, §Illegal Behavior (Integer Overflow), §Builtin
  Functions (@reduce — "Add and Mul reductions on integral types are wrapping").

## 5. @shuffle and @select

- **RULE**: `@shuffle(E, a, b, mask)` builds a new vector by selecting from `a` (mask >=
  0) or `b` (mask < 0, written with `~`); out-of-bounds masks are compile errors.
  `@select(T, pred, a, b)` picks per element.
- **WHY AI GETS IT WRONG**: uses positive masks for the second vector; forgets mask
  indexes for `b` start at -1 (`~0`).
- **CORRECT REASONING**: mask values `>= 0` index `a`; `-1/-2/...` index `b`. Use
  `~@as(i32, 0)` for "first element of b". Mask length = result length.
- **EXAMPLE** (bad):
  ```zig
  const res: @Vector(4, u8) = @shuffle(u8, a, b, @Vector(4, i32){ 0, 1, 2, 3 });
  // selects only from a — likely not the intent when b was supplied
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const res: @Vector(6, u8) = @shuffle(u8, a, b, @Vector(6, i32){
      -1, 0, 4, 1, -2, -3,
  }); // world! — mixes b (negatives) and a
  ```
- **VERIFICATION**: `zig test examples/good/reduce_shuffle.zig` (langref test_shuffle
  is the reference).
- **SOURCE**: zig-langref §Builtin Functions (@shuffle, @select).

## 6. std.simd is version-sensitive

- **RULE**: `std.simd` (zig-std-source std/simd.zig) provides helpers (e.g. interlaced,
  deinterlace, extract, iota, count, suggestVectorLength — INFERRED set); the exact
  surface drifts between versions.
- **WHY AI GETS IT WRONG**: calls a `std.simd.*` function from memory that was renamed or
  removed in the pinned version, then "fixes" it by inventing parameters.
- **CORRECT REASONING**: check std/simd.zig in the pinned std before using; prefer
  language builtins (`@splat`/`@shuffle`/`@reduce`/`@select`) which are stable, and use
  `std.simd` only where a language builtin does not cover the need.
- **EXAMPLE** (bad): calling `std.simd.interlaced` with a signature guessed from a blog
  post.
- **COUNTEREXAMPLE** (good): `std.simd.iota(@Vector(8, u32))`-style usage read directly
  from std/simd.zig for the pinned version (exact signature INFERRED — verify per pin).
- **VERIFICATION**: `zig test` against the pinned version; the langref builtins are the
  stable core.
- **SOURCE**: zig-std-source (std/simd.zig); zig-langref §Vectors.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Creation | `@Vector(len, Elem)`, len comptime; ops element-wise |
| Scalar mixing | prohibited — `@splat` broadcasts |
| Indexing | comptime indexes OK; runtime indexes require array coercion (0.16+) |
| Vector↔array | coercion / `@bitCast`; `@ptrCast` is Illegal Behavior |
| Overflow | `+` traps (Debug/ReleaseSafe); `+%` wraps; `+|` saturates |
| @reduce(.Add) | wrapping for integers — never an overflow check |
| Reorder | `@shuffle` (b indexes negative, `~0` = first of b); `@select` per element |
| Length lowering | ≤ native SIMD → 1 instr; longer → multiple; unsupported → element-wise |
| std.simd | helpers exist but surface drifts — read std/simd.zig per pin |
