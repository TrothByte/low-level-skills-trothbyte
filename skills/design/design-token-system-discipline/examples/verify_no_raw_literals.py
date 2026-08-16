#!/usr/bin/env python3
"""Raw-literal scanner for design-token-system-discipline.

Scans a component CSS file and flags direct design literals that should be
token references: hex colors, rgb()/hsl() colors, and unit lengths (px/rem/
em/%) used in declarations. Comments, @-rules (media queries, font-face) and
lines already using var()/calc() are ignored. Exit 0 = clean, 1 = literals.
"""
import re
import sys

HEX = re.compile(r"#[0-9a-fA-F]{3,8}\b")
RGB = re.compile(r"rgba?\([^)]*\)")
HSL = re.compile(r"hsla?\([^)]*\)")
LEN = re.compile(r"(?<![\w-])-?\d+(\.\d+)?(px|rem|em|vh|vw|%)")


def main():
    if len(sys.argv) != 2:
        print("usage: verify_no_raw_literals.py <component.css>")
        return 2
    path = sys.argv[1]
    css = re.sub(r"/\*.*?\*/", "", open(path, encoding="utf-8").read(), flags=re.S)
    hits = []
    for i, line in enumerate(css.splitlines(), 1):
        s = line.strip()
        if not s or s.startswith("--") or "@" in s or "var(" in s or "calc(" in s:
            continue
        for pat, kind in ((HEX, "hex color"), (RGB, "rgb() color"), (HSL, "hsl() color")):
            for m in pat.finditer(line):
                hits.append((i, f"{kind} {m.group(0)}"))
        for m in LEN.finditer(line):
            hits.append((i, f"length {m.group(0)}"))

    if hits:
        for ln, what in hits:
            print(f"RAW {path}:{ln}: {what}")
        print(f"{path}: {len(hits)} raw literal(s) — use design tokens instead")
        return 1
    print(f"{path}: no raw literals — token-first CSS OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
