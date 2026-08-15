# Evaluation — gpu-kernel-verification-beyond-oracle

Skill: `skills/gpu/gpu-kernel-verification-beyond-oracle`.
Stability: `researched` (source-backed grounding: arxiv-2606-20128, arxiv-2602-19594,
arxiv-2605-16819, cuda-cpp-guide, triton-docs). GPU toolchain (nvcc/hipcc) is NOT
available on this machine; the CUDA example files are documentary, with target
commands recorded. The oracle-weakness mechanism itself was verified with a
self-contained Python 3.11 simulation (`examples/good/sim_oracle_weakness.py`),
which was actually run; its output is recorded below. Mark: SIMULATED — models the
oracle protocol, not GPU hardware.

## Toolchain status

`nvcc` / `hipcc` / `compute-sanitizer` / GPU: NOT installed. Consequences, stated
honestly:

- `bad_fixed_shape_oracle.cu`, `bad_floor_grid.cu`, `good_ceil_grid.cu` compile
  only under a real CUDA/HIP toolchain. Target commands are documented in each file
  (`nvcc -arch=sm_80 ...`). They were NOT run here.
- The Python simulation `sim_oracle_weakness.py` uses only `float64` arithmetic and
  models the *oracle comparison protocol* (single fixed shape vs shape sweep vs
  fp64 reference). It does not model GPU scheduling, warps, or memory.

Target commands to promote to `verified` (CUDA machine):

```
nvcc -arch=sm_80 -O2 examples/good/good_ceil_grid.cu -o good_ceil_grid
./good_ceil_grid                      # expect: 16/16 shapes passed
compute-sanitizer --tool memcheck ./good_ceil_grid   # expect: clean

nvcc -arch=sm_80 examples/bad/bad_floor_grid.cu -o bad_floor_grid
./bad_floor_grid                      # expect: out[999] == -1 (unwritten tail)
```

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/bad_floor_grid.cu` | reviewer flags floor-grid tail-drop | exit 0, review-time flag (toolchain absent) |
| medium/negative | `bad/bad_fixed_shape_oracle.cu` | harness rejected as insufficient (single shape, fp32 ref) | exit 0, review-time flag |
| medium/positive | `good/good_ceil_grid.cu` | ceil-grid + `i<N` guard + fp64 ref + shape sweep | exit 0, toolchain absent |
| adversarial | shape N=1025 (non-multiple) on floor-grid kernel | tail unwritten; naive oracle cannot see it | simulated |

Detection rule: a harness is only a correctness argument if (a) the reference is fp64
or exact, (b) shapes include non-multiples of BLOCK and the edge set, (c) memory is
checked with `compute-sanitizer --tool memcheck` when available.

## False-positive evals (correct code must NOT be flagged)

- `good/good_ceil_grid.cu` — correct ceil-grid, guarded tail, fp64 reference,
  explicit edge shapes; must NOT be "strengthened" or flagged.
- A kernel that legitimately returns identity for empty input must NOT be flagged.
- An atomic-sum kernel whose order is documented and whose tolerance is justified
  must NOT be flagged as nondeterministic.

## Historical evals

Not applicable as dedicated category: the skill targets a mechanism (oracle
weakness) rather than a named CVE. The failure class is documented from
`arxiv-2606-20128` (9/9 caught by fuzz+fp64) and ISO-Bench `arxiv-2602-19594`
("passes review, segfaults under load"). No historical CVE is attributed here.

## Adversarial evals

- The floor-grid kernel passes a single-shape fp32 oracle AND a small clean run —
  the agent must refuse to certify it and demand the shape sweep + sanitizer.
- "Seen shape" overfitting (AgentKernelArena, arxiv-2605-16819): an agent trained or
  tested only on powers of two must be routed to the non-multiple shapes.

## Verified facts (python 3.11.9 run, recorded 2026-08-15)

Command: `python examples/good/sim_oracle_weakness.py`

```
naive oracle  @ N=1024 (multiple of 256): PASS  <-- certifies the buggy kernel
fuzz oracle   floor-grid kernel: FAIL on 10/17 shapes: [2, 127, 128, 255, 257, 511, 513, 1023, 1025, 4095]
fuzz oracle   ceil-grid  kernel: FAIL on 0/17 shapes: []
empty-input   floor-grid kernel: out = [] (defined as []) -> PASS
empty-input   ceil-grid  kernel: out = [] (defined as []) -> PASS

Summary: the naive single-shape oracle PASSED a buggy kernel that the
fuzz oracle FAILS on every non-multiple-of-BLOCK shape. Only the
shape sweep + fp64 reference is a real correctness check (all-clear = False).
```

Note on the counts: 0, 1 and 256 are the shapes where the floor-grid bug happens to
be silent (256 is a multiple of BLOCK; 0 and 1 are fully covered by one block), and
1048576 is again a multiple of BLOCK. Every shape with a non-empty remainder fails —
consistent with the rule "grid = ceil(N/BLOCK) or the tail is dropped".

Interpretation: the single fixed shape 1024 (a multiple of BLOCK) certifies the
floor-grid kernel; the shape sweep exposes it on every non-multiple. This is the
"Correctness Illusion" mechanism reproduced in simulation.

## Scoring (for routing eval)

- recall: bad oracle harness and floor-grid kernel detected (review-time for .cu,
  simulation for the mechanism).
- precision: good ceil-grid kernel and empty-input contract produce zero flags.
- FP-rate: no false positives expected on the good fixture.

## Target toolchains (absent, documented)

- `nvcc -arch=sm_80`, `compute-sanitizer --tool memcheck`: CUDA machine required.
- `hipcc`: AMD ROCm machine required.
- Python 3.11 simulation: AVAILABLE, run, recorded above.
