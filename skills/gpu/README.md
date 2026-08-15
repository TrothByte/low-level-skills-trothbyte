# gpu — Skills

GPUs have a memory model of their own. These skills cover GPU coherence scopes, the PTX assembly language, and why CPU reasoning fails on the GPU.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `gpu-communication-primitives` | Use when designing or reviewing multi-GPU kernels that exchange data: point-to-point peer transfers, ring and tree all-reduce/all-gather collectives, NCCL usage and topology choices, expert-parallel sharding. Prevents deadlocks, wrong chunking, and non-deterministic or unsafe collectives that silently corrupt data. | researched | `skills/gpu/gpu-communication-primitives` |
| `gpu-kernel-verification-beyond-oracle` | Use when verifying GPU kernels: escaping the fixed-shape allclose oracle trap, building fuzz harnesses over unseen shapes, comparing against fp64 references, and checking edge sizes that trip grid/block indexing. Prevents kernels that pass review but segfault or corrupt under real loads. | researched | `skills/gpu/gpu-kernel-verification-beyond-oracle` |
| `gpu-memory-model-coherence` | Use when writing, reviewing, or debugging CUDA/HIP kernels where memory coherence matters — shared-memory races without __syncthreads(), relaxed atomics used as synchronization, cross-block visibility without __threadfence(), volatile for inter-thread sync, or host-device ordering. Teaches GPU memory hierarchy, coherence scopes, and correct synchronization. | researched | `skills/gpu/gpu-memory-model-coherence` |
| `ptx-assembly` | Use when writing, reading, or reviewing NVIDIA PTX assembly: state spaces, register types, memory loads/stores with scope and ordering, predication, bar.sync barriers, atomics, and warp-level shfl/vote, or when mapping between CUDA C++ and PTX/SASS. Teaches correct PTX syntax and GPU memory-model semantics. | researched | `skills/gpu/ptx-assembly` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
