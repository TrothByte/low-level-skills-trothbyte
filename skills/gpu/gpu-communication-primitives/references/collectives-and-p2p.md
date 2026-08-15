# GPU Communication Primitives — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to `registry/sources.yaml`.

## 1. Ring all-reduce: reduce-scatter then all-gather, chunked

- **RULE**: ring all-reduce with N ranks splits the buffer into N chunks. Phase 1
  (reduce-scatter): each rank sends chunk `(rank - step) mod N` to the next rank
  each step; after N-1 steps each rank holds a chunk that is the sum of all ranks'
  values for that chunk. Phase 2 (all-gather): the partial chunks rotate again until
  every rank holds every chunk. Total data moved ≈ 2·(N-1)/N · buffer.
- **WHY AI GETS IT WRONG**: treats all-reduce as "send my whole buffer, receive the
  whole buffer" (that's an incorrect gather); misindexes chunks, or starts all-gather
  from the wrong offset, so the summed data lands on the wrong chunk and the result
  is silently shuffled.
- **CORRECT REASONING**: work the rotation by hand for N=3 (not just N=4 or 2^k):
  chunk indices must be consistent between the reduce-scatter phase and the
  all-gather phase. The receiver accumulates `local[c] += incoming[c]` for exactly
  N-1 steps, then forwards; any off-by-one in the chunk index corrupts one chunk only
  — a bug that survives an `allclose` on the other chunks.
- **EXAMPLE** (bad): `next = (rank + step + 1) % n` for the reduce-scatter but
  `next = (rank - step) % n` for the all-gather — the partials are rotated onto
  different chunk positions than the gather phase reads.
- **COUNTEREXAMPLE** (good): both phases derive chunk and target from the SAME
  formula, e.g. `target = (rank + step) % n`; chunk = `(rank - step) mod n` for
  phase 1 and = `(rank + step + 1) mod n` for phase 2, each validated against the
  N=3 hand-computation.
- **VERIFICATION**: `python examples/good/sim_ring_allreduce.py` — recorded output
  shows the N=3 rotation matches a sequential sum; with the wrong rotation the
  result is a permutation with wrong values.
- **SOURCE**: `nccl-docs` (ring algorithm), `arxiv-2608-04450` (rotation bugs are a
  CommBench failure class).

## 2. All-gather output layout is a contract

- **RULE**: the all-gather output must be ordered by rank: rank 0's buffer first,
  then rank 1's, etc. (contiguous segments of `count` elements per rank). The same
  layout rule applies to gather-from/one-rank.
- **WHY AI GETS IT WRONG**: writes all chunks then "sorts later", or indexes the
  destination by the source rank instead of by the rank-order position, producing a
  buffer that has the right bytes but in permuted rank order.
- **CORRECT REASONING**: the output layout is defined by the API. NCCL's
  `ncclAllGather` expects rank-order contiguous segments; `ncclGather` collects
  into `recvbuff + rank*count`. If a different order is needed, an explicit
  permutation is required AFTER the collective, not inside it.
- **EXAMPLE** (bad): `dest[(rank + i) % n]` when filling the all-gather output.
- **COUNTEREXAMPLE** (good): `memcpy(recvbuff + rank * count, sendbuff, count)`.
- **VERIFICATION**: with N=3 and per-rank `{rank*100, rank*100+1}`, the output must
  be `0,1,100,101,200,201`; a rotation returns `0,1,200,201,100,101`. Recorded in
  sim.
- **SOURCE**: `nccl-docs` (AllGather contract); CommBench layout bugs.

## 3. NCCL is stream-ordered and per-communicator ordered

- **RULE**: NCCL collectives run on a CUDA stream; the caller must pass the stream
  and the same communicator, in the same order on every rank. NCCL operations
  enqueued on different streams are not ordered without an explicit event dependency.
- **WHY AI GETS IT WRONG**: passes `0` (default stream) from multiple threads, or
  lets different ranks issue collectives in different orders; the result is a
  deadlock or corrupt data that a single-rank test never shows.
- **CORRECT REASONING**: every rank must enqueue the same sequence of collective
  calls on the same communicator. Cross-stream ordering needs
  `cudaStreamWaitEvent`. NCCL performs an implicit device synchronization only when
  you call `cudaDeviceSynchronize`/`ncclCommDestroy` — do not rely on it.
- **EXAMPLE** (bad): rank 0 calls `ncclAllReduce` then `ncclBcast`; rank 1 calls
  `ncclBcast` then `ncclAllReduce` — the communicator deadlocks (or mismatches).
- **COUNTEREXAMPLE** (good): both ranks call AllReduce, then both call Bcast, each
  with the same stream.
- **VERIFICATION**: two-rank MPI/NCCL run (documented; no GPU here).
- **SOURCE**: `nccl-docs` (usage/ordering), `cuda-cpp-guide` §3.2.8 (streams).

## 4. In-place vs out-of-place is part of the contract

- **RULE**: `ncclAllReduce(sendbuff, recvbuff, ...)` is out-of-place; with
  `sendbuff == recvbuff` it is in-place and some algorithms (esp. on some
  architectures) require it to be a device pointer. Mixing the two across ranks is
  undefined.
- **WHY AI GETS IT WRONG**: assumes every collective silently supports aliasing;
  passes host pointers, or mixes in-place on one rank with out-of-place on another.
- **CORRECT REASONING**: pick one mode for all ranks and check `ncclResult_t`.
  NCCL requires device pointers for send/recv buffers (the host pointer form exists
  only for the CUDA-aware MPI bridge, which is out of scope here).
- **EXAMPLE** (bad): `ncclAllReduce(h_in, d_out, ...)` with a host buffer.
- **COUNTEREXAMPLE** (good): `ncclAllReduce(d_in, d_out, count, dtype, op, comm, stream)`
  and check the result.
- **VERIFICATION**: `ncclResult_t` return value is non-`ncclSuccess` on a host
  pointer (documented, not run).
- **SOURCE**: `nccl-docs` (memory requirements).

## 5. Peer-to-peer requires explicit peer access and UVA

- **RULE**: `cudaDeviceEnablePeerAccess` must be called on BOTH devices that will
  exchange data, and unified addressing (UVA, 64-bit + `cudaDeviceProp` checks)
  must hold; then `cudaMemcpyPeer` can copy device-to-device directly (NVLink/PCIe).
- **WHY AI GETS IT WRONG**: calls `cudaMemcpyPeer` without enabling peer access and
  never checks the return code — the copy silently falls back to a staged
  host-mediated copy (or fails), and the agent believes it used a fast path.
- **CORRECT REASONING**: peer access is a bidirectional capability; enable it from
  both sides, verify `canAccessPeer`, and treat the fallback as a performance
  signal, not an error — but check the API result either way.
- **EXAMPLE** (bad): `cudaMemcpyPeer(d0, dev0, d1, dev1, bytes)` with no
  `cudaDeviceEnablePeerAccess` and the error swallowed.
- **COUNTEREXAMPLE** (good):
  ```cuda
  cudaDeviceEnablePeerAccess(dev1, 0);  // from dev0
  cudaDeviceEnablePeerAccess(dev0, 0);  // from dev1
  cudaMemcpyPeer(d0, dev0, d1, dev1, bytes);  // error checked
  ```
- **VERIFICATION**: `cudaGetLastError()`/return codes checked; `nvidia-smi topo -m`
  shows the NVLink/PCIe topology (documented, no GPU here).
- **SOURCE**: `cuda-cpp-guide` §3.2.7 (peer-to-peer access).

## 6. Expert-parallel: token conservation is the invariant

- **RULE**: in expert-parallel sharding, every input token that is routed to a
  remote expert must arrive there, and every remote expert must return its output to
  the originating rank. The conservation check is `sum(received) == sum(sent)`.
- **WHY AI GETS IT WRONG**: routes only a subset ("this batch goes to rank 1")
  without a full index exchange; the missing tokens silently drop and the model
  trains on truncated data.
- **CORRECT REASONING**: build the full routing table first (which token → which
  rank/expert), exchange counts, then exchange payloads; verify conservation in a
  debug build before trusting training correctness.
- **EXAMPLE** (bad): an all-to-all that sends only `local_expert_id == rank`
  tokens and drops the rest.
- **COUNTEREXAMPLE** (good): count exchange (`ncclAllGather` of per-rank send
  counts), then all-to-all of packed buffers; assert conservation on both ends.
- **VERIFICATION**: host-side counter assert; sim-style bookkeeping.
- **SOURCE**: `arxiv-2608-04450` (expert-parallel failure classes).

## 7. Check every NCCL/p2p return code

- **RULE**: every `ncclXxx` returns `ncclResult_t`; every CUDA call returns
  `cudaError_t`. A dropped error leaves a partially-written buffer that an
  `allclose` may not catch because the corrupt region is small or unread.
- **WHY AI GETS IT WRONG**: checks only the last call; NCCL errors surface on the
  NEXT call in some configurations, and "it ran without crashing" is mistaken for
  "it returned success".
- **CORRECT REASONING**: wrap calls in a checked helper; call `ncclGetLastError` /
  `cudaGetLastError` after every API group. Treat any non-success as a failed
  verification, not a warning.
- **EXAMPLE** (bad): `ncclAllReduce(...); cudaMemcpy(...);` with neither result
  checked.
- **COUNTEREXAMPLE** (good): `NCCL_TRY(ncclAllReduce(...)); CUDA_TRY(cudaMemcpy(...));`
- **VERIFICATION**: `NCCHK`/`CUDACHK` macros with `exit()` on failure (documented).
- **SOURCE**: `nccl-docs` (error handling), `cuda-cpp-guide` §3.2.7.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Ring all-reduce | N chunks; reduce-scatter then all-gather; validate rotation for N=3 |
| All-gather layout | rank-order contiguous output; permute AFTER, not inside |
| Ordering | same collectives, same order, same communicator, one stream |
| In/out-of-place | pick one mode for all ranks; device pointers only |
| p2p | enable peer access both sides; check canAccessPeer; UVA required |
| Expert-parallel | full routing table; token conservation is the invariant |
| Errors | check every ncclResult_t/cudaError_t; errors can surface late |
