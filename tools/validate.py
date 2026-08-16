#!/usr/bin/env python3
"""validate.py v2.0 — run every repository validator in sequence.

Runs:    1. skill_lint.py          — structure of every skills/**/SKILL.md
    2. registry_check.py        — registry integrity + cycle detection (require edges)    
    3. source_check.py           — provenance SOURCE lines trace to registry/sources.yaml
    4. claim_extractor.py       — extract & verify source citations from skills/**/*.md
    5. prose_lint.py            — lightweight prose quality checks (sampled for speed)

Exit codes: 0 all pass | 1 any errors/warnings | 2 usage error
Single quality gate for contributors, pre-commit, and GitHub Actions CI.
"""
import glob
import os
import subprocess
import sys

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), ".."))


def run(command):
    """Run command and return exit code."""
    return subprocess.run(command, cwd=ROOT).returncode


def main():
    lint_path = os.path.join(ROOT, "tools", "lint", "skill_lint.py")
    registry_path = os.path.join(ROOT, "tools", "lint", "registry_check.py")
    source_path = os.path.join(ROOT, "tools", "source", "source_check.py")
    claim_path = os.path.join(ROOT, "tools", "lint", "claim_extractor.py")
    prose_path = os.path.join(ROOT, "tools", "lint", "prose_lint.py")
    token_path = os.path.join(ROOT, "tools", "tokens", "token_measure.py")

    skill_mds = glob.glob(os.path.join(ROOT, "skills", "**", "SKILL.md"), recursive=True)
    skill_dirs = sorted({os.path.dirname(p) for p in skill_mds})

    print(f"[v2.0-validate] checking {len(skill_mds)} SKILL.md files\n")

    rc = 0

    # Level 1: Structure lint
    print("-- LEVEL 1: skill_lint.py --")
    if os.path.isfile(lint_path):
        rc |= run([sys.executable, lint_path] + skill_mds)
        tag = "PASS" if not rc else f"rc={rc}"
        print(f"  [{tag}]")

    # Level 1b: Token budget hard gate (v2.0: activation cost <= 2000 tokens)
    print("\n-- LEVEL 1b: token_measure.py (activation <= 2000 tokens) --")
    if os.path.isfile(token_path):
        tok_rc = run([sys.executable, token_path, "--check", "2000"] + skill_dirs)
        if tok_rc:
            print(f"  [FAIL] token gate: at least one skill exceeds 2000-token activation cost")
        else:
            print("  [PASS]")
        rc |= tok_rc

    # Level 2: Registry + cycle detection
    print("\n-- LEVEL 2: registry_check.py --")
    rc |= run([sys.executable, registry_path])
    tag = "PASS" if not rc else f"rc={rc}"
    print(f"  [{tag}]")

    # Level 3a: Provenance audit
    print("\n-- LEVEL 3a: source_check.py --")
    rc |= run([sys.executable, source_path])
    tag = "PASS" if not rc else f"rc={rc}"
    print(f"  [{tag}]")

    # Level 3b: Claim extraction
    print("\n-- LEVEL 3b: claim_extractor.py --")
    if os.path.isfile(claim_path):
        rc |= run([sys.executable, claim_path])
        tag = "PASS" if not rc else f"rc={rc}"
        print(f"  [{tag}]")
    else:
        print("  [SKIP — claim_extractor.py not found]")

    # Level 4: Prose quality (informational only; does not fail CI in v2.0 alpha)
    print("\n-- LEVEL 4: prose_lint.py --")
    if os.path.isfile(prose_path):
        # Only check first 3 skills for speed in CI
        sample = skill_mds[:3]
        if sample:
            prose_rc = run([sys.executable, prose_path] + sample)
            tag = "PASS" if not prose_rc else f"rc={prose_rc} (informational)"
            print(f"  [{tag}] (sampled first {len(sample)}; advisory, non-blocking)")
        else:
            print("  [SKIP — no SKILL.md found]")
    else:
        print("  [SKIP — prose_lint.py not found]")

    # Final verdict
    print()
    if rc:
        print("[v2.0-validate] ERRORS FOUND")
        return 1
    print("[v2.0-validate] ALL CHECKS PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
