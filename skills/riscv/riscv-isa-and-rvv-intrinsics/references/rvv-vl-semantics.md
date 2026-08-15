# RISC-V RVV: VL/AVL, LMUL/SEW, Policies, Memory Ops — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to `registry/sources.yaml`.

## 1. vsetvli returns the actual VL = min(AVL, VLMAX)

- **RULE**: `vsetvli rd, rs1, e<N>m<M>` (and intrinsic `vsetvl_e<N>m<M>(AVL)`)
  sets the new VL to `min(AVL, VLMAX)` where `VLMAX = LMUL * VLEN / SEW`. The
  returned value in `rd` (or the `size_t` return of the intrinsic) is the
  authoritative VL and must be used by the loop.
- **WHY AI GETS IT WRONG**: assumes VL equals the requested AVL even when AVL >
  VLMAX; or assumes the intrinsic "returns void"; or forgets that VLEN is
  implementation-defined (the SAME binary runs on VLEN=128 and VLEN=512 parts).
- **CORRECT REASONING**: VL is always `min(AVL, VLMAX)`. With AVL ≥ VLMAX you get
  a full vector; with a partial tail you get `n - i`. If the code ignores the
  returned VL it over-reads or under-processes on some VLEN. VLMAX itself depends
  on runtime VLEN — never fold a VLEN assumption into a constant.
- **EXAMPLE** (bad):
  ```c
  vint64m2_t v = vle64_v_i64m2(p, 128);      /* hard-coded VL — wrong on VLEN=256 */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  size_t vl = vsetvl_e64m2(n);               /* vl = min(n, VLMAX) */
  vint64m2_t v = vle64_v_i64m2(p, vl);
  ```
- **VERIFICATION**: python `examples/good/sim_vsetvl.py` prints `VLMAX(128)`,
  `VLMAX(512)` for each LMUL/SEW and confirms `vl = min(AVL, VLMAX)`.
- **SOURCE**: `riscv-v-spec` §3.4.2 (vsetvli/vsetvl), §3.4.1 (AVL/VL/VLMAX);
  `riscv-rvv-intrinsics` (vsetvl intrinsics).

## 2. Fractional LMUL constraint: SEW in [SEW_MIN, LMUL·ELEN]

- **RULE**: fractional LMUL (mf2/mf4/mf8) is legal only for
  `SEW_MIN ≤ SEW ≤ LMUL·ELEN` (SEW_MIN=8, ELEN=64 in the standard extensions):
  mf2 → SEW ≤ 32; mf4 → SEW ≤ 16; mf8 → SEW ≤ 8. For LMUL ≥ 1 there is NO
  `LMUL·SEW ≤ VLEN` restriction — VLMAX = LMUL·VLEN/SEW holds for every standard
  SEW (VLEN ≥ ELEN ≥ SEW is guaranteed by the implementation parameters).
- **WHY AI GETS IT WRONG**: invents a "LMUL·SEW ≤ VLEN" cap (there is none for
  LMUL≥1); or assumes fractional LMUL works with any SEW. Both errors produce
  `vill` (illegal vtype) or a VLMAX the agent didn't predict.
- **CORRECT REASONING**: the only width constraint is `SEW ≤ ELEN` (LMUL≥1) and
  the fractional rule above. Unsupported vtype encodings set `vill`, zero the
  remaining vtype bits AND set `vl=0` — a config that looks valid but yields
  VL=0 and a no-op loop, not an error trap.
- **EXAMPLE** (bad): `vsetvl_e32mf2` is legal (32 ≤ 0.5·64=32);
  `vsetvl_e64mf2` sets `vill`/VL=0 because 64 > 32.
- **COUNTEREXAMPLE** (good):
  ```c
  size_t vl = vsetvl_e32mf2(n);   // VLMAX = VLEN/(SEW/LMUL) = VLEN/64
  ```
- **VERIFICATION**: sim prints legal/illegal for all (LMUL, SEW) on
  VLEN 128/256/512 with SEW_MIN=8, ELEN=64.
- **SOURCE**: `riscv-v-spec` §3.3.1 (constant parameters), §3.4.3 (fractional
  LMUL support), §6.4 (vill).

## 3. Tail and mask policies: ta/ma, tu/mu

- **RULE**: the vector configuration selects tail and mask policies:
  `ta` = tail agnostic (bits past VL may be garbage), `tu` = tail undisturbed
  (bits past VL retain prior values), `ma` = masked-off agnostic, `mu` =
  masked-off undisturbed. Default mnemonics without policy suffix mean
  agnostic/agnostic in many tools (VEXTRACTV defaults) — be explicit.
- **WHY AI GETS IT WRONG**: assumes masked-off or tail lanes are zero; a
  subsequent reduction that sums the FULL register (e.g. `vredsum` with a
  pre-loaded full register) sums garbage; or uses `tu` when it wanted `ta` and
  leaks stale data.
- **CORRECT REASONING**: with `ta,ma`, tail and masked-off lanes are
  implementation-defined (may be any value). If the result feeds a reduction over
  the whole register, the masked/tail lanes must be excluded from the sum (mask the
  reduction) or the policy must be `tu` + pre-initialized zeros. Choose the policy
  by what the consumer reads.
- **EXAMPLE** (bad):
  ```c
  vint64m2_t acc = vmv_v_x_i64m2(0, vl);
  // masked accumulate reading masked-off lanes which are agnostic (garbage):
  acc = vadd_vv_i64m2(acc, masked_add(...), ...);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  vuint64m2_t s = vmv_v_x_u64m2(0, vl);
  s = vredsum_vs_u64m2_u64m1(s, v, s, vl);   /* reduction over vl lanes only */
  ```
- **VERIFICATION**: sim prints policy semantics; compile-time review of the
  consumer of tail/masked lanes.
- **SOURCE**: `riscv-v-spec` §3.7 (policies), §5.4 (reductions).

## 4. Strip-mining: recompute VL every iteration from remaining

- **RULE**: a strip-mining loop must set `vl = vsetvl_e<M>(min(remaining, ...))`
  each iteration, because only the last iteration is partial. Writing
  `for (i = 0; i < n; i += VLMAX)` with a constant VLMAX is wrong when
  `n % VLMAX != 0`.
- **WHY AI GETS IT WRONG**: hoists a single `vsetvli` out of the loop; or uses
  `n / VLMAX` full iterations plus a separate partial (duplicating the body);
  or computes `VLMAX` from an assumed VLEN.
- **CORRECT REASONING**: VLMAX is the *capacity*; the loop's per-iteration VL is
  `min(n - i, VLMAX)`. Set AVL = remaining count inside the loop; the returned VL
  drives the load and the index update `i += vl`. This is correct for every VLEN
  with no constant anywhere.
- **EXAMPLE** (bad):
  ```c
  for (size_t i = 0; i < n; i += 8) {      /* hard-coded 8 */
      vint64m1_t v = vle64_v_i64m1(p + i, 8);
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  for (size_t i = 0; i < n; ) {
      size_t vl = vsetvl_e64m1(n - i);
      vle64_v_i64m1(p + i, vl);            /* then consume vl elements */
      i += vl;
  }
  ```
- **VERIFICATION**: sim executes the strip-mining loop over n=0..1000 for
  VLEN=128/256/512 and asserts total coverage == n with no over/under-read.
- **SOURCE**: `riscv-v-spec` §3.7.1 (example), §3.4.1 (AVL).

## 5. Strided/segmented loads: byte stride and nfields

- **RULE**: `vlse<eew>` (stride load) takes a byte stride (`ssize_t`);
  segmented variants `vlssegN`/`vlsseg` take `N` fields per element; the AVL/VL is
  the number of *elements* (each of N fields). `vlsegN`/`vlseg` are contiguous
  segmented (stride = N fields × EEW).
- **WHY AI GETS IT WRONG**: passes an element-count stride (forgetting × EEW/8);
  treats the segment count as part of the stride; uses element stride where byte
  stride is required.
- **CORRECT REASONING**: `vlse.v v, (rs1), rs2` loads from address
  `base + k*stride` (stride in BYTES) for k in 0..VL-1. For `vlsseg2`, element k
  loads two fields at `base + k*stride` and `base + k*stride + EEW/8`. A vector of
  `float` (EEW=32) with stride 4 bytes is `vlse` with stride 4, not 1.
- **EXAMPLE** (bad): `__riscv_vlse32_v_f32m1(p, 1, vl);` — stride 1 byte for a
  4-byte element.
- **COUNTEREXAMPLE** (good): `__riscv_vlse32_v_f32m1(p, sizeof(float), vl);`
- **VERIFICATION**: sim checks the address arithmetic `base + k*stride` for the
  good/bad strides.
- **SOURCE**: `riscv-v-spec` §7.8 (unit-stride/stride/segmented loads).

## 6. Fractional LMUL (mf2, mf4, mf8) semantics

- **RULE**: fractional LMULs give VLMAX = (VLEN/SEW) × LMUL; e.g. LMUL=mf2 on
  VLEN=128, SEW=32 → VLMAX = 2. Legality: `SEW_MIN ≤ SEW ≤ LMUL·ELEN`
  (SEW_MIN=8, ELEN=64); the minimum supported fractional LMUL is SEW_MIN/ELEN
  = 1/8. Unsupported combos set `vill` and `vl=0` instead of trapping.
- **WHY AI GETS IT WRONG**: treats mf2 as "half the vector" in element count but
  uses it with SEW=64 (needs SEW ≤ 32); or forgets fractional LMUL is a
  *widening* tool and its SEW range is narrower than for LMUL≥1.
- **CORRECT REASONING**: fractional LMUL is for mixed-width operations
  (e.g. SEW doubles); the SEW range for a fractional LMUL is capped by
  `LMUL·ELEN`. `VLMAX = LMUL·VLEN/SEW` still holds. If SEW exceeds the cap the
  config becomes `vill`, and `vl` reads 0 — a silent no-op, not a trap.
- **EXAMPLE** (bad): SEW=64 with LMUL=mf2 on ELEN=64 → 64 > 32, vill/VL=0.
- **COUNTEREXAMPLE** (good): SEW=32 with LMUL=mf2 → 32 = LMUL·ELEN, legal;
  VLMAX = VLEN/64.
- **VERIFICATION**: sim computes legal configs for all (LMUL, SEW) pairs.
- **SOURCE**: `riscv-v-spec` §3.4.2 (fractional LMUL), §3.4.3, §6.4 (vill).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| VL | always `min(AVL, VLMAX)`; use the vsetvl return value |
| VLMAX | `LMUL * VLEN / SEW`; VLEN is runtime |
| LMUL | LMUL≥1: any SEW≤ELEN legal; fractional: SEW ∈ [8, LMUL·64] |
| Policy | ta/ma = agnostic lanes; tu/mu = undisturbed; pick per consumer |
| Strip-mine | `vl = vsetvl(remaining)` inside the loop; `i += vl` |
| Stride | `vlse`/`vlsseg` stride is in BYTES |
| Segments | nfields per element; AVL = element count |
| vill | unsupported vtype → vill set + vl=0 (silent no-op, not a trap) |
