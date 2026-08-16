#!/usr/bin/env python3
"""Heading-level checker for design-typography-hierarchy.

Parses heading tags in an HTML file and verifies: exactly one <h1>; heading
levels never skip (increase by at most 1); the document starts with an <h1>.
Exit 0 = clean, 1 = issues.
"""
import re
import sys

H_RE = re.compile(r"<h([1-6])\b[^>]*>(.*?)</h\1>", re.S | re.I)


def main():
    if len(sys.argv) != 2:
        print("usage: heading_level_check.py <index.html>")
        return 2
    path = sys.argv[1]
    html = open(path, encoding="utf-8").read()
    heads = [(int(m.group(1)), m.group(2)[:40]) for m in H_RE.finditer(html)]
    issues = []

    h1s = [h for h, _ in heads if h == 1]
    if len(h1s) > 1:
        issues.append(f"{len(h1s)} <h1> elements — exactly one is required")
    if heads and heads[0][0] != 1:
        issues.append(f"document starts with <h{heads[0][0]}> — must start with <h1>")
    prev = heads[0][0] if heads else 0
    for level, txt in heads[1:]:
        if level > prev + 1:
            issues.append(f"heading level skipped: <h{prev}> -> <h{level}> ({txt!r})")
        prev = level

    if issues:
        for i in issues:
            print(f"ISSUE {path}: {i}")
        print(f"{path}: {len(issues)} heading issue(s) found")
        return 1
    n = len(heads)
    print(f"{path}: heading structure OK ({n} heading(s), exactly one h1, no skips)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
