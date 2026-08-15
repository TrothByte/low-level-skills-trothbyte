# GPU Kernel Verification — Beyond the Fixed-Shape Oracle

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to `registry/sources.yaml`.

## 1. One fixed shape does not certify a kernel

- **RULE**: kernel correctness is a universal claim over (input, shape, grid, block).
  Testing one shape (or a couple) cannot certify it. The Correctness Illusion paper
  (`arxiv-2606-20128`) shows fixed-shape `allclose` oracles certify kernels that are
  buggy; a fuzz harness + fp64 reference caught 9/9 of those buggy kernels.
- **WHY AI GETS IT WRONG**: treats a single green run as a proof; "it passed the
  oracle" sounds like verification; assumes an LLM-generated kernel is correct unless
  a test fails.
- **CORRECT REASONING**: enumerate the shape space. Bugs concentrate at
  non-multiples of the block size, at boundaries (0, 1, BLOCK-1, BLOCK, BLOCK+1), at
  ragged final blocks, and at extreme sizes. A harness that never visits those shapes
  is structurally incapable of finding grid/remainder bugs.
- **EXAMPLE** (bad):
  ```cuda
  // good/bad_* comparison: harness that only ever runs N=1024, BLOCK=256
  launch(good_fuzz_kernel, 1024); // 1024 is a multiple of 256 -> tail bug hidden
  ```
- **COUNTEREXAMPLE** (good):
  ```cuda
  const int sizes[] = {0,1,2,127,128,255,256,257,511,512,513,1023,1024,1025,1<<20};
  for (int s : sizes) if (!check(s)) return 1;
  ```
- **VERIFICATION**: python `examples/good/sim_oracle_weakness.py` — the fixed-shape
  oracle PASSES the tail-drop kernel while a shape sweep FAILS it.
- **SOURCE**: `arxiv-2606-20128` §"fixed-shape allclose oracle"; `arxiv-2605-16819`
  (near-perfect on seen shapes, drops on unseen).

## 2. The reference must be fp64 (or exact), not fp32

- **RULE**: the reference should be computed in the highest available precision
  (float64, or exact integer/rational), independent of the kernel's accumulation
  order. A float32 reference drifts the same way the kernel drifts and hides bugs.
- **WHY AI GETS IT WRONG**: "reference" is implemented by copying the kernel loop with
  the same types; both sides round identically, so error is invisible.
- **CORRECT REASONING**: a sum of many values in fp32 loses precision; comparing the
  kernel's fp32 result to an fp32 reference measures nothing. Compare against fp64
  with an *explained* tolerance (e.g. `rtol=1e-5*log2(N)` for a sum of N terms), not a
  magic loose value.
- **EXAMPLE** (bad): `float ref = 0; for (...) ref += in[i];` then
  `allclose(out, ref, 1e-5)`.
- **COUNTEREXAMPLE** (good): `double ref = 0; for (...) ref += (double)in[i];`
  compare with a tolerance derived from the reduction depth.
- **VERIFICATION**: same sim file: switching the reference to fp64 exposes failures
  that fp32-vs-fp32 hides (recorded in `evals/README.md`).
- **SOURCE**: `arxiv-2606-20128` (fp64 reference is the fix that caught 9/9);
  `cuda-cpp-guide` §3.2.3 (float types, precision).

## 3. Grid size: ceil, not floor, for the remainder

- **RULE**: to cover N elements with BLOCK threads/block, the grid must be
  `ceil(N/BLOCK)` = `(N + BLOCK - 1) / BLOCK`, and every thread must guard its index
  with `if (i < N)`.
- **WHY AI GETS IT WRONG**: writes `grid = N / BLOCK`; the bug is invisible at
  multiples of BLOCK, so single-shape testing passes and the failure appears only at
  non-multiple sizes — the classic "passes review, segfaults under load"
  (ISO-Bench, `arxiv-2602-19594`).
- **CORRECT REASONING**: N=1025, BLOCK=256 needs 5 blocks; `1025/256 = 4` silently
  leaves element 1024 unwritten (or written garbage). Guard every indexed access and
  size the grid to cover the tail.
- **EXAMPLE** (bad):
  ```cuda
  int grid = N / 256;            // floor — drops the last partial block
  kernel<<<grid, 256>>>(d_in, d_out, N);
  ```
- **COUNTEREXAMPLE** (good):
  ```cuda
  int grid = (N + 255) / 256;    // ceil
  kernel<<<grid, 256>>>(d_in, d_out, N);
  // inside kernel: __global__ void k(const float*in,float*out,int N){
  //   int i = blockIdx.x*256 + threadIdx.x; if (i < N) out[i] = in[i]*2; }
  ```
- **VERIFICATION**: sim records the tail element as unwritten (stays 0) for N=1025;
  shape sweep reports it. `compute-sanitizer --tool memcheck` catches the OOB write.
- **SOURCE**: `cuda-cpp-guide` §2.2 (thread hierarchy), §3.2.1 (indexing); ISO-Bench
  failure class `arxiv-2602-19594`.

## 4. Edge and adversarial shapes must be explicit

- **RULE**: always include 0, 1, 2, BLOCK-1, BLOCK, BLOCK+1, 2*BLOCK-1, 2*BLOCK+1,
  and at least one very large size (e.g. 1<<20) in the harness.
- **WHY AI GETS IT WRONG**: tests 64, 1024, 4096 — all smooth multiples — and never
  sees the off-by-one at the ragged boundary; believes "large" replaces "edge".
- **CORRECT REASONING**: index bugs are boundary bugs. The adversarial claim is that
  unseen shapes (AgentKernelArena, `arxiv-2605-16819`) break kernels that scored well
  on seen shapes, so the harness must *seek* unseen shapes, not reuse the training set.
- **EXAMPLE** (bad): a harness of only powers of two.
- **COUNTEREXAMPLE** (good): the `sizes[]` array in rule 1 plus randomized
  non-multiples; each with the fp64 reference.
- **VERIFICATION**: run the sim with the shape sweep; note which shapes fail.
- **SOURCE**: `arxiv-2605-16819` (unseen-shape generalization); `cuda-cpp-guide`
  §2.2.

## 5. Empty input and zero-work launches are defined behavior

- **RULE**: N=0 must be handled (either no launch, or a kernel that writes nothing
  and returns a defined value); reductions over empty input need an agreed identity.
- **WHY AI GETS IT WRONG**: assumes N≥1 everywhere; a 0-size launch has undefined grid
  size and can fault or corrupt, yet is never tested.
- **CORRECT REASONING**: if N==0, skip the launch or return identity; document the
  contract. Same for N < BLOCK: a single block must handle it (guard inside).
- **EXAMPLE** (bad): `kernel<<<N/256, 256>>>(..., 0);` — grid size 0, invalid launch.
- **COUNTEREXAMPLE** (good): `if (N == 0) return;` before the launch; in-kernel
  `if (i < N)` guard.
- **VERIFICATION**: sim checks N=0 path returns defined output.
- **SOURCE**: `cuda-cpp-guide` §3.2.1 (grid config); ISO-Bench shape-class bugs.

## 6. Nondeterminism is a bug unless contracted

- **RULE**: if a kernel writes a deterministic function of its inputs, repeated
  launches must produce identical bytes. If it uses `atomicAdd`/race ordering, that
  nondeterminism must be *documented* and the oracle must compare with tolerance.
- **WHY AI GETS IT WRONG**: a single pass is assumed deterministic; or a genuinely
  racy kernel "passed" once and the harness is rerun until green.
- **CORRECT REASONING**: run each shape k times; diff outputs. Non-reproducible output
  is either a race (fix + `compute-sanitizer --tool racecheck`) or an atomic-sum order
  (document; tighten tolerance or sort).
- **EXAMPLE** (bad): one run, one shape, green.
- **COUNTEREXAMPLE** (good): `for (rep in 0..2) if (run(shape) != out0) return 1;`
- **VERIFICATION**: sim reruns and compares byte-exact.
- **SOURCE**: `cuda-cpp-guide` §7.14 (atomics), §7.7 (races); ISO-Bench race class.

## 7. Crash checking is separate from oracle checking

- **RULE**: an oracle compares values; it does not detect out-of-bounds writes that
  happen to leave the compared buffer intact. Run `compute-sanitizer --tool memcheck`
  (or HIP equivalent) on the fuzz corpus in addition to the value oracle.
- **WHY AI GETS IT WRONG**: "the output was right" is read as "no memory error";
  OOB writes into padding or a don't-care region pass `allclose` while corrupting the
  heap.
- **CORRECT REASONING**: two independent verdicts: (a) value equality vs fp64
  reference across the shape sweep; (b) sanitizer-clean memory behavior. Both must
  pass.
- **EXAMPLE** (bad): `out[i+1] = ...` at the last index — `allclose` on the first N
  elements passes.
- **COUNTEREXAMPLE** (good): memcheck on the same launch reports the OOB write.
- **VERIFICATION**: `compute-sanitizer --tool memcheck ./fuzz_kernel` (documented;
  nvcc not available here).
- **SOURCE**: `cuda-cpp-guide` §2.2; ISO-Bench "segfault under load" class.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Oracle | fixed-shape allclose certifies nothing; fuzz + fp64 reference caught 9/9 |
| Reference | fp64 or exact, independent path; tolerance explained, not magic |
| Grid | `ceil(N/BLOCK)`; guard `i < N`; floor-grid drops the tail |
| Shapes | 0,1,2,BLOCK±1,2*BLOCK±1, and 1<<20; plus randomized non-multiples |
| Empty | N=0 defined (skip launch or identity); N<BLOCK in one block |
| Determinism | repeat launches must match, or the race must be documented |
| Memory | value oracle ≠ memcheck; run compute-sanitizer on the corpus |
