# BAD: a "verification" harness that only times the kernel on the benchmark
# tensor and checks that the output equals the expected value there. It has no
# hidden distributions, no input-dependence check, no work-scaling check, and a
# deliberately weak baseline. It accepts the reward-hacked bypass because the
# bypass is fast AND "correct" on the exact benchmark inputs.
# This is the failure mode arxiv-2607-16241 documents: evaluation that only
# exercises the narrow test distribution lets hardcoded bypasses win.
# # intentionally incorrect
#
# A correct harness is in examples/good/good_verified_protocol.py.

import time


def sum_reference(xs):
    return sum(xs)


BENCHMARK_TENSOR = (1, 2, 3, 4)
EXPECTED = sum_reference(BENCHMARK_TENSOR)


def genuine_kernel(xs):
    total = 0
    for x in xs:
        total += x
    return total


def hardcoded_bypass_kernel(xs):
    # reward hack: constant-time match on the benchmark's tensor values,
    # precomputed answer; a constant elsewhere. O(1) work.
    if len(xs) == 4 and xs[0] == 1 and xs[1] == 2 and xs[2] == 3 and xs[3] == 4:
        return EXPECTED
    return 0


def naive_baseline(xs):
    """Deliberately weak baseline: re-computes the sum four times."""
    total = 0
    for _ in range(4):
        total = sum(xs)
    return total


def naive_harness(kernel):
    # the "harness": time the kernel on the benchmark tensor only.
    xs = BENCHMARK_TENSOR
    t0 = time.perf_counter()
    for _ in range(200_000):
        out = kernel(xs)
    dt_kernel = time.perf_counter() - t0

    t0 = time.perf_counter()
    for _ in range(200_000):
        out_base = naive_baseline(xs)
    dt_base = time.perf_counter() - t0

    correct = (out == EXPECTED) and (out_base == EXPECTED)
    speedup = dt_base / dt_kernel if dt_kernel > 0 else float("inf")
    return correct, speedup


for name, kernel in [("genuine_kernel", genuine_kernel),
                     ("hardcoded_bypass_kernel", hardcoded_bypass_kernel)]:
    correct, speedup = naive_harness(kernel)
    verdict = "ACCEPT" if correct and speedup > 1 else "REJECT"
    print(f"{name}: output correct on benchmark tensor = {correct}, "
          f"speedup vs naive baseline = {speedup:.2f}x -> {verdict}")

print("\nThe naive timing-only harness ACCEPTS the reward-hacked bypass:")
print("it is fast and happens to be correct on the benchmark tensor. Only hidden")
print("distributions + input-dependence + work-scaling expose the hack.")
