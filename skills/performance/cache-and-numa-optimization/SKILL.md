---
name: cache-and-numa-optimization
description: Use when writing or reviewing memory-bound C code where cache behavior and NUMA placement dominate performance: false sharing, cache-line padding, row-major vs column-major access, struct-of-arrays vs array-of-structs, strided access, prefetching, and NUMA node-local allocation with numactl.
---

# Cache & NUMA Optimization

## When to use

- Diagnosing a multithreaded program that scales poorly or slows down when
  more threads are added (false sharing is the prime suspect).
- Making a memory-bound loop faster: choosing iteration order (row-major vs
  column-major), data layout (SoA vs AoS), or alignment/padding.
- Explaining why a loop that "should be fast" is bound by cache misses:
  strided access, padding waste, pointer-chase latency.
- Deciding whether software prefetch helps, and at which cache level.
- Placing memory on the correct NUMA node, choosing a memory policy, or
  interpreting `numactl` output on Linux.

## When not to use

- Correctness-only C questions with no performance context (use
  `c-undefined-behavior` / `safe-low-level-from-scratch`).
- Vectorization reasoning — vectorizer reports, `restrict`, `-fopt-info`
  (use `simd-vectorization-cross-layer`).
- Compute-bound code where the bottleneck is ALU throughput, not memory:
  layout changes will not help; measure first.
- Memory-ordering correctness. Atomics/barriers do not fix false sharing;
  false sharing is not a data race (use `memory-ordering-reasoning` /
  `atomics-c11-cpp11-rust`).
- End-to-end profiling methodology (use `performance-measurement-discipline`).

## What the agent often gets wrong

- "Padding fixes false sharing." Padding fixes it only when each thread truly
  writes its OWN object; if threads share one object, restructure the
  algorithm (per-thread partials + reduction), padding just wastes memory.
- "`_Alignas(64)` is enough to fix false sharing." Alignment alone is not:
  two objects aligned to 64 can still sit in one line if they are smaller
  than 64 bytes. You need alignment AND a stride of at least one line.
- "More threads must be faster." False sharing and contention can make a
  2-thread run slower than 1 thread. Always verify with a wall-clock run.
- "Struct padding is a compiler bug." Padding is implementation-defined ABI
  layout, not a defect; it only matters as bandwidth waste in hot loops.
- "Row-major vs column-major is a style choice." In C, arrays are row-major;
  loop nesting order decides which layout is cache-friendly. Swapping the two
  loops is the cheapest locality fix.
- "`__builtin_prefetch` speeds up slow loops." Hardware prefetchers already
  cover sequential and small-stride patterns; unmeasured prefetch usually
  slows the loop down (costs an issue slot and address math).
- "NUMA only matters on huge servers." Any multi-socket machine and many
  many-core single-socket machines have multiple memory controllers; first
  touch decides where pages land.
- "`numactl --cpunodebind=0` also binds memory." It does not; memory policy is
  separate and defaults to first-touch.

## How to reason correctly

1. Classify the bottleneck first: is the loop memory-bound or compute-bound?
   A memory-bound loop stalls on loads; verify with counters, not intuition.
2. For a memory-bound loop, count bytes the access pattern forces the cache
   to fetch (whole 64-byte lines) vs bytes the computation actually uses.
   Cost is bytes-fetched, not bytes-used.
3. For multithreaded code: do threads write DISJOINT cache lines? Different
   objects in the same line is false sharing. Fix by layout (pad each hot
   object to its own line) or algorithm (per-thread buffers, reduction).
4. Prefer layout changes (padding, SoA, loop-order swap) over prefetch or
   `#pragma` — layout removes the root cause of bandwidth waste.
5. For NUMA: decide where data will be touched, allocate it there, and keep
   the touching threads on that node. First touch, not malloc, binds pages.
6. Always verify with a before/after wall-clock measurement of the real
   workload, run several times, and take best/median (scheduling and turbo
   jitter one-off runs).

## What to verify

- The loop is actually memory-bound (high miss rate / stall ratio), not
  compute-bound — otherwise the cache change is noise.
- Layout changes preserve correctness and add no races: threads must never
  write the same cache line; atomics must remain correct.
- The false-sharing diagnosis is real: the padded twin runs measurably
  faster on the same machine.
- A "row-major" fix actually changes the access order, not just the variable
  names.
- NUMA claims (`numactl`, node placement, first touch) are verified on Linux;
  on Windows they are documented, not executable.

## How to verify

```
# False sharing (Windows, GCC 16.1 MinGW): wall time of each file
gcc -O2 -pthread examples/bad/false_sharing.c -o fs_bad.exe && ./fs_bad.exe
gcc -O2 -pthread examples/good/padded_counters.c -o fs_good.exe && ./fs_good.exe

# Locality: column-major vs row-major over the same 4096x4096 matrix
gcc -O2 examples/bad/strided_access.c -o strided.exe && ./strided.exe
gcc -O2 examples/good/row_major.c -o rowmajor.exe && ./rowmajor.exe

# Layout: AoS (24-byte elements) vs SoA over the same 8M elements
gcc -O2 examples/bad/array_of_structs.c -o aos.exe && ./aos.exe
gcc -O2 examples/good/struct_of_arrays.c -o soa.exe && ./soa.exe
```

Run each several times and take best/median. On Linux confirm with:

```
perf stat -e cache-misses,cache-references,cycles ./rowmajor.exe
numactl --hardware                        # NUMA topology
numactl --cpunodebind=0 --membind=0 ./prog
```

## Where the knowledge comes from

- `intel-opt-manual` — cache hierarchy, memory optimization, false sharing,
  AoS/SoA, prefetching guidance
- `agner-fog` — microarchitecture/optimization manuals, cache and latency
  data
- `perf-wiki` — `perf stat`/`perf record` cache-miss measurement methodology
- `iso-c11-n1570` — §6.2.8 alignment, §7.22.3.1 `aligned_alloc`
- `gcc-manual` — `__builtin_prefetch`, `aligned_alloc`, attributes
- `numactl(8)` — Linux NUMA policy tool (not in sources registry; documented
  as Linux target, UNVERIFIED on Windows)

## Related skills

- `performance-measurement-discipline` — measure before optimizing (require of)
- `simd-vectorization-cross-layer` — the other half of memory-bound
  performance (recommend)
- `memory-ordering-reasoning` — what atomics/barriers do and do not do for
  cache (recommend)

## Evaluation

Synthetic: false sharing (adjacent vs padded counters), column-major vs
row-major traversal, AoS vs SoA bandwidth — measured on GCC 16.1 (MinGW,
x86-64) on Windows; numbers in `evals/README.md`.
False-positive: already-cache-friendly code (row-major loop, SoA, per-thread
padded counters) must not be flagged; prefetch recommended only when it
measurably helps.
Adversarial: "prefetch fixes everything", "padding is always the answer",
"NUMA is irrelevant on a workstation", "cpunodebind binds memory" must be
rejected.
NUMA rules are source-backed but VERIFIED as Linux targets only; on Windows
they are documented, not executed.
