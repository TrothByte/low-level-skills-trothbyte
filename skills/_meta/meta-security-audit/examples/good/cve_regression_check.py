# GOOD: cve_regression_check.py
"""
The CVE regression pair check: a claim
"this fix closes CVE-XXXX" is verified by building BOTH variants and
requiring (vulnerable FAILS, fixed PASSES). Models the ablation-delta rule
for security claims.

Run: python examples/good/cve_regression_check.py
Exit: 0 if the pair proves detection, 1 otherwise.
"""


def vulnerable_target(x):
    """Simulated vulnerable code: integer overflow before alloc (CVE-2016-8617
    class). For x > 1000 the multiplication wraps and passes a too-small size."""
    return (x * 4) & 0xFFFF  # 16-bit wrap


def fixed_target(x):
    """Fixed variant: explicit guard. Returns -1 on overflow."""
    if x > 16383:
        return -1
    return x * 4


def build_and_run(name, fn):
    """Simulated 'compile + run under sanitizer'. In the real audit this is
    gcc + UBSan + recorded exit code."""
    results = {}
    for inp in (10, 50000):  # 50000 is the gate input (overflow case)
        results[inp] = fn(inp)
    return results


def main():
    # The gate: a detectable overflow (x=50000 wraps in vulnerable,
    # guarded in fixed: 50000*4 = 200000 > 65535, wraps to 200000-3*65536).
    gate_input = 50000
    vuln = build_and_run("vulnerable", vulnerable_target)
    fixed = build_and_run("fixed", fixed_target)

    vuln_wrapped = vuln[gate_input] != gate_input * 4
    fixed_guarded = fixed[gate_input] == -1

    print(f"vulnerable build: x=5000 -> {vuln[gate_input]} (wrap: {vuln_wrapped})")
    print(f"fixed build:      x=5000 -> {fixed[gate_input]} (guarded: {fixed_guarded})")

    if vuln_wrapped and fixed_guarded:
        print("CVE regression pair: vulnerable FAILS, fixed PASSES -> claim VERIFIED")
        return 0
    print("CVE regression pair: detection NOT demonstrated -> claim UNVERIFIED")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
