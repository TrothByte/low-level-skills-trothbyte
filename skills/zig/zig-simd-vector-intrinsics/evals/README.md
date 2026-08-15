# Evaluation — zig-simd-vector-intrinsics

Skill: `skills/zig/zig-simd-vector-intrinsics`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host; the code
targets the 0.15–0.17 API surface (verified against the langref Vectors section and the
0.16.0 release notes). Verification commands below are the recorded plan, not run results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/vector_runtime_index.zig` | fails on 0.16+: runtime vector index | `zig test` |
| easy/negative | `bad/scalar_mix.zig` | fails: scalar/vector operator mix | `zig test` |
| medium/negative | `bad/vector_overflow.zig` | Debug: panic (overflow); ReleaseFast: silent wrap | `zig test` / `-Doptimize=ReleaseFast` |
| medium/negative | review | `@ptrCast` between vector and array memory | review |
| positive | `good/vector_add.zig` | passes | `zig test` |
| positive | `good/reduce_shuffle.zig` | passes; @reduce/@shuffle/@select | `zig test` |
| positive | `good/vector_overflow_safe.zig` | passes; widening + saturating | `zig test` |

## False-positive evals (correct code must not be flagged)

- `good/vector_overflow_safe.zig` — widening before accumulation and explicit `+|`
  saturating add are the correct overflow policies.
- `good/reduce_shuffle.zig` — `@shuffle` with negative masks for the second vector and
  `@select` are correct.
- Deliberate `+%` wrapping with a documented policy is correct; comptime-indexed `v[0]`
  is correct.
- Coercion `const arr: [N]T = vec;` for runtime iteration — correct, not a "copy".

## Historical evals

- 0.16.0 "Forbid Runtime Vector Indexes" — `bad/vector_runtime_index.zig` reproduces the
  pre-0.16 pattern.
- 0.16.0 "Vectors and Arrays No Longer Support In-Memory Coercion" — `@ptrCast`-based
  byte views must become coercion/`@bitCast`.
- 0.16.0 LLVM 21 upgrade disabled loop vectorization as a workaround — performance claims
  about vectorized loops must be measured, not assumed (release notes, KNOWN).

## Adversarial evals

- The NEON counter-overflow mirror: a u8 accumulator that passes ReleaseFast (wrapping
  `@reduce(.Add)`) and panics in Debug — the guard must be added (widen/saturate), NOT the
  build mode switched.
- A dot-product kernel that "looks right" under `-O3`-style ReleaseFast but silently wraps
  on large inputs — value-range reasoning required.
- A `std.simd` helper called with a guessed signature — must be checked against
  std/simd.zig in the pinned version.

## Verified facts

- KNOWN (from langref §Vectors/§Illegal Behavior and 0.16.0 release notes; not run on
  this host):
  - Vector length is comptime-known; scalar/vector mixing is prohibited; `@splat`
    broadcasts.
  - 0.16.0 forbids runtime vector indexes (coerce to array).
  - `@ptrCast` between vectors and arrays is Illegal Behavior; `@bitCast`/coercion is the
    route.
  - `@reduce(.Add)`/`.Mul` on integral types are wrapping; integer vector overflow via
    `+` is safety-checked Illegal Behavior in Debug/ReleaseSafe.
  - Vectors shorter than the native SIMD size typically compile to single instructions;
    longer split; unsupported ops degrade element-wise; lengths up to 2^32-1, very long
    ones may crash the compiler.
- INFERRED: the exact `std.simd` function surface (std/simd.zig drifts across versions);
  LLVM lowering details (llvm.vector.reduce / shufflevector / select).
- UNVERIFIED (needs zig on this host): actual test output and panic messages.

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. Host x86_64 has SSE/AVX, so the good
  examples should lower to real SIMD instructions once zig is available; QEMU-based
  vector tests on ARM targets are the documented second pass (QEMU absent here).
