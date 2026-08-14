#!/usr/bin/env python3
"""skill_lint.py — validate a SKILL.md against the authoring rules.

Checks:
  - frontmatter present with `name` and `description`
  - description <= 50 words
  - SKILL.md body <= 300 lines
  - required sections present
  - no absolute paths (e.g. /Users/... or C:\...)
Usage: python skill_lint.py <path-to-SKILL.md> [more ...]
Exit: 0 clean, 1 warnings, 2 errors.
"""
import re
import sys

REQUIRED_SECTIONS = [
    "When to use",
    "When not to use",
    "What the agent often gets wrong",
    "How to reason correctly",
    "What to verify",
    "How to verify",
    "Where the knowledge comes from",
    "Related skills",
    "Evaluation",
]
MAX_BODY_LINES = 300
MAX_DESC_WORDS = 50


def lint(path: str) -> tuple[list[str], list[str]]:
    warnings, errors = [], []
    try:
        text = open(path, encoding="utf-8").read()
    except OSError as e:
        return [], [f"cannot read {path}: {e}"]
    lines = text.splitlines()

    if not text.startswith("---"):
        errors.append("frontmatter missing: file must start with '---'")
        return warnings, errors
    fm_end = None
    for i, ln in enumerate(lines[1:], start=1):
        if ln == "---":
            fm_end = i
            break
    if fm_end is None:
        errors.append("frontmatter not closed (no second '---')")
        return warnings, errors
    fm = "\n".join(lines[1:fm_end])
    name = re.search(r"^name:\s*(.+)$", fm, re.M)
    desc = re.search(r"^description:\s*(.+)$", fm, re.M)
    if not name:
        errors.append("frontmatter missing 'name'")
    if not desc:
        errors.append("frontmatter missing 'description'")
    else:
        wc = len(desc.group(1).split())
        if wc > MAX_DESC_WORDS:
            warnings.append(f"description too long: {wc} words (max {MAX_DESC_WORDS})")

    body_lines = len(lines[fm_end:])
    if body_lines > MAX_BODY_LINES:
        warnings.append(f"SKILL.md too long: {body_lines} body lines (max {MAX_BODY_LINES})")

    for sec in REQUIRED_SECTIONS:
        if not re.search(rf"^#+\s*{re.escape(sec)}", text, re.M):
            warnings.append(f"missing section: '{sec}'")

    for m in re.finditer(r"(?:/Users/|C:\\|\\\\)", text):
        warnings.append(f"absolute path at line {text[:m.start()].count(chr(10)) + 1}")
        break
    return warnings, errors


def main() -> int:
    paths = sys.argv[1:]
    if not paths:
        print("usage: skill_lint.py <SKILL.md> [...]")
        return 2
    rc = 0
    for p in paths:
        warnings, errors = lint(p)
        for e in errors:
            print(f"ERROR  {p}: {e}")
        for w in warnings:
            print(f"WARN   {p}: {w}")
        if errors:
            rc = 2
        elif warnings and rc == 0:
            rc = 1
        print(f"{p}: {'OK' if not errors and not warnings else f'{len(warnings)} warnings, {len(errors)} errors'}")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
