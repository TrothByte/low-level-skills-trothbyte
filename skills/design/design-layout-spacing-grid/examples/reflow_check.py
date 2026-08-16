#!/usr/bin/env python3
"""Reflow checker (WCAG 1.4.10) for design-layout-spacing-grid.

Static heuristics that a layout can reflow to 320px width without 2D
scrolling: a @media query exists; no width/min-width > 320px; no
overflow-x:hidden on body/html (masks the problem); no fixed-px grid tracks.
Exit 0 = clean, 1 = issues. (Rendered verification is browser-based; these
are deterministic smoke checks.)
"""
import re
import sys

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


def main():
    if len(sys.argv) != 2:
        print("usage: reflow_check.py <layout.css>")
        return 2
    path = sys.argv[1]
    css = strip_comments(open(path, encoding="utf-8").read())
    vars_ = parse_root(css)
    issues = []

    if "@media" not in css:
        issues.append("no @media queries — layout cannot reflow at 320px (WCAG 1.4.10)")

    for m in re.finditer(r"([^{}]+)\{([^{}]*)\}", css, re.S):
        sel = m.group(1).strip()
        if sel == ":root":
            continue
        decls = {d.group(1).strip(): d.group(2).strip() for d in DECL_RE.finditer(m.group(2))}
        for prop in ("width", "min-width"):
            v = decls.get(prop)
            if not v:
                continue
            val = resolve(v, vars_).strip()
            mm = re.match(r"^([\d.]+)px$", val)
            if mm and float(mm.group(1)) > 320:
                issues.append(f"{sel}: {prop}: {val} > 320px — breaks reflow at 320px")
        if sel in ("body", "html") and decls.get("overflow-x", "").strip() == "hidden":
            issues.append(f"{sel}: overflow-x:hidden masks reflow problems instead of fixing them")
        gtc = decls.get("grid-template-columns")
        if gtc and re.search(r"-?\d+(\.\d+)?px", gtc):
            issues.append(f"{sel}: grid-template-columns uses fixed px tracks — use fr/% units")

    if issues:
        for i in issues:
            print(f"ISSUE {path}: {i}")
        print(f"{path}: {len(issues)} reflow issue(s) found")
        return 1
    print(f"{path}: reflow at 320px OK (media query present, no fixed widths)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
