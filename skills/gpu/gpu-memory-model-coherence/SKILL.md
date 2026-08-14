---
name: gpu-memory-model-coherence
description: Use when writing, reviewing, or debugging CUDA/HIP kernels where memory coherence matters — shared-memory races without __syncthreads(), relaxed atomics used as synchronization, cross-block visibility without __threadfence(), volatile for inter-thread sync, or host-device ordering. Teaches GPU memory hierarchy, coherence scopes, and correct synchronization.
---

# GPU Memory Model & Coherence Scope (CUDA/HIP)

## When to use

- Reviewing or writing kernels that exchange data between threads of a block through
  shared memory or global memory.
- Diagnosing intermittently wrong GPU results: "works sometimes", "works on my laptop",
  "passed 100 runs then failed".
- Choosing how to publish data between blocks (atomics, fences, kernel boundaries).
- Host-code that reads/writes device data (streams, events, `cudaMemcpy`, managed memory).
- Porting CPU multi-threaded code (pthreads/OpenMP) to CUDA or HIP.

## When not to use

- Pure single-thread kernels with no cross-thread communication.
- PTX instruction-level review (state spaces, operand widths) — use `ptx-assembly`.
- CPU-only memory ordering (x86/ARM) — use `memory-ordering-reasoning`.
- Kernel performance tuning (occupancy, coalescing, bank conflicts) — performance guide,
  not correctness.

## What the agent often gets wrong

- "Threads of a block can read each other's shared-memory writes right away."
  NO. Shared memory has no automatic coherence between threads; a write is only
  guaranteed visible after `__syncthreads()` (a block barrier).
- "`__syncthreads()` inside `if (tid == 0)` is fine." NO. The barrier must be reached by
  every non-exited thread of the block, or the kernel hangs or is undefined.
- "`atomicAdd` is a full synchronization point." NO. CUDA atomic functions are
  `memory_order_relaxed` by default — atomicity only, no ordering.
- "`volatile` makes a flag safe across threads." NO. CUDA volatile is not atomic and
  gives no ordering; the Programming Guide explicitly forbids using it for inter-thread
  synchronization.
- "Writes to global memory from block A are immediately visible to block B." NO. Without
  a fence+atomic protocol (or a kernel boundary), the ordering and visibility are
  undefined; the L1/L2 hierarchy may serve stale data.
- "Two kernel launches on different streams are ordered." NO. Streams order their own
  commands; cross-stream ordering requires events or synchronization calls. The guide
  states inter-stream/inter-kernel communication is undefined.
- "It works on the CPU/on one GPU generation, so it is correct." GPU block scheduling is
  nondeterministic; a race can hide for thousands of runs.

## How to reason correctly

1. Classify the communicating threads: same warp, same block, same grid (different
   blocks), host vs device. Each pair needs a specific synchronization primitive.
2. For same-block communication, insert `__syncthreads()` between every write and the
   dependent reads. Verify it is reached by ALL non-exited threads.
3. For cross-block communication, use a release/acquire protocol: publish data, then
   `__threadfence()` (or an `acq_rel` atomic), then signal with an atomic that the
   consumer acquires. Never rely on plain stores/loads or relaxed atomics.
4. For host-device communication, use stream order (`cudaMemcpy`/kernel in the same
   stream), or events/`cudaStreamSynchronize`/`cudaDeviceSynchronize`.
5. Treat any cross-thread non-atomic access as a data race = undefined behavior
   (CUDA guide: "Any data-race is undefined behavior, and has no defined semantics").
6. When in doubt, prefer `cuda::atomic` with explicit `memory_order_acquire/release` and
   the right thread scope over bare atomic functions + guesswork.

## What to verify

- Every shared-memory write has a matching `__syncthreads()` (or `__syncwarp`) before any
  thread reads it; the barrier is outside divergent code.
- Cross-block flags use release/acquire (atomic `acq_rel`/`release` + `acquire`, or
  relaxed atomic + `__threadfence()` on both sides). Plain loads/stores do not count.
- Atomic scope matches the peers: block (`_block` suffix / `thread_scope_block`), device
  (default `atomicAdd`), system (`_system` suffix / `thread_scope_system`).
- Host side: kernels and copies that exchange data are in one stream or joined by
  events; reads after `cudaMemcpy` D2H observe the kernel's writes.
- No `volatile` used for inter-thread synchronization.

## How to verify

```
# Target verification (run on a machine with the CUDA toolkit):
nvcc -arch=sm_80 -O2 examples/good/good_shared_syncthreads.cu   -o /tmp/g1
nvcc -arch=sm_80 -O2 examples/good/good_cross_block_fence.cu    -o /tmp/g2
compute-sanitizer --tool racecheck examples/bad/bad_shared_no_syncthreads.cu
compute-sanitizer --tool racecheck examples/good/good_shared_syncthreads.cu
# racecheck must report the shared race in the bad kernel and stay clean on the good one.

# Host-side compile check (what this machine can run; kernel logic only):
gcc -Wall -Wextra -Werror -O2 -I examples examples/bad/bad_shared_no_syncthreads.c
```

Do not claim `nvcc`/`compute-sanitizer` were run here — they are documented targets.

## Where the knowledge comes from

- `cuda-cpp-guide` — CUDA C++ Programming Guide §2.3 (memory hierarchy), §2.5.1 (thread
  scopes), §3.2.8.5 (streams/events), §7.5 (memory fences), §7.6 (synchronization
  functions), §7.14 (atomic functions), §14.5.3.3 (volatile qualifier)
- `ptx-isa` — PTX ISA §8 (memory consistency model, scope, release/acquire),
  §5.1 (state spaces), §9.7.10.8/11 (ld/st defaults), §9.7.14.5 (atom defaults),
  §9.7.14.1 (bar.sync)
- `hip-docs` — HIP Programming Model (memory hierarchy, cross-work-group sync, streams)

## Related skills

- `ptx-assembly` — PTX mapping of the same coherence rules (require of)
- `memory-ordering-reasoning` — release/acquire model reused at GPU scope (require of)
- `atomics-c11-cpp11-rust` — atomics API reference, CUDA follows C++ memory orders
- `c-undefined-behavior` — data race is UB, which is what these rules avoid

## Evaluation

Synthetic: shared-memory exchange without `__syncthreads()`, relaxed-atomic flag protocol,
cross-block last-block detection without `__threadfence()`, `volatile` used as a sync flag,
divergent `__syncthreads()`.
False-positive: correct `__syncthreads()`-guarded exchange, correct
`__threadfence()` + `atomicExch` publish, relaxed `atomicAdd` for a statistics counter,
and a same-stream kernel+memcpy must NOT be flagged.
Adversarial: a flag protocol that "works" on x86 and in 10k host-simulation runs but is a
GPU race. Verification targets: `nvcc`, `compute-sanitizer --tool racecheck` (documented,
not run on this machine).
