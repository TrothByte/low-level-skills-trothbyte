# Evaluation — gpu-communication-primitives

Skill: `skills/gpu/gpu-communication-primitives`.
Stability: `researched` (source-backed grounding: nccl-docs, arxiv-2608-04450,
cuda-cpp-guide). No GPU, no NCCL, no multi-GPU host on this machine; all `.cu`
examples are documentary with target commands recorded. The ring all-reduce
rotation semantics were verified with a self-contained Python 3.11 model
(`examples/good/sim_ring_allreduce.py`), actually run; output recorded below.
Mark: SIMULATED — models collective arithmetic, not NCCL/GPU hardware.

## Toolchain status

`nvcc` + `-lnccl`, multiple GPUs: NOT available. Consequences:

- `bad_ring_rotation.cu`, `bad_allgather_layout.cu`, `good_nccl_allreduce.cu`,
  `good_p2p_peer.cu` compile only on a CUDA + NCCL + multi-GPU host. Target
  commands recorded in each file. NOT run here.
- The Python ring model verifies the *rotation math* (reduce-scatter + all-gather
  chunk indices) for N=3,4,5,8 against a sequential element-wise ground truth. It
  does not model NCCL, RDMA, or GPU scheduling.

Target commands to promote to `verified` (multi-GPU host):

```
nvcc -arch=sm_80 -lnccl -O2 examples/good/good_nccl_allreduce.cu -o good_ar
mpirun -np 4 ./good_ar          # expect: fp64-reference comparison passes
nvcc -arch=sm_80 -lnccl examples/bad/bad_ring_rotation.cu -o bad_ring
mpirun -np 6 ./bad_ring         # expect: chunk index mismatch printed
```

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/bad_ring_rotation.cu` | off-by-one in all-gather chunk index | host arithmetic demo only; toolchain absent |
| medium/negative | `bad/bad_allgather_layout.cu` | output not rank-order contiguous | review-time flag; toolchain absent |
| medium/positive | `good/good_nccl_allreduce.cu` | all-reduce with stream + result checks | toolchain absent |
| positive | `good/good_p2p_peer.cu` | peer access enabled both directions before copy | toolchain absent |

## False-positive evals (correct code must NOT be flagged)

- Correct ring all-reduce with the standard `(rank - step - 1) % N` recv chunk in
  both phases — must NOT be "fixed" into the buggy form.
- `ncclAllReduce` with `sendbuff == recvbuff` (documented in-place) — must NOT be
  flagged.
- All-gather writing `recvbuff + rank*COUNT` (rank-order contiguous) — must NOT be
  "optimized" to a permuted write.
- A p2p program that checks `canAccessPeer` and falls back to staged copies when
  p2p is unavailable — must NOT be flagged as "slow workaround".

## Historical evals

Not applicable as dedicated category: no named CVE is attributed. The failure
classes come from CommBench (`arxiv-2608-04450`): rotation errors, layout errors,
ordering errors, expert-parallel token loss. Historical runs of real NCCL
incidents are out of scope until a multi-GPU host is available.

## Adversarial evals

- The rotation bug passes N=4 (power of two) and a single-rank run — the agent must
  demand N=3 or N=6 (non-power-of-two) testing. The simulation is the oracle for
  this: N=3,4,5,8 all checked.
- "It ran without crashing" (errors swallowed) — the agent must check every
  `ncclResult_t`/`cudaError_t` and treat late-surfacing errors as failures.

## Verified facts (python 3.11.9 run, recorded 2026-08-15)

Command: `python examples/good/sim_ring_allreduce.py`

```
N=3: correct rotation PASS; off-by-one rotation detected
N=4: correct rotation PASS; off-by-one rotation detected
N=5: correct rotation PASS; off-by-one rotation detected
N=8: correct rotation PASS; off-by-one rotation detected
ring all-reduce model: ALL PASS
Model of collective arithmetic and rotation semantics — not GPU hardware. Documented
target: nvcc -arch=sm_80 -lnccl on multi-GPU.
```

Interpretation: the standard ring rotation `recv_chunk = (rank - step - 1) % N` in
both phases reproduces the sequential element-wise sum on every rank for
N=3,4,5,8; the off-by-one variant is detected in every case. This confirms the
rotation rules in `references/collectives-and-p2p.md` (rule 1) and provides the
oracle for the adversarial eval.

## Scoring (for routing eval)

- recall: rotation bug, layout bug, ordering bug, and token-loss patterns detected
  via reasoning + sim.
- precision: correct ring rotation, in-place usage, and rank-order all-gather not
  flagged.
- FP-rate: zero expected on good fixtures.

## Target toolchains (absent, documented)

- `nvcc -arch=sm_80 -lnccl`, multi-GPU host, `mpirun`: NCCL + CUDA machine.
- `nvidia-smi topo -m`: topology inspection for p2p verification.
- Python 3.11 model of rotation semantics: AVAILABLE, run, recorded above.
