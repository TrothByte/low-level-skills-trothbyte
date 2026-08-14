# simd — Skills

SIMD is where CPUs get fast. These skills bridge compiler auto-vectorization and hand-written vector code: reading -fopt-info, finding blockers, and reasoning about alignment and aliasing.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `simd-vectorization-cross-layer` | Use when reasoning about why a C loop did or did not vectorize, reading GCC `-fopt-info-vec`/`-fopt-info-missed-vec` or Clang `-Rpass=loop-vectorize` output, diagnosing aliasing, loop-carried dependency, alignment, or trip-count blockers, choosing between auto-vectorization, `restrict`, runtime dispatch, and intrinsics, and inspecting xmm/ymm vector asm. | source-backed | `skills/simd/simd-vectorization-cross-layer` |
| `vectorization-reasoning` | Use when analyzing whether a C loop can vectorize or why it does not — loop-carried dependencies, aliasing and restrict, known vs unknown trip counts, reductions, induction variables, alignment and cost-model assumptions, and interpreting GCC -fopt-info-vec and missed reports before touching intrinsics. | source-backed | `skills/simd/vectorization-reasoning` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
