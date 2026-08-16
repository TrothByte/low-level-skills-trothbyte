# BAD: no_negative_cases.py
# intentionally incorrect
"""
This eval contains only positive fixtures. All of them pass, so the report
says "eval passed: 4/4". But there are no negative fixtures and no ambiguous
cases, so:
  - recall is undefined (nothing can be missed),
  - FP-rate is 0 by construction (nothing can be falsely flagged),
  - the eval cannot distinguish a working skill from a broken one.
A positive-only eval is a smoke test, not an eval.
"""
# intentionally incorrect


def clamp_ok(x):
    return max(0, min(100, x))


def broken_clamp(x):
    return x  # defect: never clamps


POSITIVE_ONLY = [
    (0, 0),
    (42, 42),
    (50, 50),
    (100, 100),
]


def run():
    # Only happy-path inputs; the broken implementation would pass these too.
    failed = 0
    for inp, expected in POSITIVE_ONLY:
        got = broken_clamp(inp)
        if got != expected:
            failed += 1
    print(f"eval passed: {len(POSITIVE_ONLY) - failed}/{len(POSITIVE_ONLY)}")
    # Misleading: broken_clamp passes every fixture, so "4/4" is reported
    # even though the skill's core guarantee (clamping) is completely broken.


if __name__ == "__main__":
    run()
