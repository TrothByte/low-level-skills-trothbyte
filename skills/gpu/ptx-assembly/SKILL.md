---
name: ptx-assembly
description: Use when writing, reading, or reviewing NVIDIA PTX assembly: state spaces, register types, memory loads/stores with scope and ordering, predication, bar.sync barriers, atomics, and warp-level shfl/vote, or when mapping between CUDA C++ and PTX/SASS. Teaches correct PTX syntax and GPU memory-model semantics.
---

# PTX Assembly & SASS Reasoning

## When to use

- Writing hand-tuned PTX (inline `asm()` in CUDA, or a standalone `.ptx` file).
- Reviewing PTX emitted by nvcc (`nvcc -ptx`) for correctness of memory operations,
  atomics, or barriers.
- Debugging GPU memory-divergence bugs: a kernel "sometimes" reads stale data across
  threads or blocks.
- Reasoning about how CUDA C++ atomics/fences map to PTX, and how PTX maps to SASS.
- Inspecting SASS with `cuobjdump -sass` and explaining why the hardware does something.

## When not to use

- High-level CUDA C++ correctness without ever looking at the assembly.
- AMD/HIP — use `hip-docs`; PTX is NVIDIA-only.
- CPU memory ordering (x86/ARM) — use `memory-ordering-reasoning`.
- Verifying end-to-end kernel behavior on real hardware (that needs a GPU run, not PTX
  review). This skill covers the PTX layer, not the full CUDA runtime model.

## What the agent often gets wrong

- "`ld` without a state space is fine, it defaults to global." NO. `ld`/`st` without a
  state space use Generic Addressing, which requires a generic address (e.g. from
  `cvta.to.global`). A shared or param address is not generic.
- "Plain `ld.global` / `st.global` are atomic and ordered." NO. Default is `.weak`
  (no synchronization, no atomicity). Ordering requires `.acquire`/`.release`/`fence`.
- "Atomics default to `.sys` scope." NO. `atom`/`red` default to `.relaxed` ordering
  and `.gpu` scope. Device-wide flags need an explicit scope.
- "`.b32`/`.b64` is about pointer width." NO. The type qualifier is the DATA width. A
  64-bit pointer is `.u64`; a 32-bit element is `.u32`.
- "`bar.sync` synchronizes anything across blocks." NO. Barriers only synchronize
  threads within a CTA (block). Cross-block coordination needs atomics + acquire/release.
- "`@p` is the same as a hardware flag like x86 `jne`." Predicates are set by `setp` and
  inverted with `@!p`; getting the polarity backwards is a classic bug.
- "`shfl` without `.sync` is fine." Deprecated; `shfl.sync` requires a membermask and is
  UB if the executing thread is not in the mask.

## How to reason correctly

1. Parse every memory instruction as `op.sem.scope.space.type`. State the space (or
   generic), the data type width, and — for cross-thread data — the ordering and scope.
2. For synchronization, draw the release/acquire edge: a `st.release` in one thread and
   an `ld.acquire` (in another thread) that reads it form the only valid hand-off. No
   edge, no happens-before — plain loads/stores are `.weak`.
3. Pick scope by where the peer thread lives: `.cta` same block, `.gpu`/`.sys` across
   blocks. Shared memory defaults to `.shared::cta`.
4. Match data type to the object size, not the pointer size. Pointer registers are
   `.u64` with `.address_size 64`.
5. Warp ops (`shfl.sync`, `vote.sync`) are converged operations: every lane in the
   membermask must execute the same instruction, or the behavior is undefined.
6. Remember PTX is an intermediate ISA: ptxas lowers it to SASS at install time. Read
   SASS (`cuobjdump`) to confirm the final mapping.

## What to verify

- State space present and correct on every `ld`/`st`/`atom` (or an explicit generic
  address via `cvta`).
- Data width of load/store matches the object (not the pointer).
- Any cross-thread flag: release store pairs with acquire load; scope covers the peers.
- Barriers: same barrier id, same count (multiple of warp size), executed by ALL threads
  that can reach them (no divergent `bar.sync`).
- `shfl.sync`/`vote.sync`: membermask includes every executing lane; source lanes active.

## How to verify

```
# 1. Emit PTX from CUDA C++ (if reviewing compiler output):
nvcc -arch=sm_80 -ptx kernel.cu -o kernel.ptx

# 2. Assemble hand-written PTX (catches syntax/type errors):
ptxas -arch=sm_80 examples/good/good_acquire_release_flag.ptx -o out.cubin

# 3. Disassemble SASS to check the final hardware mapping:
cuobjdump -sass out.cubin
```

If a GPU toolchain is unavailable, do a manual parse of every instruction against the
PTX ISA grammar and mark the result `self-reviewed`, not `tool-verified`.

## Where the knowledge comes from

- `ptx-isa` — PTX ISA Specification (syntax, state spaces, memory consistency model,
  scope, fences, atomics, bar, shfl/vote)
- `cuda-cpp-guide` — CUDA C++ Programming Guide (execution model, synchronization)
- `memory-ordering-reasoning` — general ordering reasoning reused at the PTX level

## Related skills

- `gpu-memory-model-coherence` — GPU coherence scope (require of)
- `memory-ordering-reasoning` — release/acquire model, applies to PTX scope semantics
- `c-undefined-behavior` — data races and UB rules that drive correct PTX sync

## Evaluation

Synthetic: cross-CTA flag with weak ops (must flag, fix with acquire/release), missing
`bar.sync` shared-memory exchange, wrong-width load, inverted predicate, `shfl.sync`
without membermask convergence.
False-positive: correct `ld.global.acquire.gpu.b32`/`st.global.release.gpu.b32` pair and
a proper `bar.sync`-guarded exchange must NOT be "strengthened" to `fence.sc` or flagged.
Ambiguous: `fence.acq_rel` vs explicit acquire/release on every op — must mark OPTIONAL,
not a bug. Verification commands target `ptxas -arch=sm_80` and `cuobjdump -sass`.
