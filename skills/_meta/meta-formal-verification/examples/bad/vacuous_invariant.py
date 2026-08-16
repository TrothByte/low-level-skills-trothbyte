# BAD: vacuous_invariant.py
# intentionally incorrect
"""
Claims the loop is "formally verified" using a vacuous invariant that any
verifier accepts but that proves nothing about the postcondition. The
invariant `i >= 0` is true at every point (base and step trivially pass) yet
implies nothing about `sum`. The script then reports VERIFIED anyway — the
classic vacuous-proof failure (LiveFMBench: vacuous/wrong invariants).
"""
# intentionally incorrect


def verify_sum_with_vacuous_invariant():
    n = 100
    i = 0
    s = 0
    # "invariant": i >= 0  — vacuously true, carries no information.
    # A real verifier will accept the inductive step (it holds trivially),
    # so this "proof" passes while proving nothing.
    assert i >= 0  # base: trivially true
    while i < n:
        i += 1
        s += i
        assert i >= 0  # step: trivially true — NOT the real invariant
    # The postcondition s == n*(n+1)//2 is never stated nor implied.
    assert i >= 0  # "exit" condition is meaningless
    print(f"sum computed = {s}")
    print("FORMALLY VERIFIED: loop safe and correct")  # fabricated: no
    # postcondition, no implication check, no tool run


if __name__ == "__main__":
    verify_sum_with_vacuous_invariant()
