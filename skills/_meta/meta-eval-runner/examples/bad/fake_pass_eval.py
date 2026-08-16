# BAD: fake_pass_eval.py
# intentionally incorrect
"""
This "eval" always reports PASS regardless of the target behavior. It has no
fixtures, no gate, and no verdict computation: the result is hardcoded.
An eval that cannot fail is not an eval — it is a report generator.

Run: python examples/bad/fake_pass_eval.py
Expected (if this were honest): FAIL. The skill's scorer must flag it.
"""
# intentionally incorrect

import sys


def run_eval():
    # No fixtures, no target, no gate — the verdict is fabricated.
    # The classic failure mode: agent writes eval, runs it once, hardcodes
    # the observed output as the "expected" result, and ships it.
    print("Running eval for skill clamp-safety...")
    print("ALL 17 FIXTURES PASS")  # fabricated; nothing was executed
    return 0


def main():
    code = run_eval()
    sys.exit(code)


if __name__ == "__main__":
    main()
