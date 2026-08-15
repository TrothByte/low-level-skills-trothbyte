---
name: gpu-communication-primitives
description: Use when designing or reviewing multi-GPU kernels that exchange data: point-to-point peer transfers, ring and tree all-reduce/all-gather collectives, NCCL usage and topology choices, expert-parallel sharding. Prevents deadlocks, wrong chunking, and non-deterministic or unsafe collectives that silently corrupt data.
---

# GPU Communication Primitives (p2p, collectives, NCCL)

## When to use

- Writing or reviewing multi-GPU code: peer-to-peer copy, `ncclAllReduce`, broadcast,
  all-gather, scatter/gather, expert-parallel routing.
- Deciding between ring vs tree vs double-binary-tree algorithm for a collective.
- Checking NCCL usage: communicator creation, stream semantics, in-place vs
  out-of-place buffers, result types, device ordering.
- Diagnosing a wrong-result or hang that only appears with 2+ GPUs.
- Verifying an LLM-generated collective against the CommBench failure patterns
  (best model scored 30.7%: most generated collectives are wrong).

## When not to use

- Single-GPU kernel logic — use `gpu-kernel-verification-beyond-oracle`.
- The memory model / coherence of a single device — use `gpu-memory-model-coherence`.
- CPU MPI — use `hpc-mpi-programming` (same algorithms, different APIs).
- RDMA/InfiniBand low-level verbs — use `hpc-rdma-verbs`.

## What the agent often gets wrong

- "NCCL collectives are like memcpy: just copy." No — collectives have in-place /
  out-of-place contracts, a `ncclResult_t`, and must be passed a valid CUDA stream;
  ignoring the stream breaks ordering with surrounding kernels.
- "Ring all-reduce: each rank sends a full buffer to the next." No — ring
  all-reduce splits the buffer into N chunks and runs reduce-scatter + all-gather;
  chunk indexing and step rotation are where implementations break.
- "Tree and ring are interchangeable." No — latency-bound vs bandwidth-bound:
  rings are better at large messages, trees at small ones with many nodes; wrong
  choice shows up as performance, not correctness.
- "Expert-parallel: just all-to-all the experts." Wrong send/receive counts per rank,
  or routing only the "current batch", drops data and corrupts the model.
- "Peer-to-peer needs no setup." `cudaDeviceEnablePeerAccess` and UVA must be
  established; otherwise `cudaMemcpyPeer` silently falls back to staged copies or
  errors.
- "All-gather must keep the data in rank order." The output layout (per-rank
  contiguous segments, ordered by rank) is a contract; violating it produces
  correctly-shaped but wrongly-arranged tensors.
- "Collectives can be launched from different ranks on different streams." Every rank
  must call the same collective on the same NCCL communicator in the same order;
  diverging order or stream desync deadlocks or corrupts.

## How to reason correctly

1. State the collective's contract exactly: input buffer, output buffer, count,
   datatype, in-place (same buffer) vs out-of-place, and the output layout.
2. Choose the algorithm by message size and node count: ring for large messages
   (bandwidth-bound), tree/double-binary-tree for small messages with many nodes
   (latency-bound). NCCL's own heuristic does this — do not override without reason.
3. For ring all-reduce, trace chunk-by-chunk: chunk `c` of rank `r` is sent to
   `(r+1) % N` each step; after reduce-scatter each rank holds chunk
   `(r - step) % N` summed, then all-gather rotates partials. Verify the rotation
   with a small N (3 or 4) by hand before trusting the code.
4. For p2p, enumerate the topology: which pairs can reach each other directly, what
   the PCIe/NVLink hierarchy is, whether peer access is enabled on both sides.
5. Enforce ordering: all ranks call the same sequence of collectives on the same
   communicator; NCCL is not thread-safe per communicator, and mixing streams
   without a common event dependency is a data race on the collective's ordering.
6. Check the return value of every NCCL call and every `cudaDeviceEnablePeerAccess`
   /`cudaMemcpyPeer` call; a silent error leaves the buffer partially written.

## What to verify

- For each collective: datatype, count, in/out layout, stream, result checked.
- Ring rotation math correct for N=3..8 (not just N=4 or N=power-of-two).
- Every rank participates; no rank skips a call or reorders calls.
- p2p: peer access enabled both directions before the copy; error checked.
- Expert-parallel: total tokens conserved (sum of received == sum of sent).
- Multiple runs: outputs identical where determinism is the contract.

## How to verify

```
# Multi-GPU host (documented target; no GPU here):
nvcc -arch=sm_80 -O2 -lnccl -o ring_ar ring_allreduce.cu
mpirun -np 4 ./ring_ar          # or launch with a job script; NCCL + multi-GPU
# checks: result == fp64 reference for N ranks, incl. non-power-of-two N

# Self-contained Python model of the ring all-reduce rotation (runs with
# python 3.11, no GPU): verifies chunk rotation + reduce-scatter + all-gather
# arithmetic for N=3,4,5,8 ranks.
python examples/good/sim_ring_allreduce.py
```

Toolchain status: no GPU, no NCCL on this machine. The `.cu` examples are
documentary (researched — toolchain not available; command: `nvcc -lnccl`).
The Python ring-rotation simulation was actually run; output in `evals/README.md`.

## Where the knowledge comes from

- `nccl-docs` — collective ops, communicators, streams, ring/tree/double-binary-tree
  algorithms, peer-to-peer.
- `arxiv-2608-04450` — CommBench: generated collectives are mostly wrong; best model
  only 30.7% correct + competitive.
- `cuda-cpp-guide` — `cudaMemcpyPeer`, peer access, UVA, streams.

## Related skills

- `gpu-kernel-verification-beyond-oracle` — same value-verification discipline
  (fp64 reference, shape sweep) applied to collective outputs.
- `hpc-mpi-programming` — MPI analog of ring/tree collectives; shared terminology.
- `hpc-rdma-verbs` — what NCCL sits on top of (NVLink/PCIe vs IB verbs).
- `gpu-memory-model-coherence` — ordering guarantees collectives rely on.

## Evaluation

Synthetic: a ring all-reduce with wrong chunk rotation (`bad/bad_ring_rotation.cu`)
and an all-gather with misordered output segments (`bad/bad_allgather_layout.cu`)
must be caught by the rotation/layout reasoning and the simulation.
False-positive: correct ring all-reduce, correct NCCL stream usage, correct
in-place all-reduce, and correct expert-parallel token counts must NOT be flagged.
Adversarial: the rotation bug passes a single-rank run and N=4 (power of two) but
fails N=3 or N=6 — the agent must test non-power-of-two rank counts.
Historical: CommBench failure classes (`arxiv-2608-04450`) used as negative cases.
Commands and recorded results: `evals/README.md`.
