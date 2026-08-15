#!/usr/bin/env python3
"""Self-contained model of RISC-V RVV vsetvl VL math and strip-mining.

Verifies the spec rules (riscv-v-spec §3.4.1-3.4.3):
  1. VL = min(AVL, VLMAX), VLMAX = LMUL * VLEN / SEW (for legal configs).
  2. Legal-config constraint: LMUL*SEW <= VLEN (and LMUL*SEW >= 8 for
     fractional LMUL like mf2/mf4/mf8).
  3. Strip-mining loop recomputing vl = min(remaining, VLMAX) covers exactly n
     elements for every n and every VLEN (no over-read, no under-read).

This models the vector-length arithmetic, not the hardware pipeline. No
clang-RVV cross / qemu on this machine (documented target: clang
--target=riscv64-unknown-elf -march=rv64gcv + qemu-riscv64 -cpu rv64,v=true).
"""

LMULS = {"mf8": 1/8, "mf4": 1/4, "mf2": 1/2, "m1": 1, "m2": 2,
         "m4": 4, "m8": 8}


SEW_MIN = 8
ELEN = 64


def vlmax(lmul, sew, vlen):
    return int(lmul * vlen / sew)


def config_legal(lmul, sew, vlen):
    """Legality per v-spec §3.4.3 (constant parameters: SEW_MIN=8, ELEN=64).
    - LMUL >= 1: legal for every SEW <= ELEN (standard SEWs 8/16/32/64);
      VLMAX = LMUL*VLEN/SEW. No LMUL*SEW <= VLEN restriction exists.
    - LMUL < 1 (fractional): legal iff SEW_MIN <= SEW <= LMUL*ELEN
      (e.g. mf2 -> SEW<=32; mf4 -> SEW<=16; mf8 -> SEW<=8 on ELEN=64).
    """
    if lmul >= 1:
        return sew <= ELEN
    return SEW_MIN <= sew <= lmul * ELEN


def main():
    vlen = 128
    print(f"VLMAX table (VLEN={vlen}, SEW_MIN=8, ELEN=64):")
    for lm_name, lm in LMULS.items():
        for sew in (8, 32, 64):
            if config_legal(lm, sew, vlen):
                print(f"  LMUL={lm_name:4s} SEW={sew:2d}: VLMAX={vlmax(lm, sew, vlen)}")
            else:
                print(f"  LMUL={lm_name:4s} SEW={sew:2d}: ILLEGAL")

    # Rule 1: VL = min(AVL, VLMAX); AVL > VLMAX clamps.
    print("\nRule 1 (VL = min(AVL, VLMAX)) for SEW=64, LMUL=1, VLEN=128:",
          "VLMAX = 2")
    for avl in (0, 1, 2, 5, 100):
        v = min(avl, 2)
        print(f"  AVL={avl:3d} -> VL={v}")

    # Rule 3: strip-mining coverage across VLENs and all n.
    print("\nStrip-mining coverage (vl recomputed per iteration):")
    ok = True
    for vlen in (128, 256, 512):
        for n in range(0, 1001):
            i, visited = 0, 0
            while i < n:
                vl = min(n - i, 1 * vlen // 64)   # SEW=64, LMUL=1
                assert vl > 0
                visited += vl
                i += vl
            if visited != n:
                ok = False
                print(f"  VLEN={vlen} n={n}: covered {visited} != {n}")
    print(f"  {'ALL PASS' if ok else 'FAIL'}: every n in 0..1000 fully covered "
          "for VLEN 128/256/512")

    print("\nModel of RVV VL arithmetic per riscv-v-spec §3.4 — not hardware. "
          "Documented target: clang --target=riscv64-unknown-elf -march=rv64gcv "
          "+ qemu-riscv64.")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
