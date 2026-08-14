# GPU Memory Model & Coherence — Reference

Sources: `cuda-cpp-guide` (CUDA C++ Programming Guide, current edition: §2.3, §2.5.1,
§3.2.8.5, §7.5, §7.6, §7.14, §14.5.3.3), `ptx-isa` (PTX ISA 8.5: §5.1, §8, §9.7.10.8/11,
§9.7.14.1, §9.7.14.5), `hip-docs` (HIP Programming Model). Claims marked
[SELF-REVIEWED] were validated against the source text during authoring, not with a
running GPU toolchain. Targets: `nvcc`, `compute-sanitizer`.

## 1. GPU memory hierarchy

- **RULE**: Threads access four main spaces: local (private per thread), shared
  (per thread block, same lifetime as the block), global (all threads of the grid,
  persistent across launches), and constant (read-only, cached). On HIP: registers/LDS
  (shared)/HBM (global)/constant/texture.
- **WHY AI GETS IT WRONG**: treats shared memory as "global cache" visible to the whole
  grid, or assumes global memory is the only space that needs synchronization.
- **CORRECT REASONING**: shared memory belongs to one block (plus cluster peers on
  sm_90+); only `.global` is the mechanism for threads in different blocks/clusters to
  communicate. The space determines which synchronization primitive is legal.
- **EXAMPLE** (bad): block 3 reads `sh[]` written by block 0 — shared memory is not
  visible across blocks.
- **COUNTEREXAMPLE** (good): inter-block data goes through global memory; shared memory is
  used only for within-block cooperation.
- **VERIFICATION**: code review of the space qualifiers; `nvcc -ptx` shows
  `ld.shared`/`ld.global`/`ld.const`.
- **SOURCE**: `cuda-cpp-guide` §2.3; `ptx-isa` §5.1.4 (global), §5.1.7 (shared);
  `hip-docs` "Memory model".

## 2. Coherence domains and thread scopes

- **RULE**: Ordering/atomicity guarantees are scoped: `thread` (one thread),
  `block`/`cta` (all threads of one block), `device`/`gpu` (all threads on the GPU),
  `system`/`sys` (host + device + peer devices). A block-scope operation orders only
  within the block; a device-scope operation reaches the whole device.
- **WHY AI GETS IT WRONG**: uses a `.cta`-scope fence/barrier for a flag consumed by
  another block, or assumes `atomicAdd` is device-wide by default.
- **CORRECT REASONING**: pick the scope by where the peer thread lives. CUDA atomic
  functions default to device scope; the `_block` suffix narrows to block scope; the
  `_system` suffix widens to host+device. PTX `atom` defaults to `.gpu` scope and
  `.relaxed` ordering.
- **EXAMPLE** (bad): `atomicAdd_block(flag, 1)` to publish work for a different block —
  the atomicity only covers one block.
- **COUNTEREXAMPLE** (good): plain `atomicAdd(flag, 1)` (device scope) or
  `atomicAdd_system(...)` when the host also participates.
- **VERIFICATION**: cross-check the suffix/scope against the peer's location; inspect PTX
  with `nvcc -ptx`.
- **SOURCE**: `cuda-cpp-guide` §2.5.1 (thread scopes), §7.14 (atomic scopes);
  `ptx-isa` §8.5 (scope), §9.7.14.5 (atom defaults).

## 3. No automatic shared-memory coherence — `__syncthreads()` is required

- **RULE**: There is no automatic coherence of shared memory between threads of a block.
  `__syncthreads()` acts as a barrier at which all threads of the block wait, and it
  makes all global and shared memory accesses made before it visible to all threads in
  the block.
- **WHY AI GETS IT WRONG**: "shared is on-chip, so writes are instantly visible." A
  shared store is visible to the storing thread immediately, but not to other threads
  until a block-wide barrier orders it.
- **CORRECT REASONING**: every shared write followed by another thread's read needs a
  `__syncthreads()` between them (or `__syncwarp` when all participants are one warp).
  Without it there is a read-after-write/write-after-write hazard.
- **EXAMPLE** (bad): each thread writes `sh[t] = in[t];` then reads `sh[t-1]` with no
  barrier (see `examples/bad/bad_shared_no_syncthreads.c`).
- **COUNTEREXAMPLE** (good): `sh[t] = in[t]; __syncthreads(); out[t] = sh[t-1];`
  (see `examples/good/good_shared_syncthreads.c`).
- **VERIFICATION**: `compute-sanitizer --tool racecheck` flags the bad kernel's shared
  access and stays clean on the good kernel.
- **SOURCE**: `cuda-cpp-guide` §7.6 (`__syncthreads()`); `ptx-isa` §9.7.14.1 (`bar.sync`).

## 4. `__syncthreads()` must be reached by every non-exited thread

- **RULE**: `__syncthreads()` is allowed in conditional code only if the condition
  evaluates identically across the whole block; otherwise the kernel is likely to hang or
  produce unintended side effects. Since Volta, barriers are enforced per thread.
- **WHY AI GETS IT WRONG**: guarding the barrier with `if (threadIdx.x == 0)` looks
  harmless because the kernel "usually works" — it deadlocks or misbehaves
  nondeterministically.
- **CORRECT REASONING**: a barrier synchronizes ALL participating threads; a subset
  reaching it waits forever. Keep barriers in uniform control flow, or exit threads
  after their last barrier.
- **EXAMPLE** (bad): `if (threadIdx.x < 8) __syncthreads();` in a 32-thread block.
- **COUNTEREXAMPLE** (good): `__syncthreads();` executed unconditionally by every thread.
- **VERIFICATION**: code review of control flow; `compute-sanitizer` reports barrier
  violations on a real run.
- **SOURCE**: `cuda-cpp-guide` §7.6; `ptx-isa` §9.7.14.1.

## 5. Volatile in CUDA and its limits

- **RULE**: CUDA C++ `volatile` reads/writes are NOT atomic and give NO ordering
  guarantees. The Programming Guide states volatile is not suitable for inter-thread
  synchronization or MMIO; it compiles to `.volatile` PTX instructions whose number of
  memory operations is not even guaranteed to match.
- **WHY AI GETS IT WRONG**: "volatile prevents caching, so it synchronizes threads" —
  true for a single producer/consumer register-style flag on some CPUs, false on GPUs.
- **CORRECT REASONING**: use `cuda::atomic`/`cuda::atomic_ref` with acquire/release, or
  the atomic functions, for synchronization. Volatile still has a narrow role (e.g. a
  `volatile` result array so stores bypass L1 and are seen by a later block — the
  guide's last-block example) but never as a sync primitive.
- **EXAMPLE** (bad): `volatile unsigned int flag;` used as a cross-thread ready flag
  without atomics.
- **COUNTEREXAMPLE** (good): `cuda::atomic_ref<int, cuda::thread_scope_device> f{*flag};`
  with `f.store(1, memory_order_release)` / `f.load(memory_order_acquire)`.
- **VERIFICATION**: grep for `volatile` used as a sync flag; confirm ordering is provided
  by atomics instead.
- **SOURCE**: `cuda-cpp-guide` §14.5.3.3 (volatile qualifier); `ptx-isa` §9.7.10.8
  (`.volatile` qualifier).

## 6. `atomicAdd` and memory ordering (relaxed by default)

- **RULE**: The CUDA atomic functions (`atomicAdd`, `atomicExch`, ...) have
  `memory_order_relaxed` ordering — atomicity only, no happens-before. Ordering requires
  `cuda::atomic` with acquire/release (e.g. `cuda::thread_scope_device`), or a relaxed
  atomic paired with `__threadfence()`. PTX `atom` defaults to `.relaxed` ordering and
  `.gpu` scope.
- **WHY AI GETS IT WRONG**: assumes `atomicAdd` is seq_cst like a mutex; uses it as a
  "flag with data" publish and the data read races.
- **CORRECT REASONING**: relaxed RMW gives atomicity on the counter but orders nothing
  else. For publish/signal, write the data, fence (or use release), then signal; the
  consumer acquires, then reads the data. `red` (no destination) cannot even form an
  acquire pattern.
- **EXAMPLE** (bad): producer `*data = 42; atomicAdd(flag, 1);` and consumer
  `while (atomicAdd(flag, 0) == 0); read(*data);` — relaxed, the data read is racy
  (see `examples/bad/bad_atomic_flag_relaxed.c`).
- **COUNTEREXAMPLE** (good): producer `*data = 42; __threadfence(); atomicExch(flag, 1);`
  consumer `while (atomicAdd(flag, 0) == 0); __threadfence(); read(*data);`
  (see `examples/good/good_atomic_flag_fence.c`), or `cuda::atomic` with
  acquire/release.
- **VERIFICATION**: `compute-sanitizer --tool racecheck` on the pair; PTX inspection
  (`nvcc -ptx`) shows the fence/`atom.relaxed` mapping.
- **SOURCE**: `cuda-cpp-guide` §7.14, §14.5.3.3; `ptx-isa` §9.7.14.5 (atom), §9.7.14.6
  (red), §8.8 (release/acquire patterns).

## 7. Cross-block visibility: no barrier, use fence + atomic (or kernel boundary)

- **RULE**: There is no cross-block barrier inside one kernel. Blocks coordinate via
  global memory using atomic operations and memory fences; without a fence between a
  store and its signaling atomic, the signal can become visible before the data.
  Alternatively, split the work into two kernel launches — a kernel boundary orders the
  grid.
- **WHY AI GETS IT WRONG**: "the counter can't reach the last value before my store" —
  the store may still be in L1/still in flight; the atomic can win the race.
- **CORRECT REASONING**: insert `__threadfence()` (device scope) or `__threadfence_system()`
  (host + peers) between the data store and the signaling atomic, and volatile if the
  data must bypass L1 (guide's last-block-detection example).
- **EXAMPLE** (bad): store partial sum, then `atomicInc(count, gridDim.x)` with no fence —
  the last block may read stale partial sums (see `examples/bad/bad_cross_block_no_fence.c`).
- **COUNTEREXAMPLE** (good): `partials[bid] = sum; __threadfence(); atomicInc(count, gridDim.x);`
  then the last block reads partials (see `examples/good/good_cross_block_fence.c`).
- **VERIFICATION**: `compute-sanitizer` racecheck; review that every cross-block signal
  has a fence or an `acq_rel` atomic.
- **SOURCE**: `cuda-cpp-guide` §7.5 (memory fences, last-block example); `ptx-isa`
  §9.7.14.4 (fence).

## 8. Host-device synchronization: streams, events, cudaMemcpy

- **RULE**: A stream is a sequence of commands that execute in order; different streams
  may execute out of order or concurrently, and this is not guaranteed — inter-stream
  communication is undefined. Kernel launches are asynchronous; `cudaMemcpy` and
  synchronous copies order against prior work. Use events
  (`cudaEventRecord`/`cudaStreamWaitEvent`) or `cudaStreamSynchronize`/
  `cudaDeviceSynchronize` to join streams.
- **WHY AI GETS IT WRONG**: assumes two kernels on different streams run in issue order,
  or that reading device memory from the host immediately sees kernel writes without a
  sync point.
- **CORRECT REASONING**: completion of a task in a stream synchronizes with the start of
  the following task in the same stream; an event record/wait creates a cross-stream
  edge; `cudaDeviceSynchronize` waits for all streams. On HIP the same stream FIFO rule
  applies.
- **EXAMPLE** (bad): kernel writes `out[]` on stream A, host reads `out[]` after
  launching on stream B with no event/join — the read may race.
- **COUNTEREXAMPLE** (good): copy D2H on the same stream after the kernel (or
  `cudaEventRecord` + `cudaStreamWaitEvent`, or `cudaDeviceSynchronize`).
- **VERIFICATION**: review the stream/event graph; run under `compute-sanitizer` or
  `Nsight Systems` timeline.
- **SOURCE**: `cuda-cpp-guide` §3.2.8.5 (streams), §3.2.8.5.3 (explicit sync), §3.2.8.8
  (events); `ptx-isa` §8.9.4 (stream/event synchronizes-with);
  `hip-docs` "Asynchronous concurrent execution".

## 9. Warp-level behavior

- **RULE**: A warp (32 lanes on NVIDIA; 64 on CDNA, 32 on RDNA) executes converged
  instructions; `__syncwarp(mask)` orders memory among the participating lanes of a
  warp and is UB if the calling thread is not in the mask. Warp-synchronous code
  (assumes lockstep) broke with Volta's independent thread scheduling: never rely on
  implicit warp convergence for correctness — use `__syncwarp` or atomics.
- **WHY AI GETS IT WRONG**: writes `sh[tid]` then `shfl`/plain reads assuming lockstep
  guarantees visibility; or calls `__syncwarp` with a stale mask.
- **CORRECT REASONING**: within a warp, `__syncwarp` gives a cheaper, narrower barrier
  than `__syncthreads`; across warps, only block barriers or atomics work. `shfl.sync`/
  `vote.sync` require converged execution with a correct membermask.
- **EXAMPLE** (bad): `sh[lane] = v; if (lane > 0) use sh[lane-1];` with no `__syncwarp`
  — the neighbor's store may not be visible.
- **COUNTEREXAMPLE** (good): `sh[lane] = v; __syncwarp(); use sh[lane-1];`
- **VERIFICATION**: `compute-sanitizer` racecheck; `ptxas`/`cuobjdump` to confirm
  `BAR.SYNC`/`SHFL` mapping.
- **SOURCE**: `cuda-cpp-guide` §7.6 (`__syncwarp`); `ptx-isa` §9.7.14.2 (`bar.warp.sync`),
  §9.7.10.6 (`shfl.sync`); `hip-docs` "Warp (or Wavefront)".

## 10. Why "it works on CPU" reasoning fails on GPU

- **RULE**: The GPU model is weakly ordered with nondeterministic block scheduling and
  per-SM caches; there is no shared L3 coherence you can rely on, and any data race is
  undefined behavior with no defined semantics. CPU test runs serialize what the GPU
  interleaves.
- **WHY AI GETS IT WRONG**: "the unit test passes on the host/emulator" is treated as
  proof of device correctness; races then surface as flaky results on real hardware.
- **CORRECT REASONING**: correctness must come from the memory model (barriers, fences,
  atomic orders, stream order), not from observed behavior on one machine or one run.
  Host-simulation can validate kernel logic/compilation but never race freedom.
- **EXAMPLE** (bad): validating a flag protocol by running the kernel body in a C loop on
  the CPU.
- **COUNTEREXAMPLE** (good): reasoning about the missing release/acquire edge and
  verifying with `compute-sanitizer --tool racecheck` on the device.
- **VERIFICATION**: `compute-sanitizer` racecheck/synccheck; stress runs; `Nsight Compute`
  on the target architecture.
- **SOURCE**: `cuda-cpp-guide` §7.5 ("Any data-race is undefined behavior");
  `hip-docs` "Grid" (nondeterministic work-group scheduling).

## Quick decision table

| Communication pair | Correct primitive |
|---|---|
| same warp, register/shared | `__syncwarp(mask)` |
| same block, shared/global | `__syncthreads()` |
| cross-block, one kernel | `__threadfence()` + device-scope atomic (release/acquire) |
| cross-kernel / host-device | stream order, events, `cudaMemcpy` sync, `cudaDeviceSynchronize` |
| host + device atomics | `atomicAdd_system` / `cuda::thread_scope_system` |
| statistics counter, order irrelevant | relaxed `atomicAdd` (fine as-is) |
