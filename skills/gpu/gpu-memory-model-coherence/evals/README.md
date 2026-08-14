# Evaluation — gpu-memory-model-coherence

Skill: `skills/gpu/gpu-memory-model-coherence`. Stability: `researched` (source-backed
grounding from the CUDA C++ Programming Guide, PTX ISA and HIP docs; host-compile
verified; not yet `evaluated` — no GPU toolchain on this machine; see Toolchain status).

## Toolchain status

`nvcc` is NOT installed on this machine (`nvcc --version` -> command not recognized), so
`compute-sanitizer` is unavailable too. Consequences, stated honestly:

- The `.c` example files compile as plain C through `examples/cuda_stubs.h` and were
  verified with `gcc -Wall -Wextra -Werror -O2` (host logic + compile-clean only). The
  host run is a serial simulation: it never proves race freedom.
- GPU-only claims are grounded in the primary sources, which were fetched and read during
  authoring (see Verified facts), and marked `[SELF-REVIEWED]` where relevant. They were
  NOT confirmed by a running `nvcc`/`compute-sanitizer`.

Target commands to promote to `verified` (run on a machine with the CUDA toolkit):

```
gcc -Wall -Wextra -Werror -O2 -I examples examples/bad/bad_shared_no_syncthreads.c  -o /tmp/bad_sh
gcc -Wall -Wextra -Werror -O2 -I examples examples/good/good_shared_syncthreads.c  -o /tmp/good_sh
gcc -Wall -Wextra -Werror -O2 -I examples examples/bad/bad_cross_block_no_fence.c   -o /tmp/bad_fence
gcc -Wall -Wextra -Werror -O2 -I examples examples/good/good_cross_block_fence.c    -o /tmp/good_fence
gcc -Wall -Wextra -Werror -O2 -I examples examples/bad/bad_atomic_flag_relaxed.c    -o /tmp/bad_flag
gcc -Wall -Wextra -Werror -O2 -I examples examples/good/good_atomic_flag_fence.c    -o /tmp/good_flag

nvcc -arch=sm_80 -O2 -I examples examples/good/good_shared_syncthreads.c  -o /tmp/g1
nvcc -arch=sm_80 -O2 -I examples examples/good/good_cross_block_fence.c    -o /tmp/g2
compute-sanitizer --tool racecheck /tmp/g1   # must be clean
compute-sanitizer --tool racecheck /tmp/g2   # must be clean
```

Expected results (must confirm): the good kernels run racecheck-clean; the bad kernels are
flagged (shared data race, missing-fence cross-block race, relaxed-flag data race).
Host builds: all six files compile with `-Wall -Wextra -Werror -O2`.

## What was verified (this session)

Host compile of all six examples with `gcc -Wall -Wextra -Werror -O2`; all pass
`-Werror`. Host runs: `bad_shared_no_syncthreads` deterministically returns the wrong
value (thread 0 reads the still-unwritten shared slot), all others return 0 — exactly the
"host hides the race" behavior the skill teaches.

Primary-source facts verified against the fetched texts (CUDA C++ Programming Guide,
current edition via archive snapshot; PTX ISA 8.5; HIP Programming Model docs):

- `__syncthreads()` waits for all threads of the block AND makes prior global/shared
  accesses visible to all block threads; conditional use requires identical evaluation
  block-wide (`cuda-cpp-guide` §7.6).
- CUDA atomic functions have `memory_order_relaxed` ordering, scoped by suffix:
  none = device, `_block` = block, `_system` = system (`cuda-cpp-guide` §7.14).
- CUDA `volatile` is not atomic and gives no ordering; explicitly not suitable for
  inter-thread synchronization (`cuda-cpp-guide` §14.5.3.3).
- `__threadfence_block/__threadfence/__threadfence_system` are seq-cst fences at
  block/device/system scope; the last-block partial-sum pattern requires a fence between
  the store and the counter increment (`cuda-cpp-guide` §7.5).
- Streams execute their commands in order; different streams may run out of order or
  concurrently and must not be relied on for inter-stream communication
  (`cuda-cpp-guide` §3.2.8.5); completion of a stream task synchronizes with the start
  of the following task, events/`cudaStreamSynchronize` create edges (PTX ISA §8.9.4).
- PTX: plain `ld`/`st` default to `.weak` (no sync/atomicity); `atom` defaults to
  `.relaxed` ordering and `.gpu` scope; `.shared` defaults to `::cta` (`ptx-isa`
  §9.7.10.8/11, §9.7.14.5).
- HIP: threads in different work-groups cannot synchronize directly and must use global
  memory atomics or fences; work-group execution order is nondeterministic; stream
  commands are FIFO with side effects visible to subsequent commands in the same stream
  (`hip-docs` Programming Model).

## Synthetic evals

- **easy/negative**: shared-memory exchange without `__syncthreads()`
  (`examples/bad/bad_shared_no_syncthreads.c`) — must flag the missing barrier and fix
  by inserting it between the store and the read.
- **medium/negative**: relaxed-atomic flag protocol
  (`examples/bad/bad_atomic_flag_relaxed.c`) — must identify `memory_order_relaxed`
  semantics and fix with `__threadfence()` on both sides (or `cuda::atomic`
  acquire/release).
- **medium/negative**: cross-block last-block detection without `__threadfence()`
  (`examples/bad/bad_cross_block_no_fence.c`) — must insert the device-scope fence
  between the partial store and `atomicInc`.
- **hard/negative**: `volatile` used as a cross-thread synchronization flag — must
  refuse the pattern and require atomics.
- **adversarial**: a flag protocol that passes host simulation and "worked on x86"
  mental models, i.e. the bad examples above run to exit code 0 on the host — the agent
  must not treat a host pass as correctness and must demand `compute-sanitizer`
  verification.

## False-positive evals (correct code must NOT be flagged)

- Correct `__syncthreads()`-guarded shared exchange (`good_shared_syncthreads.c`) — must
  NOT be flagged for a "missing atomic".
- Correct `__threadfence()` + `atomicExch` message-passing pair
  (`good_atomic_flag_fence.c`) — must NOT be strengthened to seq-cst atomics or `_system`
  scope.
- Correct last-block pattern with `__threadfence()` (`good_cross_block_fence.c`) — must
  NOT be flagged.
- Relaxed `atomicAdd` for a pure statistics counter where order is irrelevant — relaxed
  is the correct choice; must NOT be flagged.
- Same-stream kernel + `cudaMemcpy` D2H sequence — stream order is a valid
  synchronization edge; must NOT be flagged for missing events.

## Scoring

- detection: names the missing primitive (`__syncthreads`/`__threadfence`/release-acquire
  edge) and the exact lines.
- reasoning: explains the mechanism (shared coherence requires a barrier; relaxed atomics
  order nothing; fences order only the calling thread's operations), not just "add a
  barrier".
- fix: minimal correct change; does not over-strengthen (no blanket `fence.sc.sys`,
  no seq-cst everywhere).
- verification: cites `compute-sanitizer --tool racecheck` + `nvcc` as targets and
  states the host run only validates compilation/logic.

## Promotion checklist (to `evaluated`)

1. Run the `gcc` commands above on any machine (already passing here).
2. Run the `nvcc` builds + `compute-sanitizer --tool racecheck` on a CUDA machine;
   good kernels clean, bad kernels flagged.
3. Confirm PTX mapping with `nvcc -ptx` (`ld.shared`/`bar.sync`, `atom.relaxed.gpu`,
   `membar`/fence) against the claims in `references/gpu-memory-model.md`.
4. Re-verify `[SELF-REVIEWED]` marks against the real toolchain output.
