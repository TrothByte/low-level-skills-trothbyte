#!/usr/bin/env python3
"""Type-scale / text-spacing checker for design-typography-hierarchy.

Reads a CSS file (resolving :root custom properties and var()), then checks:
  1. distinct font sizes form a deliberate modular scale (consecutive ratios
     match a known step: 1.125/1.2/1.25/1.333, or a >= 1.9 display jump), or
     a clamp() fluid role is used;
  2. fewer than 7 distinct sizes (AI-slop ramps are long and arbitrary);
  3. weight contrast: light-only (100-300) or uniform (all 400) ramps are
     flagged; headings must not equal body weight with close sizes;
  4. body text line-height >= 1.5 (WCAG 1.4.8 / 1.4.12), headings exempt.

Exit 0 = clean, 1 = issues.
"""
import re
import sys

RATIOS = (1.125, 1.2, 1.25, 1.333)
HEADING_SEL = re.compile(r"h[1-6]\b|\.display|\.hero")


def strip_comments(css):
    return re.sub(r"/\*.*?\*/", "", css, flags=re.S)


def parse_root(css):
    vars_ = {}
    for m in re.finditer(r":root\s*\{(.*?)\}", css, re.S):
        for d in re.finditer(r"--([\w-]+)\s*:\s*([^;]+);", m.group(1)):
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


def to_px(value, vars_, base=16):
    value = resolve(value, vars_).strip()
    m = re.match(r"^([\d.]+)px$", value)
    if m:
        return float(m.group(1))
    m = re.match(r"^([\d.]+)(rem|em)$", value)
    if m:
        return float(m.group(1)) * base
    if "clamp(" in value:
        return None
    return None


def to_line(value, vars_):
    value = resolve(value, vars_).strip()
    m = re.match(r"^([\d.]+)$", value)
    if m:
        return float(m.group(1))
    m = re.match(r"^([\d.]+)px$", value)
    if m:
        return float(m.group(1)) / 16.0
    return None


def parse_rules(css):
    rules = []
    for m in re.finditer(r"([^{}]+)\{([^{}]*)\}", css, re.S):
        sel = m.group(1).strip()
        if sel == ":root":
            continue
        decls = dict((d.group(1).strip(), d.group(2).strip())
                     for d in re.finditer(r"([\w-]+)\s*:\s*([^;]+);", m.group(2)))
        rules.append((sel, decls))
    return rules


def main():
    if len(sys.argv) != 2:
        print("usage: type_scale_check.py <style.css>")
        return 2
    path = sys.argv[1]
    css = strip_comments(open(path, encoding="utf-8").read())
    vars_ = parse_root(css)
    rules = parse_rules(css)
    issues = []

    sizes = []
    for sel, decls in rules:
        fs = decls.get("font-size")
        if not fs:
            continue
        px = to_px(resolve(fs, vars_), vars_)
        if px is not None:
            sizes.append((sel, px))

    distinct = sorted(set(px for _, px in sizes))
    if len(distinct) >= 7:
        issues.append(f"{len(distinct)} distinct font sizes ({[f'{s:g}px' for s in distinct]}) — not a deliberate scale")
    if len(distinct) >= 2:
        for a, b in zip(distinct, distinct[1:]):
            ratio = b / a
            if not any(abs(ratio - r) / r < 0.02 for r in RATIOS) and ratio < 1.9:
                issues.append(f"size gap {a:g}px -> {b:g}px (ratio {ratio:.3f}) matches no modular step")

    # weight contrast
    weights = []
    heading_weights = []
    for sel, decls in rules:
        fw = decls.get("font-weight")
        if not fw:
            continue
        try:
            w = int(resolve(fw, vars_))
        except ValueError:
            continue
        weights.append((sel, w))
        if HEADING_SEL.search(sel):
            heading_weights.append(w)
    if heading_weights and all(w <= 300 for w in heading_weights) and not any(w >= 700 for _, w in weights):
        issues.append("all heading weights are light (<= 300) with no 700+ anywhere — no weight contrast")
    if heading_weights and all(400 <= w <= 500 for w in heading_weights):
        issues.append("headings use body weight (400-500) — hierarchy relies on size alone")

    # line-height >= 1.5 on body text
    for sel, decls in rules:
        lh = decls.get("line-height")
        if not lh or HEADING_SEL.search(sel):
            continue
        v = to_line(resolve(lh, vars_), vars_)
        if v is not None and v < 1.5:
            issues.append(f"{sel}: line-height {v:g} < 1.5 (WCAG 1.4.8/1.4.12)")

    if issues:
        for i in issues:
            print(f"ISSUE {path}: {i}")
        print(f"{path}: {len(issues)} issue(s) found")
        return 1
    print(f"{path}: type scale deliberate, weight contrast present, text spacing OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
