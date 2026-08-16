#!/usr/bin/env python3
"""Spacing-scale and grid checker for design-layout-spacing-grid.

Parses a CSS file (resolving :root custom properties) and checks:
  1. spacing properties (margin/padding/gap/inset/top/right/bottom/left and
     :root spacing variables) use only 4/8pt scale values (multiples of 4);
  2. grid containers use a 12-column-system grid (repeat(N) where N is a
     factor of 12) and consistent gutters (column-gap == row-gap).

Exit 0 = clean, 1 = issues.
"""
import re
import sys

SPACE_PROPS = ("margin", "margin-top", "margin-right", "margin-bottom",
               "margin-left", "padding", "padding-top", "padding-right",
               "padding-bottom", "padding-left", "gap", "column-gap",
               "row-gap", "inset", "top", "right", "bottom", "left")
DECL_RE = re.compile(r"([\w-]+)\s*:\s*([^;]+);")


def strip_comments(css):
    return re.sub(r"/\*.*?\*/", "", css, flags=re.S)


def parse_root(css):
    vars_ = {}
    for m in re.finditer(r":root\s*\{(.*?)\}", css, re.S):
        for d in DECL_RE.finditer(m.group(1)):
            if d.group(1).startswith("--"):
                vars_[d.group(1)] = d.group(2).strip()
    return vars_


def resolve(value, vars_):
    while "var(" in value:
        def repl(m):
            return vars_.get(m.group(1), "")
        new = re.sub(r"var\((--[\w-]+)\)", repl, value)
        if new == value:
            break
        value = new
    return value


def px_num(value):
    m = re.match(r"^([\d.]+)px$", value)
    return float(m.group(1)) if m else None


def parse_rules(css):
    rules = []
    for m in re.finditer(r"([^{}]+)\{([^{}]*)\}", css, re.S):
        sel = m.group(1).strip()
        if sel == ":root":
            continue
        decls = {d.group(1).strip(): d.group(2).strip() for d in DECL_RE.finditer(m.group(2))}
        rules.append((sel, decls))
    return rules


def main():
    if len(sys.argv) != 2:
        print("usage: spacing_scale_check.py <layout.css>")
        return 2
    path = sys.argv[1]
    css = strip_comments(open(path, encoding="utf-8").read())
    vars_ = parse_root(css)
    rules = parse_rules(css)
    issues = []

    for name, val in vars_.items():
        if name.startswith("--font") or name.startswith("--line"):
            continue
        v = px_num(val)
        if v is not None and v % 4 != 0:
            issues.append(f":root {name}: {v:g}px not on 4/8pt scale")

    for sel, decls in rules:
        for prop, val in decls.items():
            if prop not in SPACE_PROPS:
                continue
            v = px_num(resolve(val, vars_).strip())
            if v is not None and v % 4 != 0:
                issues.append(f"{sel}: {prop}: {v:g}px not on 4/8pt scale")
        if decls.get("display", "").strip() == "grid":
            gtc = decls.get("grid-template-columns")
            if gtc:
                m = re.search(r"repeat\(\s*(\d+)", gtc)
                if m and 12 % int(m.group(1)) != 0:
                    issues.append(f"{sel}: grid-template-columns repeat({int(m.group(1))}) — not a 12-column system factor")
            c = resolve(decls.get("column-gap", ""), vars_).strip() or None
            r = resolve(decls.get("row-gap", ""), vars_).strip() or None
            if c and r and c != r:
                issues.append(f"{sel}: inconsistent gutters (column-gap {c} vs row-gap {r})")

    if issues:
        for i in issues:
            print(f"ISSUE {path}: {i}")
        print(f"{path}: {len(issues)} issue(s) found")
        return 1
    print(f"{path}: spacing on 4/8pt scale, 12-col grid, consistent gutters")
    return 0


if __name__ == "__main__":
    sys.exit(main())
