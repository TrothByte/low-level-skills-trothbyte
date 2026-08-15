#!/usr/bin/env python3
"""validate.py — run every repository validator in a single command.

Usage:
    python tools/validate.py [repo-root]

Runs, in order:
    1. skill_lint.py     — structure of every skills/**/SKILL.md
    2. registry_check.py — registry/*.yaml integrity (ids, references, paths)
    3. source_check.py   — provenance: every claim and SOURCE line traces to registry/sources.yaml

Exit code 0 if all checks pass, non-zero otherwise. Intended to be the single
quality gate for contributors, pre-commit hooks, and CI.
"""
import glob
import os
import subprocess
import sys

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), ".."))


def run(command: list[str]) -> int:
    return subprocess.run(command, cwd=ROOT).returncode


def main() -> int:
    lint = os.path.join(ROOT, "tools", "lint", "skill_lint.py")
    registry = os.path.join(ROOT, "tools", "lint", "registry_check.py")
    source = os.path.join(ROOT, "tools", "source", "source_check.py")

    skill_mds = glob.glob(os.path.join(ROOT, "skills", "**", "SKILL.md"), recursive=True)
    print(f"[validate] checking {len(skill_mds)} SKILL.md files")

    rc = 0
    rc |= run([sys.executable, lint] + skill_mds)
    rc |= run([sys.executable, registry])
    rc |= run([sys.executable, source])

    if rc:
        print("[validate] FAILED")
        return 1
    print("[validate] OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
