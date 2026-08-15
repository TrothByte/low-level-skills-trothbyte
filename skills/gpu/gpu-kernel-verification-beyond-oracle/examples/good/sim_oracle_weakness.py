#!/usr/bin/env python3
"""Simulates the fixed-shape allclose oracle weakness (Correctness Illusion).

A "GPU-like" kernel is modelled as a pure function: out[i] = 2*in[i], but with
the classic floor-grid bug: the grid is N//BLOCK instead of ceil(N/BLOCK), so
the last partial block never runs and its elements stay 0.

Pipeline being tested:
  1. naive oracle: run once at a single fixed shape (1024), compare allclose.
  2. fuzz oracle: run across many shapes (incl. non-multiples of BLOCK),
     compare against an fp64 reference.

The simulation is self-contained and deterministic; it models kernel semantics,
not GPU hardware timing. nvcc is not available on this machine, so the real
kernel cannot be compiled here (documented target command: nvcc -arch=sm_80).
"""

BLOCK = 256


def floor_grid_kernel(inp):
    """BUGGY kernel: grid = N//BLOCK (floor). Tail block is dropped."""
    n = len(inp)
    out = [0.0] * n
    grid = n // BLOCK
    for b in range(grid):
        for t in range(BLOCK):
            i = b * BLOCK + t
            if i < n:
                out[i] = inp[i] * 2.0
    return out


def ceil_grid_kernel(inp):
    """FIXED kernel: grid = ceil(N/BLOCK), with an in-kernel guard i < N."""
    n = len(inp)
    out = [0.0] * n
    grid = (n + BLOCK - 1) // BLOCK
    for b in range(grid):
        for t in range(BLOCK):
            i = b * BLOCK + t
            if i < n:
                out[i] = inp[i] * 2.0
    return out


def fp64_reference(inp):
    return [2.0 * x for x in inp]


def allclose(a, b, rtol=1e-5, atol=1e-8):
    if len(a) != len(b):
        return False
    for x, y in zip(a, b):
        if not (abs(x - y) <= atol + rtol * abs(y)):
            return False
    return True


def main():
    # --- 1. Naive fixed-shape oracle (the trap) --------------------------------
    fixed_n = 1024
    data = [float(i) for i in range(fixed_n)]
    passed_naive = allclose(floor_grid_kernel(data), fp64_reference(data))
    print(f"naive oracle  @ N={fixed_n} (multiple of {BLOCK}): "
          f"{'PASS' if passed_naive else 'FAIL'}"
          "  <-- certifies the buggy kernel")

    # --- 2. Fuzz oracle: many shapes vs fp64 reference -------------------------
    shapes = [0, 1, 2, 127, 128, 255, 256, 257, 511, 512, 513,
              1023, 1024, 1025, 4095, 4096, 1 << 20]
    bad_floor = []
    bad_ceil = []
    for n in shapes:
        data = [float(i % 97) for i in range(n)]
        bf = allclose(floor_grid_kernel(data), fp64_reference(data))
        bc = allclose(ceil_grid_kernel(data), fp64_reference(data))
        if not bf:
            bad_floor.append(n)
        if not bc:
            bad_ceil.append(n)

    print(f"fuzz oracle   floor-grid kernel: FAIL on {len(bad_floor)}/{len(shapes)}"
          f" shapes: {bad_floor}")
    print(f"fuzz oracle   ceil-grid  kernel: FAIL on {len(bad_ceil)}/{len(shapes)}"
          f" shapes: {bad_ceil}")

    # --- 3. Empty input handling ----------------------------------------------
    empty = allclose(floor_grid_kernel([]), fp64_reference([]))
    print(f"empty-input   floor-grid kernel: out = "
          f"{'[]' if not floor_grid_kernel([]) else '??'} (defined as []) -> "
          f"{'PASS' if empty else 'FAIL'}")
    print(f"empty-input   ceil-grid  kernel: out = "
          f"{'[]' if not ceil_grid_kernel([]) else '??'} (defined as []) -> "
          f"{'PASS' if empty else 'FAIL'}")

    verdict = (passed_naive and not bad_floor and not bad_ceil)
    print("\nSummary: the naive single-shape oracle PASSED a buggy kernel that the")
    print("fuzz oracle FAILS on every non-multiple-of-BLOCK shape. Only the")
    print(f"shape sweep + fp64 reference is a real correctness check (all-clear = "
          f"{verdict}).")


if __name__ == "__main__":
    main()
