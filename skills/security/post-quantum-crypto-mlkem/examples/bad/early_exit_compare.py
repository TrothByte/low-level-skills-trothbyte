"""memcmp-style early-exit comparison — BAD model.

Exiting at the first differing byte makes the iteration count (execution
time) depend on the mismatch position. On secret data this leaks the first
difference position — exactly the leak that must not exist in ML-KEM/ML-DSA
paths that compare secrets.

The branch analyzer flags the early-exit loop: iteration counts differ for
mismatch at byte 0 vs byte 63.

Run:  python examples/bad/early_exit_compare.py
Expect:  FLAG — early-exit compare (variable work leaks mismatch position)
"""
import inspect


def memcmp_style(a: bytes, b: bytes) -> tuple:
    """Early-exit compare (BAD): returns False at the first difference."""
    iterations = 0
    for x, y in zip(a, b):
        iterations += 1
        if x != y:                      # early exit leaks mismatch position
            return False, iterations
    return True, iterations


def branch_analyzer():
    """Flag the early-exit compare."""
    flags = []

    source = inspect.getsource(memcmp_style)
    if "if x != y:" in source and "return False, iterations" in source:
        flags.append("source: loop returns at the first differing byte "
                     "(`if x != y: return False`)")

    a = bytes(range(64))
    b_diff_first = bytes([0xFF]) + a[1:]
    b_diff_last = a[:-1] + bytes([0xFF])

    _, it_first = memcmp_style(a, b_diff_first)
    _, it_last = memcmp_style(a, b_diff_last)
    print(f"diff at byte 0:  iterations={it_first}")
    print(f"diff at byte 63: iterations={it_last}")

    if it_first != it_last:
        flags.append(f"behavior: iteration count depends on mismatch "
                     f"position ({it_first} vs {it_last}) — timing leak")
    return flags


def main() -> int:
    flags = branch_analyzer()
    if flags:
        for f in flags:
            print(f"FLAG: {f}")
        print("FLAG — early-exit compare: variable work leaks the mismatch "
              "position")
        return 1
    print("no flags (unexpected for this fixture)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
