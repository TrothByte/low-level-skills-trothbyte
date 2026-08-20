"""Constant-time comparison (XOR-accumulate) — GOOD model.

A secret-dependent equality check must not exit at the first difference:
the position of the first differing byte is a timing signal. The XOR-fold
runs a fixed number of iterations over every byte; the iteration count is
identical regardless of where the first difference occurs.

Run:  python examples/good/ct_compare.py
Expect:  PASS — fixed work for mismatch at first byte vs last byte
"""
import os


def ct_equal(a: bytes, b: bytes) -> tuple:
    """XOR-fold compare: fixed iterations, branch only on the accumulator."""
    acc = 0
    iterations = 0
    for x, y in zip(a, b):
        iterations += 1
        acc |= x ^ y
    return acc == 0, iterations


def main() -> int:
    a = bytes(range(64))
    b_same = bytes(range(64))
    b_diff_first = bytes([0xFF]) + a[1:]
    b_diff_last = a[:-1] + bytes([0xFF])

    same, it_same = ct_equal(a, b_same)
    diff_first, it_first = ct_equal(a, b_diff_first)
    diff_last, it_last = ct_equal(a, b_diff_last)

    print(f"same bytes:          equal={same}, iterations={it_same}")
    print(f"diff at byte 0:      equal={diff_first}, iterations={it_first}")
    print(f"diff at byte 63:     equal={diff_last}, iterations={it_last}")

    ok = True
    if it_same != it_first or it_same != it_last:
        ok = False
        print("FAIL: iteration count depends on the mismatch position")
    if it_same != len(a):
        ok = False
        print("FAIL: loop did not cover every byte")
    if not (diff_first is False and diff_last is False):
        ok = False
        print("FAIL: differing inputs must compare unequal")
    if not same:
        ok = False
        print("FAIL: identical inputs must compare equal")

    print("PASS — fixed work for mismatch at first byte vs last byte"
          if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
