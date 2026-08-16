#!/usr/bin/env python3
"""Font-stack checker for design-typography-hierarchy.

Flags default/AI-slop font stacks: Inter, Roboto, system-ui, -apple-system,
BlinkMacSystemFont, "Segoe UI". A stack whose FIRST font is a custom/brand
family is fine; a stack that opens with a default is flagged. Exit 0 clean.
"""
import re
import sys

DEFAULTS = {"inter", "roboto", "system-ui", "-apple-system",
            "blinkmacsystemfont", "segoe ui"}

FAM_RE = re.compile(r"font-family\s*:\s*([^;]+);", re.I)


def main():
    if len(sys.argv) != 2:
        print("usage: font_stack_check.py <style.css>")
        return 2
    path = sys.argv[1]
    css = re.sub(r"/\*.*?\*/", "", open(path, encoding="utf-8").read(), flags=re.S)
    issues = []
    for m in FAM_RE.finditer(css):
        stack = [f.strip().strip('"').strip("'").lower()
                 for f in m.group(1).split(",")]
        stack = [f for f in stack if f and f != "sans-serif" and f != "serif"]
        if not stack:
            continue
        if stack[0] in DEFAULTS:
            issues.append(f"default font stack starts with {stack[0]!r}: {', '.join(stack)}")

    if issues:
        for i in issues:
            print(f"ISSUE {path}: {i}")
        print(f"{path}: {len(issues)} default-stack issue(s) found")
        return 1
    print(f"{path}: no default font stacks — brand/custom family leads")
    return 0


if __name__ == "__main__":
    sys.exit(main())
