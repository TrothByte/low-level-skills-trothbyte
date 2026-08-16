# GOOD: license_audit.py
"""
Per-artifact license compatibility check for a
repository audit. Each referenced artifact is checked against the repo policy
(MIT here) and the artifact's own license; non-commercial and copyleft
restrictions are flagged. Evidence is the license id + reason, not a badge.

Run: python examples/good/license_audit.py
Exit: 0 if policy-compatible, 1 if any incompatibility found.
"""

POLICY = "MIT"  # this repository's own license

# artifact: (name, license-id, is_noncommercial, is_copyleft)
ARTIFACTS = [
    ("trailofbits/something", "MIT", False, False),
    ("vendor/firmware-blog", "CC-BY-NC-4.0", True, False),
    ("gnu/toolchain", "GPL-3.0", False, True),
    ("trothbyte/low-level-skills", "MIT", False, False),
    ("arm/cmsis", "Apache-2.0", False, False),
]


def compatible(name, lic, noncommercial, copyleft):
    if noncommercial:
        return False, f"{lic} is non-commercial — cannot be redistributed/used freely"
    if copyleft and POLICY == "MIT":
        return False, f"{lic} copyleft is incompatible with {POLICY} policy"
    if lic not in {"MIT", "Apache-2.0", "BSD-2-Clause", "BSD-3-Clause"}:
        return False, f"{lic} not in allowlist"
    return True, "ok"


def main():
    any_bad = False
    for name, lic, nc, cl in ARTIFACTS:
        ok, reason = compatible(name, lic, nc, cl)
        print(f"{'OK  ' if ok else 'FAIL'} {name}: {reason}")
        any_bad |= not ok
    if any_bad:
        print("audit: incompatibilities found — report, do not hide")
        return 1
    print("audit: all artifacts policy-compatible")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
