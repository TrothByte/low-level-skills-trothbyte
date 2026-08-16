#!/usr/bin/env python3
"""Uniqueness self-test for design-anti-ai-look-originality-review.

Scores a page+CSS pair on brand grounding:
  - the brand name (from <title>) appears in the body copy;
  - at least two distinct colors are NOT in the generic AI palette;
  - the leading font family is a custom family (not a default stack);
  - fewer than 2 AI-look fingerprint families (reuses the fingerprint logic).

Verdict "differentiated" (exit 0) or "generic" (exit 1).
"""
import re
import sys

from ai_look_fingerprint import (DEFAULT_FONTS, PURPLE, GRADIENT, CREAM,
                                 TERRACOTTA, SERIF_FACE, NEAR_BLACK, ACID_GREEN,
                                 HAIRLINE, SPACE_GROTESK, NUM_LABEL, hit_lines)

GENERIC_COLORS = {
    "#FFFFFF", "#FFFFFF", "#000000", "#111827", "#0F172A", "#0D0D0D",
    "#F9FAFB", "#F3F4F6", "#E5E7EB", "#D1D5DB", "#9CA3AF", "#6B7280",
    "#4B5563", "#374151", "#1F2937", "#2563EB", "#3B82F6", "#60A5FA",
    "#4F46E5", "#6366F1", "#7C3AED", "#8B5CF6", "#A855F7", "#6D28D9",
    "#F4F1EA", "#E07A5F", "#E76F51", "#A3E635", "#22C55E", "#84CC16",
    "#10B981", "#14B8A6", "#F59E0B", "#F97316", "#EF4444", "#EC4899",
}


def main():
    if len(sys.argv) != 3:
        print("usage: uniqueness_self_test.py <page.html> <style.css>")
        return 2
    html_path, css_path = sys.argv[1], sys.argv[2]
    html = open(html_path, encoding="utf-8").read()
    css = open(css_path, encoding="utf-8").read()
    html = re.sub(r"<!--.*?-->", "", html, flags=re.S)
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.S)
    all_text = html + "\n" + css
    checks = []

    title_m = re.search(r"<title[^>]*>(.*?)</title>", html, re.S | re.I)
    brand = None
    if title_m:
        brand = re.split(r"[\s—|-]", title_m.group(1).strip())[0].strip()
    body = re.sub(r"<[^>]+>", " ", html)
    if brand and re.search(re.escape(brand), body, re.I):
        checks.append(f"brand grounding: '{brand}' appears in body copy")
    else:
        checks.append("brand grounding: FAIL — brand name not in body copy")

    hexes = set(re.findall(r"#[0-9a-fA-F]{6}\b", all_text))
    custom = hexes - GENERIC_COLORS
    if len(custom) >= 2:
        checks.append(f"custom palette: {len(custom)} colors outside the generic AI palette ({sorted(custom)[:4]}...)")
    else:
        checks.append("custom palette: FAIL — colors are all from the generic AI palette")

    fam_hits = []
    for ln in hit_lines(css, re.compile(r"font-family\s*:\s*([^;]+)", re.I)):
        m = re.search(r"font-family\s*:\s*([^;]+)", css.splitlines()[ln - 1], re.I)
        if m:
            first = m.group(1).split(",")[0].strip().strip("\"'").lower()
            if first not in DEFAULT_FONTS and first not in ("sans-serif", "serif"):
                fam_hits.append(first)
    if fam_hits:
        checks.append(f"custom type: {fam_hits[0]!r} leads the stack")
    else:
        checks.append("custom type: FAIL — no custom family leads the stacks")

    fingerprints = []
    if hit_lines(all_text, PURPLE):
        fingerprints.append("purple")
    if hit_lines(all_text, CREAM) and hit_lines(all_text, TERRACOTTA):
        fingerprints.append("cream-terracotta")
    if hit_lines(all_text, NEAR_BLACK) and hit_lines(all_text, ACID_GREEN):
        fingerprints.append("dark-acid")
    if hit_lines(all_text, HAIRLINE):
        fingerprints.append("hairlines")
    if hit_lines(all_text, SPACE_GROTESK):
        fingerprints.append("space-grotesk")
    if hit_lines(all_text, NUM_LABEL):
        fingerprints.append("numbered-labels")
    if len(fingerprints) >= 2:
        checks.append(f"fingerprint count: FAIL — {len(fingerprints)} AI-look families")

    fails = [c for c in checks if "FAIL" in c]
    for c in checks:
        print(f"CHECK {c}")
    if fails:
        print(f"{html_path}: GENERIC — {len(fails)} brand-grounding check(s) failed")
        return 1
    print(f"{html_path}: DIFFERENTIATED — brand-grounded, custom palette and type")
    return 0


if __name__ == "__main__":
    sys.exit(main())
