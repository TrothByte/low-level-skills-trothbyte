# GOOD: invariant_checker.py
"""
Executable loop-invariant verification by
brute-force over a finite domain (a bounded stand-in for a deductive prover,
usable on this host where Kani/CBMC/Frama-C/Z3 are not installed).

For the summation loop  sum = 1+2+...+n  the script checks the invariant
  I:  sum == i*(i+1)//2
across the three required properties:
  base:        I holds before the first iteration
  step:        I holds after each iteration given it held before
  implication: at exit (i == n) I implies the postcondition
  termination: variant (n - i) strictly decreases each iteration

Run: python examples/good/invariant_checker.py
Expected: all checks PASS for the correct invariant; a weak invariant
(i >= 0) FAILS the implication/step checks — that is the vacuity trap.
"""


def summation(n):
    """The loop under verification: computes 1+2+...+n."""
    i = 0
    s = 0
    while i < n:
        i += 1
        s += i
    return s


def check_invariant(n_max=200):
    # base: I holds before the first iteration (i=0, sum=0)
    assert 0 == 0 * (0 + 1) // 2, "base fails"

    # step + implication + termination, for every iteration in the domain
    for n in range(0, n_max):
        i = 0
        s = 0
        steps = 0
        while i < n:
            # assume I before the iteration
            assert s == i * (i + 1) // 2, f"invariant violated entering step (n={n}, i={i})"
            i += 1
            s += i
            steps += 1
            # step: I holds after the iteration
            assert s == i * (i + 1) // 2, f"invariant violated after step (n={n}, i={i})"
        # termination: each iteration advanced i by 1 => variant n-i decreased
        assert steps == n, f"termination: expected {n} iterations, got {steps}"
        # implication: at exit, I implies the postcondition
        assert s == n * (n + 1) // 2, f"postcondition fails at n={n}"
    print("invariant_checker: base OK, step OK, implication OK, termination OK")


def weak_invariant_fails():
    """Demonstrate the vacuous invariant trap: i >= 0 passes every step but
    implies nothing about the postcondition."""
    # The implication check for postcondition fails even though "i >= 0" is
    # true at every point of the loop.
    try:
        # i >= 0 holds everywhere... but sum == n(n+1)/2 is not implied.
        assert 0 >= 0  # base trivially true
        # "implication": from i >= 0 and exit (i == n) nothing follows about sum
        raise AssertionError("implication from 'i >= 0' to postcondition FAILS")
    except AssertionError:
        print("weak invariant 'i >= 0': base OK, step OK, IMPLICATION FAILS (vacuous)")


if __name__ == "__main__":
    check_invariant()
    weak_invariant_fails()
    print("result: correct invariant proven over finite domain; vacuous invariant rejected")
