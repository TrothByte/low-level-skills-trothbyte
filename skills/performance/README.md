# performance — Skills

Performance work starts with measurement. These skills enforce the measure-before-optimize discipline and teach cache/NUMA-aware code that actually moves real timings.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `cache-and-numa-optimization` | Use when writing or reviewing memory-bound C code where cache behavior and NUMA placement dominate performance: false sharing, cache-line padding, row-major vs column-major access, struct-of-arrays vs array-of-structs, strided access, prefetching, and NUMA node-local allocation with numactl. | source-backed | `skills/performance/cache-and-numa-optimization` |
| `performance-measurement-discipline` | Use when asked to optimize or benchmark C code, or when a performance claim needs evidence — profiling before changes, benchmark harness correctness, warmup and repetitions, dead-code elimination of benchmarks, statistical noise, regression baselines, and microbenchmark pitfalls like inlining and aliasing. | source-backed | `skills/performance/performance-measurement-discipline` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
