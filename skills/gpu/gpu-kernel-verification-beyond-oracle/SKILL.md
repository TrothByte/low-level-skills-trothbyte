---
name: gpu-kernel-verification-beyond-oracle
description: Use when verifying GPU kernels: escaping the fixed-shape allclose oracle trap, building fuzz harnesses over unseen shapes, comparing against fp64 references, and checking edge sizes that trip grid/block indexing. Prevents kernels that pass review but segfault or corrupt under real loads.
---

# GPU Kernel Verification Beyond the Fixed-Shape Oracle

## When to use

- Checking whether a generated or hand-written kernel is really correct, not just
  "correct on the shape the author tested".
- Reviewing a verification harness: if the harness only calls the kernel at one or
  two fixed shapes with a float32 `allclose`, it is not a correctness proof.
- Writing or reviewing a fuzz harness, an fp64 reference, or edge-size test matrix
  (0, 1, 2, 127/128/255/256/257, non-multiple-of-block, huge sizes).
- Diagnosing kernels that "pass review but segfault under load" (ISO-Bench) or that
  are near-perfect on seen shapes but break on unseen ones (AgentKernelArena).

## When not to use

- CPU numerical code — use host-side property testing; no grid/block indexing exists.
- PTX/SASS-level review — use `ptx-assembly`; this skill is about the oracle protocol,
  not the ISA layer.
- GPU memory-model / synchronization bugs that no oracle shape sweep catches — use
  `gpu-memory-model-coherence` plus `compute-sanitizer`.
- Kernel *performance* tuning — correctness fuzzing is orthogonal to speed.

## What the agent often gets wrong

- "It passed `allclose`, so the kernel is correct." A single fixed shape certifies
  nothing; fuzz + fp64 reference caught 9/9 buggy kernels the fixed-shape oracle
  certified (Correctness Illusion).
- "The reference can be computed in float32 too." Both sides drift identically; a
  float32 reference hides precision bugs that a float64 reference exposes.
- "If it doesn't crash on my test shape, indexing is fine." Grid = `n / BLOCK`
  (floor) instead of `ceil(n / BLOCK)` is invisible at multiples of BLOCK and drops
  the tail block everywhere else.
- "Edge sizes don't matter." Off-by-one, remainder handling and empty-input paths
  live at exactly 0, 1, BLOCK-1, BLOCK, BLOCK+1.
- "Fuzzing is random garbage." Effective fuzzing is shape- and value-directed:
  powers of two, primes, remainders, denormals, INT_MAX/INT_MIN, empty and max.

## How to reason correctly

1. Treat the kernel as a pure function over (input, grid, block, shape). Correctness
   is a universal quantifier over that domain; one point does not prove it.
2. Build the reference in the highest-precision, simplest form (float64 or exact
   integer/analytic), independent of the kernel's internal accumulation order.
3. Fuzz the *shape* space first: any grid/block/remainder bug will appear at
   non-multiple sizes. Then fuzz values inside representative shapes.
4. Sweep the small-and-edge set explicitly: 0, 1, 2, BLOCK-1, BLOCK, BLOCK+1,
   BLOCK*2-1, BLOCK*2+1, and one very large size (check for overflow/truncation).
5. For reductions, also check block/grid counts: 1 block, many blocks, ragged last
   block, and empty input (define the result: 0, identity, or error).
6. Where determinism is the contract, run the kernel several times and diff outputs;
   a nondeterministic result is a bug unless the kernel documents otherwise.
7. If a crash is possible (out-of-bounds writes), run under `compute-sanitizer`
   (`--tool memcheck`) in addition to the oracle; a wrong value and a corrupt heap
   are different failure classes.

## What to verify

- Harness: >10 shapes, including non-multiples of BLOCK and edge sizes.
- Reference: float64 (or exact), not float32; independent code path.
- Comparison: `allclose` with tight, justified rtol/atol — not `rtol=1e-1`.
- Grid/block math: `ceil` used, not floor; remainder handled; no empty-launch UB.
- Reduction: block-level and final block handled; empty input defined.
- Memory: bounds of every `in[...]`/`out[...]` index at the edge shapes; run
  `compute-sanitizer --tool memcheck` when available.
- Determinism: repeat launches agree where the contract requires.

## How to verify

```
# nvcc path (CUDA available):
nvcc -arch=sm_80 -O2 good/good_fuzz_kernel.cu -o fuzz_kernel
./fuzz_kernel            # prints shapes tested and PASS/FAIL count
compute-sanitizer --tool memcheck ./fuzz_kernel   # must be clean

# HIP path:
hipcc -O2 good/good_fuzz_kernel.cu -o fuzz_kernel

# Python simulation of the oracle weakness (runs with plain python 3.11):
python examples/good/sim_oracle_weakness.py
# Expected: fixed-shape oracle PASS; fuzz over shapes FAILS for tail sizes.
```

Toolchain status: this machine has no `nvcc`/`hipcc`/GPU. The `.cu` files below are
documentary (researched — toolchain not available; command: `nvcc -arch=sm_80`).
The Python simulation was actually run and its output recorded in `evals/README.md`.

## Where the knowledge comes from

- `arxiv-2606-20128` — Correctness Illusion: fixed-shape allclose oracle certifies
  buggy kernels; fuzz + fp64 catches 9/9.
- `arxiv-2602-19594` — ISO-Bench: kernels pass review but segfault under load
  (off-by-one / shapes / barriers).
- `arxiv-2605-16819` — AgentKernelArena: near-perfect on seen shapes, drop on unseen.
- `cuda-cpp-guide` — grid/block/thread indexing, execution model, float types.
- `triton-docs` — `tl.constexpr` shapes; Triton kernels hit the same oracle trap.

## Related skills

- `gpu-memory-model-coherence` — cross-thread sync bugs are a separate failure class.
- `ptx-assembly` — review the ISA layer when the oracle passes but behavior is wrong.
- `gpu-communication-primitives` — correctness of multi-GPU collectives, same discipline.
- `performance-measurement-discipline` — do not conflate speed runs with correctness.

## Evaluation

Synthetic: a kernel with a floor-grid tail-drop bug (`bad/bad_tail_grid.cu`) and a
float32-only reference (`bad/bad_fp32_reference.cu`) must be caught by shape fuzz and
fp64 comparison; a single-shape checker (`bad/bad_single_shape_check.py`) must be
rejected as insufficient.
False-positive: a genuinely correct kernel over all tested shapes, verified against an
fp64 reference with justified tolerance, must NOT be "fixed" or flagged.
Adversarial: the tail-drop kernel passes the fixed-shape oracle and a small clean
run — the agent must refuse to certify it without fuzz + sanitizer.
Recorded output: `evals/README.md`.
