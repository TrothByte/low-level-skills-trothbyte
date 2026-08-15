---
name: riscv-isa-and-rvv-intrinsics
description: Use when writing or reviewing RISC-V code with the vector extension: vsetvl/vsetvli AVL-VL semantics, LMUL/SEW ratios, VLEN dependence, tail and mask policies, strip-mining loops, strided and segmented loads, and RVV C intrinsics. Prevents wrong VL, policy misuse, and portability bugs across VLEN implementations.
---

# RISC-V ISA and RVV Intrinsics

## When to use

- Writing vectorized kernels for RISC-V using `vsetvli`/`vsetvl` and RVV intrinsics
  (`vle64_v`, `vse64_v`, `vsetvl_e64m1`, etc.).
- Reviewing RVV code for VL dependence: a kernel that hard-codes VL=128 breaks on a
  256-bit VLEN part.
- Implementing strip-mining loops, masked loads/stores, or strided/segmented memory
  operations (`vlse`, `vlsseg`, `vlseg`).
- Deciding LMUL/SEW for a given VLEN and data type, and reasoning about fractional
  LMUL SEW caps (SEW ≤ LMUL·ELEN, ELEN=64) and `vill` behavior.
- Porting SIMD code from AVX/NEON to RVV.

## When not to use

- Non-vector RISC-V code — use `asm-risc-v-registers-and-calling-conventions`.
- x86/NEON vector code — different vector model, no explicit VL.
- Writing an OS kernel or PMP/syscall code — use the base ISA skill.
- Performance profiling of the compiled output — this skill is correctness.

## What the agent often gets wrong

- "VL is whatever I set once." No — `vsetvli` returns the actual VL, which is
  `min(AVL, VLMAX)`; VLMAX = LMUL · VLEN / SEW. If the hardware VLEN is smaller
  than assumed, VL < AVL and the loop must re-check VL.
- "`vsetvl` arguments are (sew, lmul)." No — the intrinsic form is
  `vsetvl_e<N>m<M>(AVL)` returning a `size_t` VL, and the assembly form is
  `vsetvli rd, rs1, e<N>, m<M>` with an *encoding-immediate* for the
  sew/lmul/mask-policy fields.
- "LMUL=8 is always allowed." No — LMUL=8 (a group of 8 registers) is fine for
  LMUL≥1 configs, but FRACTIONAL LMULs are capped by SEW: mf2 needs SEW≤32,
  mf4 needs SEW≤16, mf8 needs SEW≤8 (SEW ≤ LMUL·ELEN, ELEN=64). Exceeding the
  cap sets `vill` and `vl=0` — a silent no-op, not a trap.
- "Tail and mask policy don't matter." Wrong — the tail (elements past VL) and the
  masked-off elements are either *undisturbed* (TA=0/MA=0) or *agnostic* (TA=1/
  MA=1). With agnostic, those lanes may be garbage — a reduction reading the full
  register reads garbage.
- "A strip-mining loop is just `for (i = 0; i < n; i += VL)`." No — VL must be
  recomputed from `min(n - i, VLMAX)` every iteration because the last iteration is
  partial.
- "Strided load `vlse` takes a stride in elements." It takes a stride in *bytes*
  (`ssize_t`), and for segmented/strided the segment count matters.
- "Intrinsics are portable regardless of VLEN." The C intrinsics are portable in
  *source* but the value of VL depends on runtime VLEN; a hard-coded VL is a
  portability bug.

## How to reason correctly

1. Start from the data: for each loop, compute `VLMAX = (VLEN/8) * LMUL / SEW_bytes`
   (SEW_bytes = SEW/8). This is the max VL for that config.
2. Every iteration: `vl = vsetvli_e64m1(min(remaining, ...))` — call the intrinsic
   with the *current* remaining count (AVL), not a constant. The returned `vl` is
   authoritative.
3. In assembly, read the returned `vl` from the `rd` of `vsetvli`; do not assume it
   equals AVL.
4. Set the policy bits: use `e<N>m<M>ta,ma` when masked lanes/tail may be garbage
   (reduction must then only read the first `vl` elements); use `tu,mu` when you
   need undisturbed lanes. Choose consciously, not by default.
5. For strided/segmented ops, write the element count (`nfields`/AVL) and byte
   stride; verify the byte stride against the element size.
6. Always compile the same source for two VLEN values (e.g. 128 and 512) and check
   VL recomputation; the code must be correct for both.

## What to verify

- Every `vsetvl`/`vsetvli` return value is used (or provably constant).
- Fractional LMUL SEW cap: SEW ≤ LMUL·ELEN (mf2→32, mf4→16, mf8→8); exceeding
  it sets `vill` and VL=0.
- Tail/mask policy chosen and consistent with what lanes are read afterwards.
- Strip-mining loop handles the partial last iteration (AVL = remaining, not n).
- Strided/segmented byte strides and element counts correct.
- Masked loads/stores: mask bits beyond VL and the masked-out lanes do not leak
  into the result (TA/MA policy).

## How to verify

```
# Target toolchain (documented; no clang/qemu on this machine):
clang --target=riscv64-unknown-elf -march=rv64gcv -O2 -S examples/good/good_strip_mining.c
# and run under qemu-riscv64:
qemu-riscv64 -cpu rv64,v=true,vlen=128 examples/good/good_strip_mining
# repeat with vlen=512 to prove VL independence.
```

Toolchain status: clang (no RVV cross) and qemu are NOT installed. All `.c`
examples are documentary (researched — toolchain not available; command:
`clang --target=riscv64-unknown-elf -march=rv64gcv` + `qemu-riscv64 -cpu
rv64,v=true,vlen=128`). A self-contained Python model of the vsetvl VL math and
strip-mining loop was run; its output is in `evals/README.md`.

## Where the knowledge comes from

- `riscv-v-spec` — vsetvli/vsetvl semantics, VL/VLMAX, AVL rules, policies,
  LMUL/SEW constraints, strided/segmented loads.
- `riscv-rvv-intrinsics` — the C intrinsic functions and type suffixes.
- `riscv-isa-spec` — base ISA, `M`/`C` extensions, addressing.

## Related skills

- `asm-risc-v-registers-and-calling-conventions` — base ABI, `v0-v31` register
  class, VLENCSR.
- `vectorization-reasoning` — high-level vectorization ideas mapped to RVV.
- `simd-vectorization-cross-layer` — cross-layer SIMD reasoning.

## Evaluation

Synthetic: hard-coded VL, fractional-LMUL SEW over the cap, wrong tail policy
(agnostic lanes read),
missing partial-iteration handling, wrong byte stride, incorrect `vsetvl_e*` args —
each must be flagged.
False-positive: correct `vsetvl_e64m1(min(remaining,VLMAX))` loops, `ta,ma` chosen
with only `vl` lanes read, and valid segmented loads must NOT be flagged.
Adversarial: a strip-mine loop correct for VLEN=128 but with a hard-coded inner
assumption must be caught when tested at VLEN=512 — the review must demand
VL-recomputation, not just a single VLEN compile. 
Commands and recorded results: `evals/README.md`.
