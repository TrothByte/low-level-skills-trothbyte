#!/usr/bin/env python3
"""WCAG 2.2 contrast + target-size checker for design-color-contrast-wcag-a11y.

Reads a CSS fixture. Pairs are declared as comments:
    /* CONTRAST: #hexFg on #hexBg text|large|ui */
Thresholds per WCAG 2.2: normal text 4.5:1, large text 3:1, non-text/UI
graphics 3:1. Also checks WCAG 2.2 2.5.8 (Target Size Minimum 24x24 CSS px)
for interactive selectors (a/button/input/select/textarea/btn/link/tab):
explicit min-width/height or width/height must be >= 24px; if absent, a
padding-based estimate (font-size*1.2 + vertical padding) must reach 24px.
Exit 0 = clean, 1 = failures.
"""
import math
import re
import sys

THRESH = {"text": 4.5, "large": 3.0, "ui": 3.0}
PAIR_RE = re.compile(
    r"CONTRAST:\s*(#[0-9a-fA-F]{3,8})\s+on\s+(#[0-9a-fA-F]{3,8})\s+(text|large|ui)\b")
INT_SEL = re.compile(r"(a\b|button|input|select|textarea|btn|link|tab)", re.I)
DECL_RE = re.compile(r"([\w-]+)\s*:\s*([^;]+);")


def rgb_from_hex(h):
    h = h.lstrip("#")
    if len(h) == 3 or len(h) == 4:
        h = "".join(c * 2 for c in h)
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def luminance(rgb):
    def f(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (f(x) for x in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(fg, bg):
    l1, l2 = luminance(rgb_from_hex(fg)), luminance(rgb_from_hex(bg))
    hi, lo = max(l1, l2), min(l1, l2)
    return (hi + 0.05) / (lo + 0.05)


def parse_rules(css):
    rules = []
    for m in re.finditer(r"([^{}]+)\{([^{}]*)\}", css, re.S):
        sel = m.group(1).strip()
        decls = {d.group(1): d.group(2).strip() for d in DECL_RE.finditer(m.group(2))}
        rules.append((sel, decls))
    return rules


def main():
    if len(sys.argv) != 2:
        print("usage: contrast_check.py <palette.css>")
        return 2
    path = sys.argv[1]
    css = open(path, encoding="utf-8").read()
    issues = []

    for m in PAIR_RE.finditer(css):
        fg, bg, role = m.group(1), m.group(2), m.group(3)
        r = contrast(fg, bg)
        need = THRESH[role]
        status = "PASS" if r >= need else "FAIL"
        if r < need:
            issues.append(f"contrast {fg} on {bg} ({role}): {r:.2f}:1 < {need}:1")
        print(f"{status} {fg} on {bg} ({role}): {r:.2f}:1 (need {need}:1)")

    for sel, decls in parse_rules(css):
        if not INT_SEL.search(sel):
            continue

        def px_val(v):
            m = re.match(r"^([\d.]+)px$", v)
            return float(m.group(1)) if m else None

        w = 0.0
        h = 0.0
        for prop, acc in (("min-width", "w"), ("width", "w"),
                          ("min-height", "h"), ("height", "h")):
            v = px_val(decls.get(prop, "")) if decls.get(prop) else None
            if v:
                if acc == "w":
                    w = max(w, v)
                else:
                    h = max(h, v)
        if w >= 24 and h >= 24:
            print(f"OK   {sel}: target {w:g}x{h:g} px >= 24x24")
            continue

        pad = [None] * 4
        if decls.get("padding"):
            parts = [px_val(x) for x in decls["padding"].split()]
            parts = [p for p in parts if p is not None]
            if len(parts) == 1:
                pad = parts * 4
            elif len(parts) == 2:
                pad = [parts[0], parts[1], parts[0], parts[1]]
            elif len(parts) == 3:
                pad = [parts[0], parts[1], parts[2], parts[1]]
            elif len(parts) == 4:
                pad = parts
        for prop, idx in (("padding-top", 0), ("padding-right", 1),
                          ("padding-bottom", 2), ("padding-left", 3)):
            v = px_val(decls.get(prop, "")) if decls.get(prop) else None
            if v is not None:
                pad[idx] = v
        fs = px_val(decls.get("font-size", "")) if decls.get("font-size") else None
        if all(x is not None for x in pad) and fs:
            est_h = fs * 1.2 + pad[0] + pad[2]
            if est_h >= 24:
                print(f"OK   {sel}: padding-based target est {est_h:.0f}px tall")
                continue
        issues.append(f"target {sel}: interactive element under 24x24 CSS px "
                      f"(w={w:g}, h={h:g}) — WCAG 2.2 2.5.8")

    if issues:
        for i in issues:
            print(f"ISSUE {path}: {i}")
        print(f"{path}: {len(issues)} issue(s) found")
        return 1
    print(f"{path}: all contrast pairs and targets pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
