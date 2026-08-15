# Evaluation — riscv-isa-and-rvv-intrinsics

Skill: `skills/riscv/riscv-isa-and-rvv-intrinsics`.
Stability: `researched` (source-backed grounding: riscv-v-spec, riscv-rvv-intrinsics,
riscv-isa-spec). clang (no RVV cross) and qemu are NOT installed on this machine;
the `.c` examples are documentary with target commands recorded. The vsetvl VL math
and strip-mining coverage were verified with a self-contained Python 3.11 model
(`examples/good/sim_vsetvl.py`), actually run; output recorded below. Mark:
SIMULATED — models VL/VLMAX arithmetic and loop coverage, not the hardware pipeline.

## Toolchain status

`clang --target=riscv64-unknown-elf -march=rv64gcv` and `qemu-riscv64`: NOT
available. Consequences, stated honestly:

- `bad_*.c` and `good_*.c` compile only under a clang RVV cross. Target commands
  recorded in each file. NOT run here.
- The Python model verifies (a) VLMAX = LMUL·VLEN/SEW, (b) VL = min(AVL, VLMAX),
  (c) fractional-LMUL legality per spec §3.4.3, (d) strip-mining loop coverage for
  VLEN 128/256/512 across n=0..1000. It does not model pipeline behavior,
  reductions, or `vill` trap handling beyond legality.

Target commands to promote to `verified` (clang + qemu):

```
clang --target=riscv64-unknown-elf -march=rv64gcv -O2 -S examples/good/good_strip_mining.c
qemu-riscv64 -cpu rv64,v=true,vlen=128 -L /opt/riscv/sysroot ./good_strip_mining
qemu-riscv64 -cpu rv64,v=true,vlen=512 ./good_strip_mining   # VL-independence check
```

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/bad_hardcoded_vl.c` | hard-coded VL=8 breaks on VLEN=256 (VLMAX=4) | review-time flag; toolchain absent |
| medium/negative | `bad/bad_lmul_cap.c` | SEW=64 with LMUL=mf2 exceeds mf2·ELEN=32 → vill/VL=0 | review-time flag; toolchain absent |
| medium/negative | `bad/bad_tail_policy.c` | reduction reads agnostic tail lanes | review-time flag |
| easy/negative | `bad/bad_byte_stride.c` | vlse stride is bytes, not elements | review-time flag |
| positive | `good/good_strip_mining.c` | VL recomputed per iteration; VLEN-independent | simulated (loop coverage) |
| positive | `good/good_strided.c` | byte stride = k·sizeof(float); segmented nfields | toolchain absent |
| positive | `good/good_policies.c` | explicit ta/ma or tu/mu per consumer | review-time |

Detection rule: any fixed VL/VLMAX constant, any `vsetvl` return value unused, any
fractional LMUL with SEW above LMUL·ELEN, and any full-register consumer of
agnostic lanes must be flagged.

## False-positive evals (correct code must NOT be flagged)

- `good/good_strip_mining.c` — VL-driven loop, no constant: must NOT be flagged
  for "inefficient" or "missing VLEN check".
- `good/good_strided.c` — byte stride derived from `sizeof(float)`: correct.
- `good/good_policies.c` — masked/tail-aggnostic with consumers reading only `vl`
  lanes: correct; must NOT be "fixed" to tu/mu.
- Legal configs (LMUL≥1 with any SEW≤64) must NOT be flagged via a bogus
  "LMUL·SEW ≤ VLEN" rule (that constraint does not exist for LMUL≥1).

## Historical evals

Not applicable as dedicated category: no CVE is attributed. The failure classes
(hard-coded VL, fractional-LMUL SEW cap, agnostic-tail reads) are documented from
riscv-v-spec §3.4.1/§3.4.3/§6.4. Historical RVV errata are out of scope until a
qemu-riscv64 runner is available.

## Adversarial evals

- A strip-mine loop correct for VLEN=128 but containing a hidden constant
  (e.g. `i += 2` derived from VLMAX=2) must be caught when re-tested at
  VLEN=512 — the review must demand VL-recomputation and a second VLEN run.
- "vill = silent no-op" trap: a config with SEW over the fractional-LMUL cap does
  not fault, it just yields VL=0; the agent must detect the empty-loop result
  rather than "it ran fine".

## Verified facts (python 3.11.9 run, recorded 2026-08-15)

Command: `python examples/good/sim_vsetvl.py`

```
VLMAX table (VLEN=128, SEW_MIN=8, ELEN=64):
  LMUL=mf8  SEW= 8: VLMAX=2
  LMUL=mf8  SEW=32: ILLEGAL
  LMUL=mf8  SEW=64: ILLEGAL
  LMUL=mf4  SEW= 8: VLMAX=4
  LMUL=mf4  SEW=32: ILLEGAL
  LMUL=mf4  SEW=64: ILLEGAL
  LMUL=mf2  SEW= 8: VLMAX=8
  LMUL=mf2  SEW=32: VLMAX=2
  LMUL=mf2  SEW=64: ILLEGAL
  LMUL=m1   SEW= 8: VLMAX=16
  LMUL=m1   SEW=32: VLMAX=4
  LMUL=m1   SEW=64: VLMAX=2
  LMUL=m2   SEW= 8: VLMAX=32
  LMUL=m2   SEW=32: VLMAX=8
  LMUL=m2   SEW=64: VLMAX=4
  LMUL=m4   SEW= 8: VLMAX=64
  LMUL=m4   SEW=32: VLMAX=16
  LMUL=m4   SEW=64: VLMAX=8
  LMUL=m8   SEW= 8: VLMAX=128
  LMUL=m8   SEW=32: VLMAX=32
  LMUL=m8   SEW=64: VLMAX=16

Rule 1 (VL = min(AVL, VLMAX)) for SEW=64, LMUL=1, VLEN=128: VLMAX = 2
  AVL=  0 -> VL=0
  AVL=  1 -> VL=1
  AVL=  2 -> VL=2
  AVL=  5 -> VL=2
  AVL=100 -> VL=2

Strip-mining coverage (vl recomputed per iteration):
  ALL PASS: every n in 0..1000 fully covered for VLEN 128/256/512
```

Interpretation: VLMAX matches `LMUL·VLEN/SEW` for every legal config; VL clamps to
VLMAX; fractional LMULs are ILLEGAL above `LMUL·ELEN` (matching the spec's
support rule — note m8/SEW=64 is legal with VLMAX=16, contradicting the naive
"LMUL·SEW ≤ VLEN" claim). Strip-mining with per-iteration `vl = min(remaining,
VLMAX)` covers every n exactly for three VLENs — the portability property the
skill teaches.

## Scoring (for routing eval)

- recall: hard-coded VL, fractional-LMUL SEW cap, agnostic-tail reads, byte-stride
  errors detected via reference rules.
- precision: correct VL-driven loops and legal configs produce zero flags.
- FP-rate: zero expected on the good set; the main FP risk is flagging m8/SEW=64
  as "illegal" (it is legal) — the reviewer must use the spec constraint, not the
  invented LMUL·SEW≤VLEN rule.

## Target toolchains (absent, documented)

- `clang --target=riscv64-unknown-elf -march=rv64gcv`: RVV cross needed.
- `qemu-riscv64 -cpu rv64,v=true,vlen=<128|512>`: runtime VL-independence check.
- Python 3.11 VL-math model: AVAILABLE, run, recorded above.
